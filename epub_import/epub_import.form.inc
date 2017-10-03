<?php

/**
 * @file
 * Epub Import form.
 */

/**
 * Menu callback for 'admin/epub-import'
 */
function epub_import_import_form($form, $form_state) {
  if (!epub_import_is_configured()) {
    $form['need_config'] = array(
      '#markup' => l(t('Configure Epub import settings to start importing.'), 'admin/config/content/epub-import/settings'),
    );

    return $form;
  }

  $heading_level = variable_get('epub_import_header_level', 'h1');

  $instruction =<<<INSTR
    <p class="well">The epub import enables you to publish full documents or chapters that have been exported to epub format. If you are creating a new document, first create a Book and then use this page to upload the file(s) that will become the Book. If you are replacing documents or chapters, select the Book you intend to replace.</p>
INSTR;

  $form['instruction'] = array(
    '#markup' => $instruction,
  );

  $form['import'] = array(
    '#type' => 'file',
    '#title' => t('Upload file(s)'),
    '#name' => 'files[]',
    '#attributes' => array('multiple' => 'multiple'),
    '#description' => t('Hold down Ctrl+Click to select multiple files (⌘+Click on Mac).')
  );

  $options = epub_import_get_books();

  // Add a drop-down to select the destination book.
  $form['book_id'] = array(
    '#type' => 'select',
    '#title' => t('Book'),
    '#options' => $options,
    '#description' => t('Choose an existing book to or !create.',
      array('!create' => l('create a new book', 'node/add/book'))),
    '#required' => TRUE,
    '#ajax' => array(
      'event' => 'change',
      'wrapper' => 'book-children-wrapper',
      'callback' => 'epub_import_fill_book_children_callback',
      'method' => 'replace',
    ),
  );

  $form['epub_import_header_level'] = array(
    '#type' => 'select',
    '#title' => t('Header level'),
    '#description' => t('Determines the break point in the Epub file when creating each node.'),
    '#default_value' => variable_get('epub_import_header_level', 'h1'),
    '#options' => array(
      'h1' => 'H1',
      'h2' => 'H2',
      'h3' => 'H3',
    ),
  );

  $form['publish_options'] = array(
    '#type' => 'radios',
    '#title' => t('Publishing options'),
    '#default_value' => 'new',
    '#options' => array(
      'new' => t('Create new content'),
      'replace' => t('Replace existing content')
    ),
  );

  $form['content_replace_wrapper'] = array(
    '#type' => 'container',
    '#states' => array(
      'visible' => array(
        ':input[name="publish_options"]' => array('value' => 'replace')
      )
    ),
  );

  $options = array('' => '- Select -');
  if (!empty($form_state['values']['book_id'])) {
    $options += epub_import_book_children($form_state['values']['book_id']);
  }
  else {
    $options[0] = '(Select a Book)';
  }

  $form['content_replace_wrapper']['content_replace'] = array(
    '#type' => 'select',
    '#title' => t('Content to be replaced'),
    '#options' => $options,
    '#prefix' => '<div id="book-children-wrapper">',
    '#suffix' => '</div>',
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );

  return $form;
}

/**
 * Validator for form epub_import_import_form().
 */
function epub_import_import_form_validate($form, &$form_state) {
  $errors = $epub_files = array();
  $files = $_FILES['files'];

  if (!is_dir(EPUB_IMPORT_TEMP_PATH)) {
    mkdir(EPUB_IMPORT_TEMP_PATH, 0755, TRUE);
  }

  // Clear temp folder first.
  if (system('rm -rf ' . EPUB_IMPORT_TEMP_PATH . '/*') === FALSE) {
    form_set_error('', 'Temporary directory not writable. Contact site administrator before proceeding.');
    watchdog('epub import', 'Temporary directory not writable.', WATCHDOG_ERROR);
    return;
  }

  for ($i = 0; $i < count($files['name']); $i++) {
    if ($files['size'][$i]) {
      if ($files['type'][$i] != 'application/epub+zip') {
        $errors[] = t('<b>!file</b> is not an epub format.', array('!file' => $files['name'][$i]));
        continue;
      }
      elseif (!empty($files['error'][$i])) {
        $errors[] = t('<b>!file</b>: upload error (code !code).', array('!code' => $files['error'][$i]));
        continue;
      }
      elseif (empty($errors)) {
        $file_path = EPUB_IMPORT_TEMP_PATH . '/' . $files['name'][$i];
        move_uploaded_file($files['tmp_name'][$i], $file_path);
        $epub_files["file$i"] = $files['name'][$i];
      }
    }
  }

  if ($errors) {
    foreach ($errors as $error) {
      form_set_error('', $error);
    }
    watchdog('epub import', join("\n", $errors));
  }
  elseif ($epub_files) {
    // Unzip epub files.
    try {
      foreach ($epub_files as $key => $file_name) {
        $zip = new ZipArchive();
        $zip->open(EPUB_IMPORT_TEMP_PATH . '/' . $file_name);
        $zip->extractTo(EPUB_IMPORT_TEMP_PATH . '/' . $key);
        $zip->close();
      }
    }
    catch (Exception $ex) {
      form_set_error('', 'File unarchiving error.');
      watchdog('epub import', 'At form: File unarchiving error.');
      return;
    }

    $form_state['files'] = $epub_files;
  }
  else {
    form_set_error('', 'No files uploaded to process.');
    watchdog('epub import', 'At form: No files uploaded to process.');
  }
}

/**
 * Submit handler for form epub_import_import_form().
 */
function epub_import_import_form_submit($form, $form_state) {
  $parse_count = $page_count = 0;

  variable_set('epub_import_header_level', $form_state['values']['epub_import_header_level']);

  $book_id = $form_state['values']['book_id'];

  if ($form_state['values']['publish_options'] == 'replace') {
    if ($book_page_mlid = $form_state['values']['content_replace']) {
      epub_import_book_page_delete($book_id, $book_page_mlid);
    }
  }

  foreach (array_keys($form_state['files']) as $epub) {
    _epub_import_current_file_key($epub);
    
    if ($parsed = epub_import_parse_epub($epub)) {
      epub_import_save_nodes($parsed);
      $parse_count++;
      $page_count += count($parsed);
    }
  }

  if ($parse_count == count($form_state['files'])) {
    $book = node_load($book_id);

    drupal_set_message('Epub file imported and parsed successfully.');
    drupal_set_message(t('@pages created in !book.',
      array(
        '@pages' => format_plural($page_count, '1 book page was', '@count book pages were'),
        '!book' => l($book->title, "node/$book_id")
      )
    ));

    drupal_set_message(t('You can !reorder if necessary.',
      ['!reorder' => l('reorder book structure', "admin/content/book/$book_id")]));

    watchdog('epub import', t('@pages created in !book.',
      array(
        '@pages' => format_plural($page_count, '1 book page was', '@count book pages were'),
        '!book' => l($book->title, "node/$book_id")
      )
    ));
  }
  else {
    drupal_set_message('Some Epub(s) not parsed.', 'error');
    watchdog('epub import', 'Some Epub(s) not parsed.', WATCHDOG_ERROR);
  }
}

/**
 * Get the list of all top level Books. Create a Book if there is none.
 * 
 * @return
 *   An array of Books (nid => title).
 */
function epub_import_get_books() {
  // Add first Book if necessary.
  if (!book_get_books()) {
    $book = new stdClass();
    $book->language = LANGUAGE_NONE;
    $book->type = 'book';
    $book->is_new = TRUE;
    $book->uid = 1;

    $book->title = 'Book One';
    $book->book['bid'] = 'new';
    $book = node_submit($book);

    node_save($book);
  }

  $options = array();
  foreach (book_get_books() as $book) {
    $options[$book['nid']] = $book['title'];
  }

  return $options;
}

/**
 * Get array of first level children of a Book node. Array key contains mlid. To
 * be used as drop down options in the form.
 * 
 * @param $book_id
 *   Node id of a top level Book.
 * 
 * @return
 *   An array of first level children of Book.
 */
function epub_import_book_children($book_id) {
  if (empty($book_id)) {
    return array('0' => '(Select a Book)');
  }

  $children = book_toc($book_id, 2);

  // Remove dashes and the first element (book itself).
  foreach ($children as $key => $val) {
    if (strpos($val, '-- ') === 0) {
      $children[$key] = substr($val, 3);
    }
    else {
      unset($children[$key]);
    }
  }

  return $children ? $children : array('0' => '(Empty book)');
}

/**
 * Ajax callback for Book dropdown.
 */
function epub_import_fill_book_children_callback($form, $form_state) {
  return $form['content_replace_wrapper']['content_replace'];
}

/**
 * Deletes a Book page and its children. As deleting one via node_delete() won't
 * automatically delete its children, it finds all the children and delete them.
 * 
 * @param $book_id
 *   Book's node ID.
 * @param $book_page_mlid
 *   Menu link id of the Book page to delete. Book hierarchy uses menu system.
 */
function epub_import_book_page_delete($book_id, $book_page_mlid) {
  $menu_links_ids = array();
  $top_found = FALSE;
  $children = book_toc($book_id, 10);

  foreach ($children as $key => $val) {
    // Find the top item to delete.
    if ($key == $book_page_mlid) {
      $menu_links_ids[] = $key;
      $top_found = TRUE;
    }
    // Find the children of the top item.
    elseif ($top_found and strpos($val, '----') === 0) {
      $menu_links_ids[] = $key;
    }
  }

  $nids = array();
  // Prepares book nids in order to delete book pages from bottom up, which is
  // more efficient as there is overhead of a book page getting deleted posing
  // on its children book pages.
  while ($mlid = array_pop($menu_links_ids)) {
    $menu = menu_link_load($mlid);
    $nids[] = explode('/', $menu['link_path'])[1];
  }

  node_delete_multiple($nids);
}
