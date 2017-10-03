<?php

/**
 * @file
 * Epub Import blocks.
 */

/**
 * Implements hook_block_info().
 */
function epub_import_block_info() {
  $blocks = array();

  $blocks['epub_import_import_form'] = array(
    'info' => t('Epub Import: Import form'),
  );

  return $blocks;
}

/**
 * Implements hook_block_view().
 */
function epub_import_block_view($delta = '') {
  $function = $delta . '_block';
  if (function_exists($function)) {
    return $function();
  }
}

function epub_import_import_form_block() {
  $form = drupal_get_form('epub_import_import_form');
  $content = drupal_render($form);

  return array('subject' => 'Epub Import', 'content' => $content);
}
