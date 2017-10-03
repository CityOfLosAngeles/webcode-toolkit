<?php

/**
 * @file
 * Epub Import admin settings.
 */

function epub_import_admin_settings() {
  // Fieldset: Content
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $types = node_type_get_names();
  $allowed = variable_get('book_allowed_types', array('book'));
  $types = array_filter($types, function ($val) use ($allowed) {
    return in_array($val, $allowed);
  }, ARRAY_FILTER_USE_KEY);
  $form['content']['epub_import_node_type'] = array(
    '#type' => 'radios',
    '#title' => t('Import Epub as'),
    '#default_value' => variable_get('epub_import_node_type', ''),
    '#options' => $types,
    '#description' => t('You can enable !books to other content types.', array('!books' => l('book outlines', 'admin/content/book/settings'))),
    '#required' => TRUE,
  );

  $text_formats = array();
  foreach(filter_formats() as $ff) {
    $text_formats[$ff->format] = $ff->name;
  }

  $form['content']['epub_import_text_format'] = array(
    '#type' => 'radios',
    '#title' => t('Text format'),
    '#description' => t('Contents will be created using the text format.'),
    '#default_value' => variable_get('epub_import_text_format', 'full_html'),
    '#options' => $text_formats,
    '#description' => t('Contents will be created using the text format.'),
    '#required' => TRUE,
  );

  // Fieldset: Images
  $form['images'] = array(
    '#type' => 'fieldset',
    '#title' => t('Images'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $image_styles = array();
  foreach (image_styles() as $is) {
    $image_styles[$is['name']] = $is['label'];
  }

  $form['images']['epub_import_image_style'] = array(
    '#type' => 'radios',
    '#title' => t('Image style'),
    '#description' => t('Images in the content will show using the image style.'),
    '#default_value' => variable_get('epub_import_image_style', ''),
    '#options' => $image_styles,
    '#required' => TRUE,
  );

  $form['images']['epub_import_image_folder'] = array(
    '#type' => 'textfield',
    '#title' => t('Images folder'),
    '#description' => t('Images from Epub will be saved under this folder. e.g. /sites/default/files/') . variable_get('epub_import_image_folder', 'epub_images'),
    '#default_value' => variable_get('epub_import_image_folder', 'epub_images'),
  );

  // Fieldset: Taxonomy
  $form['taxonomy'] = array(
    '#type' => 'fieldset',
    '#title' => t('Taxonomy'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
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

  $form['taxonomy']['epub_import_terms'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Taxonomy vocabulary'),
    '#description' => t('Tagged text in Epub are recognized as terms associated with the node.'),
    '#default_value' => variable_get('epub_import_terms', array()),
    '#options' => $term_fields,
  );


  $form['taxonomy']['epub_import_class_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Class name prefix'),
    '#description' => t('Class name prefix used in the Epub file (e.g. taxonomy_field_tags).'),
    '#default_value' => variable_get('epub_import_class_prefix', ''),
    '#required' => TRUE,
  );

  return system_settings_form($form);
}
