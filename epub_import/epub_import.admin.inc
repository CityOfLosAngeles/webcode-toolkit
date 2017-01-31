<?php

/**
 * @file
 * Epub Import admin settings.
 */

function epub_import_admin_settings() {
  if (epub_import_is_configured()) {
    $form['import'] = array(
      '#markup' => '<h3>' . l('Import Epub files', 'admin/epub-import') . '</h3>',
    );
  }

  $types = node_type_get_names();
  $allowed = variable_get('book_allowed_types', array('book'));
  $types = array_filter($types, function ($val) use ($allowed) {
    return in_array($val, $allowed);
  }, ARRAY_FILTER_USE_KEY);
  $form['epub_import_node_type'] = array(
    '#type' => 'radios',
    '#title' => t('Import Epub as'),
    '#default_value' => variable_get('epub_import_node_type', ''),
    '#options' => $types,
    '#description' => t('You can enable !books to other content types.', array('!books' => l('book outlines', 'admin/content/book/settings'))),
    '#required' => TRUE,
  );

  epub_import_check_book();

  $options = array(0 => '<' . t('none') . '>');
  foreach (book_get_books() as $book) {
    $options[$book['nid']] = $book['title'];
  }

  // Add a drop-down to select the destination book.
  $form['epub_import_book_id'] = array(
    '#type' => 'select',
    '#title' => t('Book'),
    '#default_value' => variable_get('epub_import_book_id', 0),
    '#options' => $options,
    '#description' => t('Your new upload will be a part of the selected book.'),
  );

  $form['epub_import_header_level'] = array(
    '#type' => 'select',
    '#title' => t('Header level for each content'),
    '#description' => t('The header level for which a content is create.'),
    '#default_value' => variable_get('epub_import_header_level', 'h1'),
    '#options' => array(
      'h1' => 'H1',
      'h2' => 'H2',
      'h3' => 'H3',
    ),
  );

  $text_formats = array();
  foreach(filter_formats() as $ff) {
    $text_formats[$ff->format] = $ff->name;
  }

  $form['epub_import_text_format'] = array(
    '#type' => 'radios',
    '#title' => t('Text format'),
    '#description' => t('Contents will be created using the text format.'),
    '#default_value' => variable_get('epub_import_text_format', 'full_html'),
    '#options' => $text_formats,
    '#required' => TRUE,
  );

  $image_styles = array();
  foreach (image_styles() as $is) {
    $image_styles[$is['name']] = $is['label'];
  }

  $form['epub_import_image_style'] = array(
    '#type' => 'radios',
    '#title' => t('Image style'),
    '#description' => t('Images in the content will show using the image style.'),
    '#default_value' => variable_get('epub_import_image_style', ''),
    '#options' => $image_styles,
    '#required' => TRUE,
  );


  $term_fields = array();

  if ($fields = field_info_instances('node', variable_get('epub_import_node_type', ''))) {
    foreach ($fields as $key => $field) {
      if ($field['display']['default']['module'] == 'taxonomy') {
        $field_info = field_info_field($key);

        if ($field_info['type'] == 'taxonomy_term_reference') {
          $term_fields[$field_info['settings']['allowed_values'][0]['vocabulary'] . ':' . $field_info['field_name']] = $field['label'] . ' (' . $field_info['field_name'] . ')';
        }
      }
    }
  }

  $form['epub_import_terms'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Taxonomy vocabulary'),
    '#description' => t('Tagged words are recognized as terms associated with the content.'),
    '#default_value' => variable_get('epub_import_terms', array()),
    '#options' => $term_fields,
  );

  $form['epub_import_image_folder'] = array(
    '#type' => 'textfield',
    '#title' => t('Images folder'),
    '#description' => t('Images from Epub will be saved under this folder. e.g. /sites/default/files/') . variable_get('epub_import_image_folder', 'epub_images'),
    '#default_value' => variable_get('epub_import_image_folder', 'epub_images'),
  );

  $form['epub_import_class_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Class name prefix'),
    '#description' => t('Class name prefix used in the Epub file.<br><br>Recognized class names (where prefix is xxx):<br> - xxx-list-alpha<br> - xxx-list-numbered<br> - xxx-term-yyy (yyy is a taxonomy vocabulary machine name)'),
    '#default_value' => variable_get('epub_import_class_prefix', ''),
    '#required' => TRUE,
  );

  return system_settings_form($form);
}

/**
 * Create first top level book if needed.
 */
function epub_import_check_book() {
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
}
