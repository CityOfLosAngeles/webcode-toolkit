<?php
/**
 * @file
 * bookshow.pages_default.inc
 */

/**
 * Implements hook_default_page_manager_handlers().
 */
function bookshow_default_page_manager_handlers() {
  $export = array();

  $handler = new stdClass();
  $handler->disabled = FALSE; /* Edit this to true to make a default handler disabled initially */
  $handler->api_version = 1;
  $handler->name = 'node_view__panel_context_a3fc2d34-ab6f-4fc6-a6f5-4d1dff5ce007';
  $handler->task = 'node_view';
  $handler->subtask = '';
  $handler->handler = 'panel_context';
  $handler->weight = 0;
  $handler->conf = array(
    'title' => 'Book',
    'no_blocks' => 1,
    'pipeline' => 'standard',
    'body_classes_to_remove' => '',
    'body_classes_to_add' => '',
    'css_id' => '',
    'css' => '',
    'contexts' => array(),
    'relationships' => array(),
    'name' => '',
    'access' => array(
      'plugins' => array(
        0 => array(
          'name' => 'node_type',
          'settings' => array(
            'type' => array(
              'book' => 'book',
            ),
          ),
          'context' => 'argument_entity_id:node_1',
          'not' => FALSE,
        ),
      ),
      'logic' => 'and',
    ),
  );
  $display = new panels_display();
  $display->layout = 'twocol_stacked';
  $display->layout_settings = array();
  $display->panel_settings = array(
    'style_settings' => array(
      'default' => NULL,
      'top' => NULL,
      'left' => NULL,
      'right' => NULL,
      'bottom' => NULL,
    ),
  );
  $display->cache = array();
  $display->title = '';
  $display->uuid = 'ecec6fb1-56d8-4c64-8822-d89cfd4768a5';
  $display->storage_type = 'page_manager';
  $display->storage_id = 'node_view__panel_context_a3fc2d34-ab6f-4fc6-a6f5-4d1dff5ce007';
  $display->content = array();
  $display->panels = array();
  $pane = new stdClass();
  $pane->pid = 'new-a6e5b3c3-87cb-4032-a7b5-e04a59a8975c';
  $pane->panel = 'left';
  $pane->type = 'block';
  $pane->subtype = 'book-navigation';
  $pane->shown = TRUE;
  $pane->access = array();
  $pane->configuration = array(
    'override_title' => 0,
    'override_title_text' => '',
    'override_title_heading' => 'h2',
  );
  $pane->cache = array();
  $pane->style = array(
    'settings' => NULL,
  );
  $pane->css = array();
  $pane->extras = array();
  $pane->position = 0;
  $pane->locks = array();
  $pane->uuid = 'a6e5b3c3-87cb-4032-a7b5-e04a59a8975c';
  $display->content['new-a6e5b3c3-87cb-4032-a7b5-e04a59a8975c'] = $pane;
  $display->panels['left'][0] = 'new-a6e5b3c3-87cb-4032-a7b5-e04a59a8975c';
  $pane = new stdClass();
  $pane->pid = 'new-a5875f17-6bfb-4b45-88bb-4f2524e49298';
  $pane->panel = 'left';
  $pane->type = 'node_book_nav';
  $pane->subtype = 'node_book_nav';
  $pane->shown = TRUE;
  $pane->access = array();
  $pane->configuration = array(
    'context' => 'argument_entity_id:node_1',
    'override_title' => 0,
    'override_title_text' => '',
    'override_title_heading' => 'h2',
  );
  $pane->cache = array();
  $pane->style = array(
    'settings' => NULL,
  );
  $pane->css = array();
  $pane->extras = array();
  $pane->position = 1;
  $pane->locks = array();
  $pane->uuid = 'a5875f17-6bfb-4b45-88bb-4f2524e49298';
  $display->content['new-a5875f17-6bfb-4b45-88bb-4f2524e49298'] = $pane;
  $display->panels['left'][1] = 'new-a5875f17-6bfb-4b45-88bb-4f2524e49298';
  $pane = new stdClass();
  $pane->pid = 'new-1857983c-78ca-460a-80d2-fbfccb42e7e8';
  $pane->panel = 'right';
  $pane->type = 'node_body';
  $pane->subtype = 'node_body';
  $pane->shown = TRUE;
  $pane->access = array();
  $pane->configuration = array(
    'context' => 'argument_entity_id:node_1',
    'override_title' => 1,
    'override_title_text' => '',
    'override_title_heading' => 'h2',
  );
  $pane->cache = array();
  $pane->style = array(
    'settings' => NULL,
  );
  $pane->css = array();
  $pane->extras = array();
  $pane->position = 0;
  $pane->locks = array();
  $pane->uuid = '1857983c-78ca-460a-80d2-fbfccb42e7e8';
  $display->content['new-1857983c-78ca-460a-80d2-fbfccb42e7e8'] = $pane;
  $display->panels['right'][0] = 'new-1857983c-78ca-460a-80d2-fbfccb42e7e8';
  $display->hide_title = PANELS_TITLE_FIXED;
  $display->title_pane = 'new-1857983c-78ca-460a-80d2-fbfccb42e7e8';
  $handler->conf['display'] = $display;
  $export['node_view__panel_context_a3fc2d34-ab6f-4fc6-a6f5-4d1dff5ce007'] = $handler;

  return $export;
}
