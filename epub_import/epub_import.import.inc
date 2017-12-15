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
  $heading_id = '';

  while ($data = fgets($fp)) {
    $line = trim($data);

    $el = _epub_import_parse_element($line);

    if ($el['heading'] == 'h123') {
      _epub_import_flush_list($list_mode, $list, $parsed, $heading_id);

      $heading_id = _epub_import_current_unit_id($el['tag']);

      $parsed[$heading_id]['heading'] = $el['tag'];
      $parsed[$heading_id]['title']   = html_entity_decode(strip_tags($el['clean-line']));
      $parsed[$heading_id]['content'] = array();
      $parsed[$heading_id]['terms']   = array();
    }
    elseif ($el['heading'] == 'h456' and !$el['list']) {
      _epub_import_flush_list($list_mode, $list, $parsed, $heading_id);

      foreach (module_implements('epub_import_get_attr') as $module) {
        if ($attr = module_invoke($module, 'epub_import_get_attr', $el)) {
          $el['attr'] += $attr;
        }
      }

      $parsed[$heading_id]['content'][] = _epub_import_render_element($el);
    }
    elseif ($el['clean-line']) {
      if ($el['list'] == 'ol' or $el['list'] == 'ul') {
        $list[] = $el['list-level'] . ':' . $el['list-type'] . ':' . $el['content'];
        $list_mode = TRUE;
      }
      elseif ($list_mode) {
        $list[] = $el['clean-line'];
      }
      elseif ($el['tag'] == 'table') {
        $el['attr']['class'][] = 'table';

        $table = array(
          '<div class="table-responsive">',
          _epub_import_render_element($el)
        );
        $table_mode = TRUE;
      }
      elseif ($table_mode) {
        if ($el['tag'] == '/table') {
          $table[] = $el['clean-line'];
          $table[] = '</div>'; // .table-responsive

          $parsed[$heading_id]['content'][] = _epub_import_render_element_array($table);
          $table = array();
          $table_mode = FALSE;
        }
        elseif ($el['tag'] == 'tr') {
          $tr = array($el);
        }
        elseif ($el['tag'] == '/tr') {
          $tr[] = $el;
          $table[] = _epub_import_render_element_array($tr);
          $tr = array();
        }
        elseif (!empty($tr)) {
          // Remove unnecessary <p> in table cells.
          if ($el['tag'] == 'p') {
            $el['tag'] = '<none>';
          }
          $tr[] = $el;
        }
        else {
          $table[] = $el;
        }
      }
      elseif (!empty($heading_id)) {
        $parsed[$heading_id]['content'][] = $el['clean-line'];
      }
    }

    if ($heading_id) {
      _epub_import_parse_terms($parsed[$heading_id], $line);
    }
  }

  _epub_import_flush_list($list_mode, $list, $parsed, $heading_id);

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
      $parsed[$prev_heading_id]['content'][] = "<$h>{$parsed[$heading_id]['title']}</$h>";
      $parsed[$prev_heading_id]['content'] = array_merge($parsed[$prev_heading_id]['content'], $parsed[$heading_id]['content']);
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
    'value' => $content['content'] ? join("\n", $content['content']) : '',
    'summary' => '',
    'format' => variable_get('epub_import_text_format', 'full_html'),
  );

  if (epub_import_content_type_has_book_outline()) {
    $node->book['bid'] = $_POST['book_id'];
    $node->book['module'] = 'book';

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
  }

  _epub_import_attach_terms($node, $content);

  node_save($node);

  if (!empty($push_self)) {
    $ml_stack[$level] = $node->book['mlid'];
  }

  watchdog('epub import', node_type_get_name($node) . ' "' . $node->title . '" has been created.');
}

function _epub_import_parse_element($line) {
  if (preg_match('#^<(\w*)\b([^>]*)>?(.*?)(</\\1>|/>)?$#i', $line, $match)) {
    $el = [
      'tag'     => $match[1],
      'heading' => preg_match('#h(1|2|3)#', $match[1]) ? 'h123' :
                  (preg_match('#h(4|5|6)#', $match[1]) ? 'h456' : ''),
      'line'    => $line,
      'clean-line' => _epub_import_clean_line($line),
      'content' => _epub_import_clean_line($match[3]),
    ] + _epub_import_parse_attributes($match[2]);

    // Remove ol letter/number as it will be rendered by html.
    if ($el['list'] == 'ol') {
      if (preg_match('#^(<span [^>]+>)?([A-Za-z0-9]{1,5})\.(</span>)?#', $el['content'])) {
        $el['content'] = preg_replace('#^(<span [^>]+>)?([A-Za-z0-9]{1,5})\.(</span>)?#', '', $el['content']);
      }
    }
    elseif ($el['tag'] == 'img') {
      $el['tag'] = '<none>';
    }

    _epub_import_parse_list($el);
  }
  elseif (preg_match('#<(/\w+)>#', $line, $match)) {
    $el = [
      'tag'        => $match[1],
      'heading'    => '',
      'line'       => $line,
      'clean-line' => _epub_import_clean_line($line), // OR $line ?
      'content'    => '',
      'attr'       => array(),
      'list'       => '',
      'list-class' => '',
      'list-type'  => '',
      'list-level' => '',
    ];
  }

  return !empty($el) ? $el : NULL;
}


function _epub_import_parse_attributes($str) {
  static $allow = array('class', 'src', 'colspan', 'rowspan', 'href', 'data-toggle', 'data-type', 'data-title', 'data-content');

  static $class_prefix;

  if (!isset($class_prefix)) {
    $class_prefix = variable_get('epub_import_class_prefix', '');
  }

  $attrs = $newattrs = array();
  preg_match_all("/\s.*=(['\"]).*\\1/msiU", $str, $split, PREG_SET_ORDER);
  foreach ($split as $spl) {
    $attrs[] = $spl[0];
  }

  $list = $list_class = '';
  foreach ($attrs as $a) {
    $tmp = explode("=", trim($a), 2);
    if (trim($a) != "" && (!isset($tmp[1]) || (trim($tmp[0]) != "" && !in_array(strtolower(trim($tmp[0])), $allow)))) {
      // Do nothing and continue;
    }
    elseif (trim($tmp[0]) == 'class') {
      $classes = trim($tmp[1], ' \'\"');
      $classes = explode(' ', $classes);

      $known_classes = array_filter($classes, function ($val) use ($class_prefix, &$list, &$list_class) {
        if (preg_match('#^(ol|ul)(-\d)?$#', $val, $m)) {
          $list = $m[1];
          $list_class = $val;
          return;
        }

        return $class_prefix && strpos($val, $class_prefix) === 0;
      });
      if (!empty($known_classes)) {
        $newattrs['class'] = array_values($known_classes);
      }
    }
    else {
      $newattrs[$tmp[0]] = trim($tmp[1], ' \'\"');
    }
  }

  return array(
    'attr' => $newattrs,
    'list' => $list,
    'list-class' => $list_class,
  );
}

function _epub_import_parse_list(&$el) {
  if ($el['list'] == 'ol') {
    $text = strip_tags($el['clean-line']);
    list($bullet, ) = explode('.', $text, 2);

    $list_level = strpos($el['list-class'], '-')
                ? 'L' . explode('-', $el['list-class'])[1]
                : '';

    if (preg_match('#[A-Z]#', $bullet)) {
      $el['list-type'] = 'A';
      $el['list-level'] = $list_level ?: 'L1';
    }
    elseif (preg_match('#[1-9][0-9]?#', $bullet)) {
      $el['list-type'] = '1';
      $el['list-level'] = $list_level ?: 'L2';
    }
    elseif (preg_match('#[a-z]#', $bullet)) {
      $el['list-type'] = 'a';
      $el['list-level'] = $list_level ?: 'L3';
    }
    elseif (preg_match('#[ivx]{1,5}#', $bullet)) {
      $el['list-type'] = 'i';
      $el['list-level'] = $list_level ?: 'L4';
    }
  }
  elseif ($el['list'] == 'ul') {
    $el['list-type'] = '';
    $el['list-level'] = 'UL';
  }

  $el += array('list-type' => '', 'list-level' => '');
}

function _epub_import_flush_list(&$list_mode, &$list, &$parsed, $heading_id) {
  if ($list_mode and !empty($list)) {
    $parsed[$heading_id]['content'][] = _epub_import_render_list($list);
    $list = array();
    $list_mode = FALSE;
  }
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

function _epub_import_render_element($el) {
  // InDesign tends to use non standard self closing tags like <td />. So force closing tag.
  static $closer = array('th', 'td');

  if ($el['tag'] == '<none>') {
    return $el['clean-line'];
  }

  $output = '<' . $el['tag'];

  if (!empty($el['attr'])) {
    foreach ($el['attr'] as $key => $val) {
      if (is_array($val)) {
        $val = join(' ', $val);
      }

      $output .= " $key=\"$val\"";
    }
  }

  $content = $el['content'];

  if ($content) {
    $output .= '>' . $content . '</' . $el['tag'] . '>';
  }
  elseif (in_array($el['tag'], $closer) and $el['clean-line'] == "<{$el['tag']} />") {
    $output .= '></' . $el['tag'] . '>';
  }
  elseif (preg_match('# />$#', $el['line'])) {
    $output .= ' />';
  }
  else {
    $output .= '>';
  }

  return $output;
}

function _epub_import_render_element_array($els) {
  $output = array();

  foreach ($els as $el) {
    if (is_array($el)) {
      $output[] = _epub_import_render_element($el);
    }
    else {
      $output[] = $el;
    }
  }

  return join("\n", $output);
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
  $output = array('<ul>');

  foreach ($list as $item) {
    if (preg_match('#^UL::#', $item, $match)) {
      $output[] = '<li>' . _epub_import_clean_list_item($item) . '</li>';
    }
  }

  $output[] = '</ul>';

  return join("\n", $output);
}

function _epub_import_render_ordered_list($list) {
  $level = 0;

  list( , $type, ) = explode(':', $list[0], 3);
  $output = array('<ol type="' . $type . '">');

  foreach ($list as $item) {
    if (preg_match('#^L\d:[A1ai]:#', $item, $match)) {
      list($list_level, $list_type) = explode(':', $item, 3);

      // Level 1
      if ($list_level == 'L1') {
        $output[] = (($level == 1) ? '</li>' :
                    (($level == 2) ? '</li></ol></li>' :
                    (($level == 3) ? '</li></ol></li></ol></li>' :
                    (($level == 4) ? '</li></ol></li></ol></li></ol></li>' : ''))));

        $output[] = '<li>' . _epub_import_clean_list_item($item);
        $level = 1;
      }
      // Level 2
      elseif ($list_level == 'L2') {
        $output[] = (($level == 1) ? '<ol type="' . $list_type . '">' :
                    (($level == 2) ? '</li>' :
                    (($level == 3) ? '</li></ol></li>' :
                    (($level == 4) ? '</li></ol></li></ol></li>' : '' ))));

        $output[] = '<li>' . _epub_import_clean_list_item($item);
        $level = 2;
      }
      // Level 3
      elseif ($list_level == 'L3') {
        $output[] = (($level == 2) ? '<ol type="' . $list_type . '">' :
                    (($level == 3) ? '</li>' :
                    (($level == 4) ? '</li></ol></li>' : '')));

        $output[] = '<li>' . _epub_import_clean_list_item($item);
        $level = 3;
      }
      // Level 4
      elseif ($list_level == 'L4') {
        $output[] = (($level == 3) ? '<ol type="' . $list_type . '">' :
                   (($level == 4) ? '</li>' : ''));

        $output[] = '<li>' . _epub_import_clean_list_item($item);
        $level = 4;
      }
    }
    // non-bullet content
    else {
      $output[] = $item;
    }
  }

  $output[] = (($level == 1) ? '</li>' :
              (($level == 2) ? '</li></ol></li>' :
              (($level == 3) ? '</li></ol></li></ol></li>' :
              (($level == 4) ? '</li></ol></li></ol></li></ol></li>' : ''))));

  $output[] = '</ol>';

  return join("\n", $output);
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
  static $allowed = '<table><colgroup><col><thead><tbody><th><tr><td><img><a><em><strong><p><br>';

  $line = _epub_import_process_webdings($line);
  $line = strip_tags($line, $allowed);
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
  static $prefix = 'field';

  if (preg_match_all('#class="' . $prefix . '[-_]([\w-]+)">(.+?)<#', $line, $matches)) {
    $vocas = $matches[1];
    $texts = $matches[2];

    foreach ($vocas as $key => $voca) {
      $field = $prefix . '_' . $voca;
      if (!isset($parsed['terms'][$field])) {
        $parsed['terms'][$field] = [];
      }

      if (!in_array($text = trim($texts[$key]), $parsed['terms'][$field])) {
        $parsed['terms'][$field][] = $text;
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
 */
function _epub_import_strip_attrs($str) {
  static $allow = array('class', 'src', 'colspan', 'rowspan', 'href', 'data-toggle', 'data-type', 'data-title', 'data-content');
  static $class_prefix;

  if (!isset($class_prefix)) {
    $class_prefix = variable_get('epub_import_class_prefix', '');
  }

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

      }
      elseif (trim($tmp[0]) == 'class') {
        $classes = trim($tmp[1], ' \'\"');
        $classes = explode(' ', $classes);
        $known_classes = array_filter($classes, function ($val) use ($class_prefix) {
          return $class_prefix && strpos($val, $class_prefix) === 0;
        });
        if (!empty($known_classes)) {
          $newattrs[] = 'class="' . join(' ', $known_classes) . '"';
        }
      }
      else {
        $newattrs[] = trim($a);
      }
     }
     $attrs = implode(" ", $newattrs);
     $rpl = str_replace($r[1], $attrs, $tag);
     $rpl = preg_replace('/ >$/', '>', $rpl);
     $str = str_replace($tag, $rpl, $str);
   }
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
