<?php

/**
 * @file
 * Epub Import page.
 */

define('EPUB_IMPORT_TEMP_PATH', variable_get('file_temporary_path', file_directory_temp()) . '/epub-import');

/**
 * Menu callback for 'admin/epub-import'
 */
function epub_import_import_form() {
  if (!epub_import_is_configured()) {
    $form['need_config'] = array(
      '#markup' => l(t('Configure Epub import settings to start importing.'), 'admin/config/media/epub-import'),
    );

    return $form;
  }

  $heading_level = variable_get('epub_import_header_level', 'h1');

  $instruction =<<<INSTR
    <h3>Upload tips:</h3>
    <ul>
      <li>You can select multiple files by Ctrl+Click (⌘+Clicl for Mac)</li>
      <li>Heading level (currently set to '$heading_level') determines how many node are created.</li>
    </ul>
INSTR;

  $form['instruction'] = array(
    '#markup' => $instruction,
  );

  $form['import'] = array(
    '#type' => 'file',
    '#title' => t('Upload file(s)'),
    '#name' => 'files[]',
    '#attributes' => array('multiple' => 'multiple'),
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
      return;
    }

    $form_state['files'] = $epub_files;
  }
  else {
    form_set_error('', 'No files uploaded to process.');
  }
}

/**
 * Submit handler for form epub_import_import_form().
 */
function epub_import_import_form_submit($form, $form_state) {
  foreach (array_keys($form_state['files']) as $epub) {
    _epub_import_current_file_key($epub);
    
    if ($parsed = epub_import_parse_epub($epub)) {
      epub_import_save_nodes($parsed);
    }
  }

  drupal_set_message('Epub imported and parsed successfully.');
}

/**
 * Return parsed HTML from epub file.
 */
function epub_import_parse_epub($epub) {
  if (!$fp = epub_import_open_epub($epub)) {
    return FALSE;
  }
  
  $html = $list = $table = array();
  $list_mode = $table_mode = FALSE;
  $level = '';

  while ($data = fgets($fp)) {
    $line = trim($data);
    
    // Heading found (article, division, section). Don't check class Heading-1
    // as it is confusing with Heading-10.
    if (preg_match('#^<h[123].+</h[123]>$#', $line) or preg_match('#^<p class="Heading-[23].+</p>$#', $line))  {
      // Process unfinished lists that came before the headings.
      if ($list_mode and !empty($list)) {
        $html[$unit_id]['body'] .= _epub_import_render_ordered_list($list);
        $list = array();
        $list_mode = FALSE;
      }

      $heading = substr($line, 1, 2);
      $line    = strip_tags($line);
      $unit_id = substr(md5($line), 0 , 8);

      $html[$unit_id]['level'] = $heading;
      $html[$unit_id]['title'] = $line;
      $html[$unit_id]['body'] = '';
      $html[$unit_id]['terms'] = array();

      _epub_import_current_node_id($unit_id);
    }
    elseif ($clean_line = _epub_import_clean_line($line)) {
      $list_type = _epub_import_get_list_class($line);

      if ($list_type == 'alpha') {
        $list[] = '# ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'numbered') {
        $list[] = '## ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'EOF') {
        break;
      }
      else {
        if ($clean_line == '<table>') {
          $table_mode = TRUE;
          $table = array($clean_line);
        }
        elseif ($clean_line == '</table>') {
          $table_mode = FALSE;
          $table[] = $clean_line;
        }
        elseif ($clean_line == '<tr>') {
          $tr = array($clean_line);
        }
        elseif ($clean_line == '</tr>') {
          $tr[] = $clean_line;

          // <td> contains colspan attr, so add class to parent <tr>
          if (strpos($tr[1], 'colspan')) {
            $tr[0] = '<tr class="rowheader">';
          }

          $tr = join('', $tr);
          if (trim(strip_tags($tr))) {
            $table[] = $tr;
          }
          $tr = NULL;
        }
        elseif (!empty($tr)) {
          $tr[] = $clean_line;
        }
        elseif ($clean_line == '<colgroup>' or $clean_line == '</colgroup>' or $clean_line == '<col />') {
          // Ignore <colgroup> and <col />
        }
        elseif ($table_mode) {
          $table[] = $clean_line;
        }

        if ($table_mode) continue;

        if ($list_mode) {
          $list[] = $clean_line;
        }
        elseif (!empty($unit_id)) {
          if (!empty($table)) {
            $html[$unit_id]['body'] .= join('', $table);
            $table = array();
          }
          else {
            $html[$unit_id]['body'] .= $clean_line;
          }
        }
      }

      // Parse terms.
      if (!empty($unit_id)) {
        _epub_import_parse_terms($html[$unit_id], $line);
      }
    }
  }

  if ($list_mode and !empty($list)) {
    $html[$unit_id]['body'] .= _epub_import_render_ordered_list($list);
  }

  return $html;
}

function epub_import_open_epub($epub) {
  $path = EPUB_IMPORT_TEMP_PATH . "/$epub/OEBPS";

  if (!is_dir($path)) {
    form_set_error('', "Malformed epub file ($epub).");
    return FALSE;
  }

  foreach (scandir($path) as $file) {
    if ('xhtml' == pathinfo($file, PATHINFO_EXTENSION)) {
      $xml_file_name = $file;
      break;
    }
  }

  if (empty($xml_file_name)) {
    form_set_error('', 'Epub filing missing XML content.');
    return FALSE;
  }
  
  $file_path = $path . '/' . $xml_file_name;

  $html = array();

  if (!$fp = @fopen($file_path, "r")) {
    form_set_error('', "Could not read XML file ($epub).");
    return FALSE;
  }

  return $fp;
}

function epub_import_save_nodes(&$parsed) {
  epub_import_collapse($parsed);

  foreach ($parsed as $content) {
    epub_import_save_node($content);
  }
}

function epub_import_collapse(&$parsed) {
  $header_level = variable_get('epub_import_header_level', 'h1');

  foreach ($parsed as $key => $content) {
    if ($content['level'] < $header_level) {
      unset($parsed[$key]);
    }
    elseif ($content['level'] == $header_level) {
      $prev_key = $key;
    }
    elseif (!empty($prev_key)) {
      $h = $parsed[$key]['level'];
      $parsed[$prev_key]['body'] .= "\n<$h>{$parsed[$key]['title']}</$h>\n{$parsed[$key]['body']}";
      $parsed[$prev_key]['terms'] = array_merge_recursive($parsed[$prev_key]['terms'], $parsed[$key]['terms']);

      unset($parsed[$key]);
    }
  }
}

function epub_import_save_node($content) {
  global $user;

  $node = new stdClass();
  $node->type = variable_get('epub_import_node_type', '');
  $node->language = LANGUAGE_NONE;
  $node->is_new = TRUE;
  $node->uid = $user->uid;
  $node->title = $content['title'];
  $node->body[LANGUAGE_NONE][0] = array(
    'value' => $content['body'],
    'summary' => '',
    'format' => variable_get('epub_import_text_format', 'full_html'),
  );

  _epub_import_attach_terms($node, $content);

  node_save($node);

  drupal_set_message(node_type_get_name($node) . ' <em>' . $node->title . '</em> has been created.');
}

function _epub_import_attach_terms($node, $content) {
  static $import_fields, $vocab_machine_names;

  if (!isset($import_fields)) {
    $import_fields = array();

    foreach (variable_get('epub_import_terms', array()) as $key => $val) {
      if ($val) {
        list($machine_name, $field_name) = explode(':', $key);
        $import_fields[$machine_name] = $field_name;
      }
    }

    $vocab_machine_names = array_keys($import_fields);
  }

  if (!empty($content['terms'])) {
    foreach ($content['terms'] as $voca => $names) {
      if (in_array($voca, $vocab_machine_names)) {
        $names = array_unique($names);

        foreach ($names as $name) {
          if ($term = taxonomy_get_term_by_name($name, $voca)) {
            $term = reset($term);
            $node->{$import_fields[$voca]}[LANGUAGE_NONE][] = array('tid' => $term->tid);
          }
        }
      }
    }
  }
}

function _epub_import_get_list_class($line) {
  if (preg_match('/class="' . variable_get('epub_import_class_prefix', '') . '-list-([^"]+)/', $line, $match)) {
    return $match[1];
  }
  // Class name TOC-* near the ned of file indicates there is no more content.
  elseif (preg_match('/class="TOC-([^"]+)/', $line, $match)) {
    return 'EOF';
  }
}

function _epub_import_render_ordered_list($list) {
  $level = 0;
  $output = '<ol type="A">';

  foreach ($list as $item) {
    // First level (Alphabetic)
    if (substr($item, 0, 2) == '# ') {
      $output .= $level == 1 ? '</li>' : '';
      $output .= $level == 2 ? '</li></ol></li>' : '';
      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 1;
    }
    // Second level (Numbered)
    elseif (substr($item, 0, 3) == '## ') {
      $output .= $level == 1 ? '<ol>'  : '';
      $output .= $level == 2 ? '</li>' : '';
      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 2;
    }
    // non-bullet content
    else {
      $output .= '<p>' . $item . '</p>';
    }
  }

  $output .= $level == 1 ? '</li>' : '';
  $output .= $level == 2 ? '</li></ol></li>' : '';
  $output .= '</ol>';

  return $output;
}

function _epub_import_clean_list_item($item) {
  $item = trim($item);
  $item = ltrim($item, '# ');
  $item = str_replace("&#9;", ' ', $item);
  list(, $item) = explode('.', $item, 2);
  return trim($item);
}

/**
 * Clean up parsed line.
 */
function _epub_import_clean_line($line) {
  static $except = '<table><colgroup><col><thead><tbody><th><tr><td><img><a><em>';

  $line = strip_tags($line, $except);
  $line = _epub_import_strip_attrs($line);
  $line = str_replace(array('&nbsp;', '&#160;', '&#9;'), ' ', $line);

  $line = _epub_import_clean_img($line);

  return trim($line);
}

/**
 * Setter and getter for zoning code unit id currently worked on in order to
 * keep track of it from different locations in the code.
 */
function _epub_import_current_node_id($unit_id = NULL) {
  $id = &drupal_static(__FUNCTION__);

  if ($unit_id) {
    $id = $unit_id;
  }

  return $id;
}

/**
 * Setter and getter for keeping track of current file being worked on.
 */
function _epub_import_current_file_key($file_key = NULL) {
  $key = &drupal_static(__FUNCTION__);

  if ($file_key) {
    $key = $file_key;
  }

  return $key;
}

/**
 * Parse term names with proper class names.
 */
function _epub_import_parse_terms(&$content, $line) {
  preg_match_all('#<span class="' . variable_get('epub_import_class_prefix', '') . '-term-([^"]+)">(.+?)</span>#', $line, $match);

  if (!empty($match[0])) {
    $vocas = $match[1];
    $texts = $match[2];

    foreach ($vocas as $key => $voca) {
      if (!isset($content['terms'][$voca])) {
        $content['terms'][$voca] = array();
      }

      if (!in_array($texts[$key], $content['terms'][$voca])) {
        $content['terms'][$voca][] = $texts[$key];
      }
    }
  }
}

/**
 * Strip attributes off given string keeping allowed list.
 * 
 * Src: http://stackoverflow.com/questions/770219/how-can-i-remove-attributes-from-an-html-tag
 */
function _epub_import_strip_attrs($str) {
  static $allow = array('src', 'colspan', 'rowspan', 'href', 'data-toggle', 'data-type', 'data-title', 'data-content');

  if (preg_match_all("/<[^>]*\s([^>]*)\/*>/msiU", $str, $res, PREG_SET_ORDER)) {
   foreach ($res as $r) {
     $tag = $r[0];
     $attrs = array();
     preg_match_all("/\s.*=(['\"]).*\\1/msiU", " " . $r[1], $split, PREG_SET_ORDER);
     foreach ($split as $spl) {
      $attrs[] = $spl[0];
     }
     $newattrs = array();
     foreach ($attrs as $a) {
      $tmp = explode("=", $a);
      if (trim($a) != "" && (!isset($tmp[1]) || (trim($tmp[0]) != "" && !in_array(strtolower(trim($tmp[0])), $allow)))) {

      } else {
        $newattrs[] = trim($a);
      }
     }
     $attrs = implode(" ", $newattrs);
     $rpl = str_replace($r[1], $attrs, $tag);
     $str = str_replace($tag, $rpl, $str);
   }
  }

  // Remove any extra space. e.g. <table >
  if (substr($str, -2) == ' >') {
    $str = substr($str, 0, -2) . '>';
  }

  return $str;
}

function _epub_import_clean_img($line) {
  static $last_unit_id, $count, $image_style;

  if (!isset($image_style)) {
    $image_style = variable_get('epub_import_image_style', '');
  }

  $epub_path   = EPUB_IMPORT_TEMP_PATH . '/' . _epub_import_current_file_key() . '/OEBPS';
  $image_path  = variable_get('epub_import_image_folder', 'epub_images');
  $system_path = variable_get('file_public_path') . '/' . $image_path;

  if (preg_match('/src="([^"]+)"/', $line, $match)) {
    $src = $match[1];
    $img_name = pathinfo($src, PATHINFO_BASENAME);
    $img_dest = $system_path . '/' . $img_name;

    if (!file_exists($system_path) && !is_dir($system_path)) {
      mkdir($system_path, 0777, TRUE);
    }

    $dest = file_unmanaged_copy("$epub_path/$src", $img_dest, FILE_EXISTS_RENAME);
    $new_name = pathinfo($dest, PATHINFO_BASENAME);
    $src  = image_style_url($image_style, "$image_path/$new_name");
    $line = preg_replace('/src="([^"]+)"/', 'src="' . $src . '" class="img-responsive"', $line);
  }

  return $line;
}
