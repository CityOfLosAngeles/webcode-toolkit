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
    if (preg_match('#^<h[123].+</h[123]>$#', $line) or preg_match('#^<p class="Heading-[23].+</p>$#', $line)) {
      // Process unfinished lists that came before the headings.
      if ($list_mode and !empty($list)) {
        $html[$unit_id]['content'] .= _epub_import_render_ordered_list($list);
        $list = array();
        $list_mode = FALSE;
      }

      list($level, $unit_id, $title) = _epub_import_parse_heading(strip_tags($line));

      $html[$unit_id]['level']   = $level;
      $html[$unit_id]['title']   = strip_tags($line);
      $html[$unit_id]['content'] = '';
      $html[$unit_id]['terms']   = array();

      _epub_import_current_node_id($unit_id);
    }
    // Webcode token on its own line.
    elseif (strpos($line, 'webcode:') !== FALSE) {
      // Article is already recognized by <h1>, so no need (for now).
    }
    elseif ($clean_line = _epub_import_clean_line($line)) {
      $list_type = _epub_import_get_list_class($line);

      if ($list_type == 'L1') {
        $list[] = 'L1: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'L2') {
        $list[] = 'L2: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'L3') {
        $list[] = 'L3: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'L4') {
        $list[] = 'L4: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'L5') {
        $list[] = 'L5: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'L6') {
        $list[] = 'L6: ' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_type == 'EOF') {
        break;
      }
      else {
        if ($clean_line == '<table>') {
          $table_mode = TRUE;
          $clean_line = '<table class="table">';
          $table = array('<div class="table-responsive">', $clean_line);
        }
        elseif ($clean_line == '</table>') {
          $table_mode = FALSE;
          $table[] = $clean_line;
          $table[] = '</div>'; // .table-responsive
        }
        elseif ($clean_line == '<thead>') {
          $thead = TRUE;
          $table[] = $clean_line;
        }
        elseif ($clean_line == '<tbody>') {
          $thead = FALSE;
          $table[] = $clean_line;
        }
        elseif ($clean_line == '<tr>') {
          if (!empty($thead)) {
            $clean_line = '<tr class="rowheader">';
          }

          $tr = array($clean_line);
        }
        elseif ($clean_line == '</tr>') {
          $tr[] = $clean_line;

          // Change td in header row to th.
          if (strpos($tr[0], 'rowheader')) {
            foreach ($tr as $key => $val) {
              $tr[$key] = preg_replace(
                         array('#^<td#', '#</td>$#'),
                         array('<th', '</th>'),
                         $val
                       );
            }
          }
          // <td> contains colspan, so add 'rowheader' class to parent <tr>
          elseif (strpos($tr[1], 'colspan')) {
            $tr[0] = '<tr class="rowheader">';

            foreach ($tr as $key => $val) {
              $tr[$key] = preg_replace(
                         array('#^<td#', '#</td>$#'),
                         array('<th', '</th>'),
                         $val
                       );
            }
          }

          // Strip off p tag in td
          foreach ($tr as $key => $val) {
            $val = preg_replace(
                       array('#^<p>#', '#</p>$#'),
                       '',
                       $val
                     );

            $val = preg_replace('#^<(td|th)(.*)\s*(/>)$#', '<$1$2></$1>', $val);
            $tr[$key] = $val;
          }

          $tr = join('', $tr);
          if (trim(strip_tags($tr, '<img>'))) {
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
          if (!empty($table) and !empty($unit_id)) {
            $list[] = join('', $table);
            $table = array();
          }
          else {
            $list[] = $clean_line;
          }
        }
        elseif (!empty($unit_id)) {
          if (!empty($table)) {
            $html[$unit_id]['content'] .= join('', $table);
            $table = array();
          }
          else {
            $html[$unit_id]['content'] .= $clean_line;
          }
        }
      }

      // Parse terms.
      if (epub_import_get_level($level) == 'Section') {
        _epub_import_parse_terms($html[$unit_id], $line);
      }
    }
  }

  if ($list_mode and !empty($list)) {
    $html[$unit_id]['content'] .= _epub_import_render_ordered_list($list);
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
    $heading = epub_import_to_heading($content['level']);

    if ($heading < $header_level) {
      unset($parsed[$key]);
    }
    elseif ($heading == $header_level) {
      $prev_key = $key;
    }
    elseif (!empty($prev_key)) {
      $h = epub_import_to_heading($parsed[$key]['level']);
      $parsed[$prev_key]['content'] .= "\n<$h>{$parsed[$key]['title']}</$h>\n{$parsed[$key]['content']}";
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
    'value' => $content['content'],
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

/**
 * Breakdown a parse heading line. Expected format: Section 1.2.3. Example title
 */
function _epub_import_parse_heading($line) {
  $line = preg_replace('/\s{2,}/', ' ', $line);
  $line = html_entity_decode($line);
  return explode(' ', $line, 3);
}

function _epub_import_get_list_class($line) {
  if (preg_match('#^<h[456].+</h[456]>$#', $line)) {
    switch (substr($line, 1, 2)) {
      case 'h4': return 'L1';
      case 'h5': return 'L2';
      case 'h6': return 'L3';
    }
  }
  // Check if the headings come in as <p> and allow them in.
  elseif (preg_match('#^<p class="Heading-(\d).+</p>$#', $line, $match)) {
    switch ($match[1]) {
      case '4': return 'L1';
      case '5': return 'L2';
      case '6': return 'L3';
      case '7': return 'L4';
      case '8': return 'L5';
      case '9': return 'L6';
    }
  }
  // Class name TOC-* near the ned of file indicates there is no more content.
  elseif (preg_match('/class="TOC[_\-]([^"]+)/', $line, $match)) {
    return 'EOF';
  }
}

function _epub_import_render_ordered_list($list) {
  $level = 0;
  $output = '<ol type="A">';

  foreach ($list as $item) {
    $depth = substr($item, 0, 4);
    // Level 1 (Uppercase)
    if ($depth == 'L1: ') {
      $output .= (($level == 1) ? '</li>' :
                 (($level == 2) ? '</li></ol></li>' :
                 (($level == 3) ? '</li></ol></li></ol></li>' :
                 (($level == 4) ? '</li></ol></li></ol></li></ol></li>' :
                 (($level == 5) ? '</li></ol></li></ol></li></ol></li></ol></li>' :
                 (($level == 6) ? '</li></ol></li></ol></li></ol></li></ol></li></ol></li>' : ''))))));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 1;
    }
    // Level 2 (Numbered)
    elseif ($depth == 'L2: ') {
      $output .= (($level == 1) ? '<ol type="1">' :
                 (($level == 2) ? '</li>' :
                 (($level == 3) ? '</li></ol></li>' :
                 (($level == 4) ? '</li></ol></li></ol></li>' :
                 (($level == 5) ? '</li></ol></li></ol></li></ol></li>' :
                 (($level == 6) ? '</li></ol></li></ol></li></ol></li></ol></li>' : ''))))));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 2;
    }
    // Level 3 (Lowercase)
    elseif ($depth == 'L3: ') {
      $output .= (($level == 2) ? '<ol type="a">' :
                 (($level == 3) ? '</li>' :
                 (($level == 4) ? '</li></ol></li>' :
                 (($level == 5) ? '</li></ol></li></ol></li>' :
                 (($level == 6) ? '</li></ol></li></ol></li></ol></li>' : '')))));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 3;
    }
    // Level 4 (Roman)
    elseif ($depth == 'L4: ') {
      $output .= (($level == 3) ? '<ol type="i">' :
                 (($level == 4) ? '</li>' :
                 (($level == 5) ? '</li></ol></li>' :
                 (($level == 6) ? '</li></ol></li></ol></li>' : ''))));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 4;
    }
    // Level 5 (Lowercase)
    elseif ($depth == 'L5: ') {
      $output .= (($level == 4) ? '<ol type="a">' :
                 (($level == 5) ? '</li>' :
                 (($level == 6) ? '</li></ol></li>' : '')));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 5;
    }
    // Level 6 (Lowercase)
    elseif ($depth == 'L6: ') {
      $output .= (($level == 5) ? '<ol type="a">' :
                 (($level == 6) ? '</li>' : ''));

      $output .= '<li>' . _epub_import_clean_list_item($item);
      $level = 6;
    }
    // non-bullet content
    else {
      $output .= '<p>' . $item . '</p>';
    }
  }

  $output .= (($level == 1) ? '</li>' :
             (($level == 2) ? '</li></ol></li>' :
             (($level == 3) ? '</li></ol></li></ol></li>' :
             (($level == 4) ? '</li></ol></li></ol></li></ol></li>' :
             (($level == 5) ? '</li></ol></li></ol></li></ol></li></ol></li>' : '')))));

  $output .= '</ol>';

  return $output;
}

function _epub_import_clean_list_item($item) {
  // Remove 'L#: ' depth indicator.
  $item = substr(trim($item), 4);
  $item = str_replace("&#9;", ' ', $item);
  // Trim unnecessarily enclosing <p> tag.
  $item = preg_replace('#^<p>(.+)</p>$#', '$1', $item);

  // lowercase with parenthesis.
  if ($pos = strpos($item, ')') and $pos <= 2) {
    list(, $item) = explode(')', $item, 2);
  }
  elseif ($pos = strpos($item, '.') and $pos <= 8) {
    list(, $item) = explode('.', $item, 2);
  }

  return trim($item);
}

/**
 * Clean up parsed line.
 */
function _epub_import_clean_line($line) {
  static $except = '<table><colgroup><col><thead><tbody><th><tr><td><img><a><em><p><br>';

  $line = _epub_import_process_webdings($line);
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
 * Turn single g and c into unicode squares (black and white).
 * G and C are squares in webdings font.
 */
function _epub_import_process_webdings($str) {
  $str = preg_replace('#<span class="[\w-]+">g</span>#', '&#9726;', $str);
  $str = preg_replace('#<span class="[\w-]+">c</span>#', '&#9725;', $str);

  return $str;
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
  $str = preg_replace('#^<([a-z]+) >#', '<$1>', $str);

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

  // Ignore image with inline data.
  if (strpos($line, 'src="data:image')) {
    $line = '';
  }
  elseif (preg_match('/src="([^"]+)"/', $line, $match)) {
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

function epub_import_get_level($level) {
  switch ($level) {
    case 'Sec.': return 'Section';
    case 'Div.': return 'Division';
    default:     return $level;
  }
}

function epub_import_to_heading($level) {
  switch ($level) {
    case 'Article':
      return 'h1';
    case 'Div.': 
    case 'Division':
      return 'h2';
    case 'Sec.':
    case 'Section':
      return 'h3';
    default:
      return $level;
  }
}
