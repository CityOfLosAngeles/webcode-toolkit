<?php

/**
 * @file
 * Epub Import page.
 */

/**
 * @return array
 *   Array of parsed XML from epub file.
 */
function epub_import_parse_epub($epub) {
  if (!$fp = epub_import_open_epub($epub)) {
    return FALSE;
  }
  
  $parsed = $list = $table = array();
  $list_mode = $table_mode = FALSE;

  while ($data = fgets($fp)) {
    $line = trim($data);

    // Heading found.
    if (preg_match('#^<h[123].+</h[123]>$#', $line)) {
      // Process unfinished lists that came before the headings.
      if ($list_mode and !empty($list)) {
        $parsed[$heading_id]['content'] .= _epub_import_render_list($list);
        $list = array();
        $list_mode = FALSE;
      }

      list($heading, $title) = _epub_import_parse_heading($line);

      $heading_id = _epub_import_current_unit_id($heading);

      $parsed[$heading_id]['heading'] = $heading;
      $parsed[$heading_id]['title']   = $title;
      $parsed[$heading_id]['content'] = '';
      $parsed[$heading_id]['terms']   = array();
    }
    elseif ($clean_line = _epub_import_clean_line($line)) {
      list($list_level, $list_type) = _epub_import_get_list_class($line);

      if (in_array($list_level, array('L1', 'L2', 'L3', 'L4', 'L5', 'L6'))) {
        $list[] = $list_level . ':' . $list_type . ':' . $clean_line;
        $list_mode = TRUE;
      }
      elseif ($list_level == 'UL') {
        $list[] = $list_level . ':' . $list_type . ':' . $clean_line;
        $list_mode = TRUE;
      }
      elseif (in_array($list_level, array('h4', 'h5', 'h6'))) {
        if ($list_mode and !empty($list)) {
          $parsed[$heading_id]['content'] .= _epub_import_render_list($list);
          $list = array();
          $list_mode = FALSE;
        }

        $parsed[$heading_id]['content'] .= "<$list_level>$clean_line</$list_level>";
      }
      elseif ($list_level == 'EOF') {
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

        if (!$table_mode) {
          if ($list_mode) {
            if (!empty($table) and !empty($heading_id)) {
              $list[] = join('', $table);
              $table = array();
            }
            else {
              $list[] = $clean_line;
            }
          }
          elseif (!empty($heading_id)) {
            if (!empty($table)) {
              $parsed[$heading_id]['content'] .= join('', $table);
              $table = array();
            }
            /*elseif (in_array($list_level, array('h4', 'h5', 'h6'))) {
              $parsed[$heading_id]['content'] .= "<$list_level>$clean_line</$list_level>";
            }*/
            else {
              $parsed[$heading_id]['content'] .= $clean_line;
            }
          }
        }
      }
    }

    // TODO: Parse terms. # Should it always happen at h3 level? 
    if (!empty($heading) and $heading == 'h3') {
      _epub_import_parse_terms($parsed[$heading_id], $line);
    }
  }

  if ($list_mode and !empty($list)) {
    $parsed[$heading_id]['content'] .= _epub_import_render_list($list);
  }

  return $parsed;
}

function epub_import_open_epub($epub) {
  $path = EPUB_IMPORT_TEMP_PATH . "/$epub/OEBPS";

  if (!is_dir($path)) {
    form_set_error('', "Malformed epub file ($epub).");
    return FALSE;
  }

  foreach (scandir($path) as $file) {
    if ('xhtml' == pathinfo($file, PATHINFO_EXTENSION) and $file != 'cover.xhtml') {
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

  foreach ($parsed as $heading_id => $data) {
    if ($data['heading'] < $header_level) {
      // Leave the data as is.
    }
    elseif ($data['heading'] == $header_level) {
      $prev_heading_id = $heading_id;
    }
    elseif (!empty($prev_heading_id)) {
      $h = $parsed[$heading_id]['heading'];
      $parsed[$prev_heading_id]['content'] .= "\n<$h>{$parsed[$heading_id]['title']}</$h>\n{$parsed[$heading_id]['content']}";
      $parsed[$prev_heading_id]['terms'] = array_merge_recursive($parsed[$prev_heading_id]['terms'], $parsed[$heading_id]['terms']);

      unset($parsed[$heading_id]);
    }
  }
}

function epub_import_save_node($content) {
  global $user;
  static $ml_stack = array();
  static $hlevel;
  static $weight = array();

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

  $node->book['bid'] = $_POST['book_id'];

  if (empty($hlevel)) {
    $hlevel = (int)substr(variable_get('epub_import_header_level', 'h1'), 1);
  }
  $level = (int)substr($content['heading'], 1);

  if ($level < $hlevel) {
    if (isset($ml_stack[$level])) {
      unset($ml_stack[$level]);
    }
    if ($level - 1 > 0) {
      $node->book['plid'] = $ml_stack[$level - 1];
    }
    $push_self = TRUE;
  }
  elseif ($level > 1) {
    $node->book['plid'] = $ml_stack[$level - 1];
  }

  // Assign weights on top of sibling's weight.
  if (isset($weight[$level])) {
    $weight[$level]++;

    // Reset children's weights.
    if (isset($weight[$level + 1])) unset($weight[$level + 1]);
    if (isset($weight[$level + 2])) unset($weight[$level + 2]);
  }
  else {
    $weight[$level] = -15; // Book module uses weights between -15 and 15.
  }
  $node->book['weight'] = $weight[$level];

  _epub_import_attach_terms($node, $content);

  node_save($node);

  if (!empty($push_self)) {
    $ml_stack[$level] = $node->book['mlid'];
  }

  watchdog('epub import', node_type_get_name($node) . ' "' . $node->title . '" has been created.');
}

function _epub_import_attach_terms($node, $parsed) {
  static $import_fields, $vocab_machine_names;

  if (!isset($import_fields)) {
    $import_fields = array();

    foreach (variable_get('epub_import_terms', array()) as $val) {
      if (!empty($val)) {
        list($machine_name, $field_name) = explode(':', $val);
        $import_fields[$machine_name] = $field_name;
      }
    }

    $vocab_machine_names = array_keys($import_fields);
  }

  if (!empty($parsed['terms'])) {
    foreach ($parsed['terms'] as $field => $names) {
      if (in_array($field, $import_fields)) {
        $names = array_unique($names);
        $vocab = array_search($field, $import_fields);

        foreach ($names as $name) {
          if ($term = taxonomy_get_term_by_name($name, $vocab)) {
            $term = reset($term);
            $node->{$field}[LANGUAGE_NONE][] = array('tid' => $term->tid);
          }
        }
      }
    }
  }
}

/**
 * Parse heading and title from parsed line.
 */
function _epub_import_parse_heading($line) {
  if (preg_match('#^<(h\d)#', $line, $match)) {
    return array(
      $match[1],
      html_entity_decode(strip_tags($line))
    );
  }
}

function _epub_import_get_list_class($line) {
  // Check if the headings come in as <p> and allow them in.
  if (preg_match('#^<(p|h\d) class="ol[\w- ]*">(<span class="[\w- ]+">)?([A-Z]|[a-z]|[1-9][0-9]?|[ivx]{1,5})\..+</(p|h\d)>$#', $line, $match)) {
    if (preg_match('#[A-Z]#', $match[3])) {
      return array('L1', 'A');
    }
    elseif (preg_match('#[1-9][0-9]?#', $match[3])) {
      return array('L2', '1');
    }
    elseif (preg_match('#[a-z]#', $match[3])) {
      return array('L3', 'a');
    }
    elseif (preg_match('#[ivx]{1,5}#', $match[3])) {
      return array('L4', 'i');
    }
  }
  elseif (preg_match('#^<(p|h\d) class="ol-(\d)[\w- ]*">(<span class="[\w- ]+">)?([A-Z]|[a-z]|[1-9][0-9]?|[ivx]{1,5})\..+</(p|h\d)>$#', $line, $match)) {
    $level = $match[1];

    if (preg_match('#[A-Z]#', $match[4])) {
      return array('L'.$level, 'A');
    }
    elseif (preg_match('#[1-9][0-9]?#', $match[4])) {
      return array('L'.$level, '1');
    }
    elseif (preg_match('#[a-z]#', $match[4])) {
      return array('L'.$level, 'a');
    }
    elseif (preg_match('#[ivx]{1,5}#', $match[4])) {
      return array('L'.$level, 'i');
    }
  }
  elseif (preg_match('#^<p class="ul">.+</p>$#', $line, $match)) {
    return array('UL', NULL);
  }
  elseif (preg_match('#^<(h[456]).+</h[456]>$#', $line, $match)) {
    return array($match[1], NULL);
  }
  // Class name TOC-* near the ned of file indicates there is no more content.
  elseif (preg_match('/class="TOC[_\-]([^"]+)/', $line, $match)) {
    return array('EOF', NULL);
  }

  return array(NULL, NULL);
}

function _epub_import_render_list($list) {
  list($list_level, ) = explode(':', $list[0], 2);

  if ($list_level == 'UL') {
    return _epub_import_render_unordered_list($list);
  }
  else {
    return _epub_import_render_ordered_list($list);
  }
}

function _epub_import_render_unordered_list($list) {
  $output = '<ul>';

  foreach ($list as $item) {
    if (preg_match('#^UL::#', $item, $match)) {
      $output .= '<li>' . _epub_import_clean_list_item($item) . '</li>';
    }
  }

  $output .= '</ul>';

  return $output;
}

function _epub_import_render_ordered_list($list) {
  $level = 0;

  list( , $type, ) = explode(':', $list[0], 3);
  $output = '<ol type="' . $type . '">';

  foreach ($list as $item) {
    if (preg_match('#^L\d:[A1ai]:#', $item, $match)) {
      list($list_level, $list_type) = explode(':', $item, 3);

      // Level 1
      if ($list_level == 'L1') {
        $output .= (($level == 1) ? '</li>' :
                   (($level == 2) ? '</li></ol></li>' :
                   (($level == 3) ? '</li></ol></li></ol></li>' :
                   (($level == 4) ? '</li></ol></li></ol></li></ol></li>' : ''))));

        $output .= '<li>' . _epub_import_clean_list_item($item);
        $level = 1;
      }
      // Level 2
      elseif ($list_level == 'L2') {
        $output .= (($level == 1) ? '<ol type="' . $list_type . '">' :
                   (($level == 2) ? '</li>' :
                   (($level == 3) ? '</li></ol></li>' :
                   (($level == 4) ? '</li></ol></li></ol></li>' : '' ))));

        $output .= '<li>' . _epub_import_clean_list_item($item);
        $level = 2;
      }
      // Level 3
      elseif ($list_level == 'L3') {
        $output .= (($level == 2) ? '<ol type="' . $list_type . '">' :
                   (($level == 3) ? '</li>' :
                   (($level == 4) ? '</li></ol></li>' : '')));

        $output .= '<li>' . _epub_import_clean_list_item($item);
        $level = 3;
      }
      // Level 4
      elseif ($list_level == 'L4') {
        $output .= (($level == 3) ? '<ol type="' . $list_type . '">' :
                   (($level == 4) ? '</li>' : ''));

        $output .= '<li>' . _epub_import_clean_list_item($item);
        $level = 4;
      }
    }
    // non-bullet content
    else {
      $output .= $item;
    }
  }

  $output .= (($level == 1) ? '</li>' :
             (($level == 2) ? '</li></ol></li>' :
             (($level == 3) ? '</li></ol></li></ol></li>' :
             (($level == 4) ? '</li></ol></li></ol></li></ol></li>' : ''))));

  $output .= '</ol>';

  return $output;
}

function _epub_import_clean_list_item($item) {
  // Remove level and type.
  list( , , $line) = explode(':', trim($item), 3);
  $line = str_replace("&#9;", ' ', $line);
  // Trim unnecessarily enclosing <p> tag.
  $line = preg_replace('#^<p>(.+)</p>$#', '$1', $line);

  return trim($line);
}

/**
 * Clean up parsed line.
 */
function _epub_import_clean_line($line) {
  static $except = '<table><colgroup><col><thead><tbody><th><tr><td><img><a><em><strong><p><br><span>';

  if (preg_match('#^<(p|h\d) class="(ol|ol-\d)">(<span class="[\w- ]+">)?([A-Z]|[a-z]|[1-9][0-9]?|[ivx]{1,5})\.(</span>)?(.+)</(p|h\d)>$#', $line, $match)) {
    $line = trim($match[6]);
  }
  elseif (preg_match('#^<p class="ul">(<span class="_idGenBNMarker-[\d]+">.+?</span>)?(.+)</p>$#', $line, $match)) {
    $line = trim($match[2]);
  }
  elseif (preg_match('#^<p class="[\w- ]+">(.+)</p>$#', $line, $match)) {
    $line = '<p>' . trim($match[1]) . '</p>';
  }

  $line = _epub_import_process_webdings($line);
  $line = strip_tags($line, $except);
  $line = _epub_import_strip_attrs($line);
  $line = str_replace(array('&nbsp;', '&#160;', '&#9;'), ' ', $line);

  $line = _epub_import_clean_img($line);

  return trim($line);
}

/**
 * Setter and getter for heading id currently worked on in order to keep track
 * of it from different locations in the code.
 *
 * @return string
 *   Heading in dot notation such as 1.2.3.
 */
function _epub_import_current_unit_id($heading = NULL) {
  $heading_id = &drupal_static(__FUNCTION__);

  if (!isset($heading_id)) {
    $heading_id = array('h1' => 0, 'h2' => 0, 'h3' => 0);
  }

  if ($heading and in_array($heading, array('h1', 'h2', 'h3'))) {
    $heading_id[$heading]++;

    for ($h = ++$heading; $h <= 'h3'; $h++) {
      $heading_id[$h] = 0;
    }
  }

  return join('-', array_filter($heading_id));
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
function _epub_import_parse_terms(&$parsed, $line) {
  $prefix = variable_get('epub_import_class_prefix', '');

  if (preg_match_all('#class="' . $prefix . '([\w-]+)">(.+?)<#', $line, $matches)) {
    $vocas = $matches[1];
    $texts = $matches[2];

    foreach ($vocas as $key => $voca) {
      if (!isset($parsed['terms'][$voca])) {
        $parsed['terms'][$voca] = array();
      }

      if (!in_array($texts[$key], $parsed['terms'][$voca])) {
        $parsed['terms'][$voca][] = $texts[$key];
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
  $system_path = variable_get('file_public_path', 'sites/default/files') . '/' . $image_path;

  // Ignore image with inline data.
  if (strpos($line, 'src="data:image')) {
    $line = '';
  }
  elseif (preg_match('/src="([^"]+)"/', $line, $match)) {
    $src = $match[1];
    $img_name = pathinfo($src, PATHINFO_BASENAME);
    $img_dest = $system_path . '/' . $img_name;

    if (!file_exists($system_path) && !is_dir($system_path)) {
      drupal_mkdir($system_path, 0775, TRUE);
    }

    $dest = file_unmanaged_copy("$epub_path/$src", $img_dest, FILE_EXISTS_RENAME);
    $new_name = pathinfo($dest, PATHINFO_BASENAME);
    $src  = image_style_url($image_style, "$image_path/$new_name");
    $line = preg_replace('/src="([^"]+)"/', 'src="' . $src . '" class="img-responsive"', $line);
  }

  return $line;
}
