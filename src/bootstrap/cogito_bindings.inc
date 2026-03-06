// ---- Cogito bindings (shared library) ----
#include <cogito.h>

#undef cogito_about_window_new
#undef cogito_about_window_set_description
#undef cogito_about_window_set_icon
#undef cogito_about_window_set_issue_url
#undef cogito_about_window_set_website
#undef cogito_active_indicator_new
#undef cogito_app_free
#undef cogito_app_get_icon
#undef cogito_app_new
#undef cogito_app_run
#undef cogito_app_copy_to_clipboard
#undef cogito_app_set_accent_color
#undef cogito_app_set_dark_mode
#undef cogito_app_set_accent_from_image
#undef cogito_app_set_app_name
#undef cogito_app_set_appid
#undef cogito_app_set_contrast
#undef cogito_app_set_ensor_variant
#undef cogito_app_set_icon
#undef cogito_appbar_add_button
#undef cogito_appbar_new
#undef cogito_appbar_set_controls
#undef cogito_appbar_set_subtitle
#undef cogito_appbar_set_title
#undef cogito_toolbar_new
#undef cogito_button_add_menu
#undef cogito_button_add_menu_section
#undef cogito_button_get_size
#undef cogito_button_new
#undef cogito_button_set_size
#undef cogito_button_set_text
#undef cogito_carousel_item_set_halign
#undef cogito_carousel_item_set_text
#undef cogito_carousel_item_set_valign
#undef cogito_checkbox_get_checked
#undef cogito_checkbox_new
#undef cogito_checkbox_on_change
#undef cogito_checkbox_set_checked
#undef cogito_colorpicker_new
#undef cogito_colorpicker_on_change
#undef cogito_colorpicker_set_hex
#undef cogito_colorpicker_get_hex
#undef cogito_content_list_new
#undef cogito_datepicker_new
#undef cogito_datepicker_on_change
#undef cogito_dialog_new
#undef cogito_dialog_slot_clear
#undef cogito_dialog_slot_new
#undef cogito_dialog_slot_show
#undef cogito_dropdown_get_selected
#undef cogito_dropdown_new
#undef cogito_dropdown_on_change
#undef cogito_dropdown_set_items
#undef cogito_dropdown_set_selected
#undef cogito_empty_page_new
#undef cogito_empty_page_set_action
#undef cogito_empty_page_set_description
#undef cogito_empty_page_set_icon
#undef cogito_find_children
#undef cogito_find_parent
#undef cogito_fixed_set_pos
#undef cogito_grid_new_with_cols
#undef cogito_grid_on_activate
#undef cogito_grid_on_select
#undef cogito_grid_set_align
#undef cogito_grid_set_gap
#undef cogito_grid_set_span
#undef cogito_iconbtn_add_menu
#undef cogito_iconbtn_add_menu_section
#undef cogito_iconbtn_new
#undef cogito_image_new
#undef cogito_image_set_icon
#undef cogito_webview_new
#undef cogito_webview_set_url
#undef cogito_webview_get_url
#undef cogito_webview_set_open_external_on_click
#undef cogito_webview_get_open_external_on_click
#undef cogito_webview_open
#undef cogito_drawing_area_new
#undef cogito_drawing_area_on_press
#undef cogito_drawing_area_on_drag
#undef cogito_drawing_area_on_release
#undef cogito_drawing_area_get_x
#undef cogito_drawing_area_get_y
#undef cogito_drawing_area_get_pressed
#undef cogito_drawing_area_clear
#undef cogito_drawing_area_on_draw
#undef cogito_canvas_set_color
#undef cogito_canvas_set_line_width
#undef cogito_canvas_line
#undef cogito_canvas_rect
#undef cogito_canvas_fill_rect
#undef cogito_shape_new
#undef cogito_shape_set_preset
#undef cogito_shape_get_preset
#undef cogito_shape_set_size
#undef cogito_shape_get_size
#undef cogito_shape_set_color
#undef cogito_shape_set_color_style
#undef cogito_shape_get_color_style
#undef cogito_shape_set_vertex
#undef cogito_shape_get_vertex_x
#undef cogito_shape_get_vertex_y
#undef cogito_timer_set_timeout
#undef cogito_timer_set_interval
#undef cogito_timer_set_timeout_ex
#undef cogito_timer_set_interval_ex
#undef cogito_timer_set_timeout_for
#undef cogito_timer_set_interval_for
#undef cogito_timer_set_timeout_for_ex
#undef cogito_timer_set_interval_for_ex
#undef cogito_timer_clear
#undef cogito_timer_clear_for
#undef cogito_timer_clear_all
#undef cogito_label_new
#undef cogito_label_set_align
#undef cogito_label_set_ellipsis
#undef cogito_label_set_text
#undef cogito_label_set_wrap
#undef cogito_list_on_activate
#undef cogito_list_on_select
#undef cogito_load_sum_file
#undef cogito_menu_button_new
#undef cogito_menu_set_icon
#undef cogito_menu_set_shortcut
#undef cogito_menu_set_submenu
#undef cogito_menu_set_toggled
#undef cogito_node_add
#undef cogito_node_build
#undef cogito_node_free
#undef cogito_node_get_editable
#undef cogito_node_get_text
#undef cogito_node_new
#undef cogito_node_on_activate
#undef cogito_node_on_change
#undef cogito_node_on_click
#undef cogito_node_on_select
#undef cogito_node_remove
#undef cogito_node_set_a11y_label
#undef cogito_node_set_a11y_role
#undef cogito_node_set_align
#undef cogito_node_set_class
#undef cogito_node_set_disabled
#undef cogito_node_set_editable
#undef cogito_node_set_halign
#undef cogito_node_set_id
#undef cogito_node_set_margins
#undef cogito_node_set_padding
#undef cogito_node_set_text
#undef cogito_node_set_tooltip
#undef cogito_node_set_valign
#undef cogito_node_window
#undef cogito_open_url
#undef cogito_gst_init
#undef cogito_gst_shutdown
#undef cogito_gst_load
#undef cogito_gst_play
#undef cogito_gst_pause
#undef cogito_gst_stop
#undef cogito_gst_seek_ms
#undef cogito_gst_get_position_ms
#undef cogito_gst_get_duration_ms
#undef cogito_gst_set_volume
#undef cogito_gst_get_volume
#undef cogito_gst_poll_event
#undef cogito_gst_last_error
#undef cogito_gst_last_cover_path
#undef cogito_pointer_capture
#undef cogito_pointer_release
#undef cogito_progress_get_circular
#undef cogito_progress_get_indeterminate
#undef cogito_progress_get_thickness
#undef cogito_progress_get_value
#undef cogito_progress_get_wavy
#undef cogito_progress_new
#undef cogito_progress_set_circular
#undef cogito_progress_set_indeterminate
#undef cogito_progress_set_thickness
#undef cogito_progress_set_value
#undef cogito_progress_set_wavy
#undef cogito_rebuild_active_window
#undef cogito_scroller_set_axes
#undef cogito_scroller_set_sync
#undef cogito_searchfield_get_text
#undef cogito_searchfield_new
#undef cogito_searchfield_on_change
#undef cogito_searchfield_set_text
#undef cogito_buttongroup_new
#undef cogito_buttongroup_on_select
#undef cogito_buttongroup_set_size
#undef cogito_buttongroup_get_size
#undef cogito_buttongroup_set_shape
#undef cogito_buttongroup_get_shape
#undef cogito_buttongroup_set_connected
#undef cogito_buttongroup_get_connected
#undef cogito_settings_list_new
#undef cogito_settings_page_new
#undef cogito_settings_row_new
#undef cogito_settings_window_new
#undef cogito_slider_get_centered
#undef cogito_slider_get_range_end
#undef cogito_slider_get_range_start
#undef cogito_slider_get_size
#undef cogito_slider_get_value
#undef cogito_slider_new
#undef cogito_slider_on_change
#undef cogito_slider_range_new
#undef cogito_slider_set_centered
#undef cogito_slider_set_icon
#undef cogito_slider_set_range
#undef cogito_slider_set_range_end
#undef cogito_slider_set_range_start
#undef cogito_slider_set_size
#undef cogito_slider_set_value
#undef cogito_split_button_add_menu
#undef cogito_split_button_add_menu_section
#undef cogito_split_button_new
#undef cogito_split_button_set_size
#undef cogito_split_button_set_variant
#undef cogito_stepper_get_value
#undef cogito_stepper_new
#undef cogito_stepper_on_change
#undef cogito_stepper_set_value
#undef cogito_switch_get_checked
#undef cogito_switch_new
#undef cogito_switch_on_change
#undef cogito_switch_set_checked
#undef cogito_switchbar_get_checked
#undef cogito_switchbar_new
#undef cogito_switchbar_on_change
#undef cogito_switchbar_set_checked
#undef cogito_tabs_bind
#undef cogito_tabs_get_selected
#undef cogito_tabs_new
#undef cogito_tabs_on_change
#undef cogito_tabs_set_ids
#undef cogito_tabs_set_items
#undef cogito_tabs_set_selected
#undef cogito_textfield_get_hint
#undef cogito_textfield_get_text
#undef cogito_textfield_new
#undef cogito_textfield_on_change
#undef cogito_textfield_set_hint
#undef cogito_textfield_set_text
#undef cogito_textview_get_text
#undef cogito_textview_new
#undef cogito_textview_on_change
#undef cogito_textview_set_text
#undef cogito_tip_view_new
#undef cogito_tip_view_set_title
#undef cogito_toast_new
#undef cogito_toast_on_click
#undef cogito_toast_set_action
#undef cogito_toasts_new
#undef cogito_treeview_new
#undef cogito_view_chooser_bind
#undef cogito_view_chooser_new
#undef cogito_view_chooser_set_items
#undef cogito_view_dual_new
#undef cogito_view_dual_set_ratio
#undef cogito_view_switcher_new
#undef cogito_view_switcher_set_active
#undef cogito_welcome_screen_new
#undef cogito_welcome_screen_set_action
#undef cogito_welcome_screen_set_description
#undef cogito_welcome_screen_set_icon
#undef cogito_window_clear_dialog
#undef cogito_window_free
#undef cogito_window_new
#undef cogito_window_set_a11y_label
#undef cogito_window_set_autosize
#undef cogito_window_set_builder
#undef cogito_window_set_dialog
#undef cogito_window_set_side_sheet
#undef cogito_side_sheet_set_mode
#undef cogito_window_clear_side_sheet
#undef cogito_window_set_resizable
#undef cogito_fab_menu_set_size
#undef cogito_fab_menu_set_color

static bool cogito_debug_enabled(void) {
  const char* env = getenv("COGITO_DEBUG");
  return env && env[0] && env[0] != '0';
}

typedef enum {
  COGITO_HANDLE_APP = 1,
  COGITO_HANDLE_WINDOW,
  COGITO_HANDLE_NODE,
  COGITO_HANDLE_STATE
} CogitoHandleKind;

typedef struct CogitoHandle {
  YisObj base;
  void* ptr;
  int kind;
  YisVal on_click;
  YisVal on_change;
  YisVal on_select;
  YisVal on_activate;
  YisVal on_action;
  YisVal on_draw;
  YisVal builder;
} CogitoHandle;

typedef struct CogitoHandleEntry {
  cogito_node* node;
  CogitoHandle* handle;
  struct CogitoHandleEntry* next;
} CogitoHandleEntry;

typedef struct CogitoMenuHandler {
  YisVal fn;
} CogitoMenuHandler;

static void __cogito_button_on_click(YisVal btnv, YisVal handler);

// Forward declaration for internal YisVal-based function
extern void cogito_view_switcher_add_lazy_yis(cogito_node* view_switcher, YisVal id, YisVal builder);

typedef struct CogitoState {
  YisObj base;
  YisVal value;
} CogitoState;

static CogitoHandleEntry* cogito_handle_entries = NULL;

static CogitoHandle* cogito_handle_lookup(cogito_node* node) {
  for (CogitoHandleEntry* e = cogito_handle_entries; e; e = e->next) {
    if (e->node == node) return e->handle;
  }
  return NULL;
}

static void cogito_handle_register(cogito_node* node, CogitoHandle* handle) {
  CogitoHandleEntry* e = (CogitoHandleEntry*)malloc(sizeof(*e));
  e->node = node;
  e->handle = handle;
  e->next = cogito_handle_entries;
  cogito_handle_entries = e;
}

static void cogito_handle_unregister(cogito_node* node) {
  CogitoHandleEntry** cur = &cogito_handle_entries;
  while (*cur) {
    CogitoHandleEntry* e = *cur;
    if (e->node == node) {
      *cur = e->next;
      free(e);
      return;
    }
    cur = &e->next;
  }
}

static void cogito_handle_drop(YisObj* o) {
  CogitoHandle* h = (CogitoHandle*)o;
  if (!h) return;
  if (h->on_click.tag != EVT_NULL) yis_release_val(h->on_click);
  if (h->on_change.tag != EVT_NULL) yis_release_val(h->on_change);
  if (h->on_select.tag != EVT_NULL) yis_release_val(h->on_select);
  if (h->on_activate.tag != EVT_NULL) yis_release_val(h->on_activate);
  if (h->on_action.tag != EVT_NULL) yis_release_val(h->on_action);
  if (h->on_draw.tag != EVT_NULL) yis_release_val(h->on_draw);
  if (h->builder.tag != EVT_NULL) yis_release_val(h->builder);
  if (h->kind == COGITO_HANDLE_WINDOW || h->kind == COGITO_HANDLE_NODE) {
    cogito_handle_unregister((cogito_node*)h->ptr);
  }
  h->ptr = NULL;
}

static CogitoHandle* cogito_handle_new(void* ptr, int kind) {
  CogitoHandle* h = (CogitoHandle*)yis_obj_new(sizeof(CogitoHandle), cogito_handle_drop);
  h->ptr = ptr;
  h->kind = kind;
  h->on_click = YV_NULLV;
  h->on_change = YV_NULLV;
  h->on_select = YV_NULLV;
  h->on_activate = YV_NULLV;
  h->on_action = YV_NULLV;
  h->on_draw = YV_NULLV;
  h->builder = YV_NULLV;
  return h;
}

static YisVal cogito_wrap_node(cogito_node* node, int kind) {
  if (!node) return YV_NULLV;
  CogitoHandle* h = cogito_handle_lookup(node);
  if (!h) {
    h = cogito_handle_new(node, kind);
    cogito_handle_register(node, h);
  }
  return YV_OBJ(h);
}

static CogitoHandle* cogito_handle_from_val(YisVal v, const char* what) {
  if (v.tag != EVT_OBJ) yis_trap(what);
  return (CogitoHandle*)v.as.p;
}

static cogito_app* cogito_app_from_val(YisVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.app expects app");
  return (cogito_app*)h->ptr;
}

static cogito_window* cogito_window_from_val(YisVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.window expects window");
  return (cogito_window*)h->ptr;
}

static cogito_node* cogito_node_from_val(YisVal v) {
  CogitoHandle* h = cogito_handle_from_val(v,
  "cogito.node expects node");
  return (cogito_node*)h->ptr;
}
// Non-static version for use by view_switcher.inc
cogito_node* cogito_unwrap_handle(YisVal v) {
  if (v.tag != EVT_OBJ) return NULL;
  CogitoHandle* h = (CogitoHandle*)v.as.p;
  return (cogito_node*)h->ptr;
}

static const char* cogito_required_cstr(YisVal v, YisStr** tmp) {
  if (v.tag == EVT_NULL) return "";
  if (v.tag == EVT_STR) return ((YisStr*)v.as.p)->data;
  YisStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : "";
}

static const char* cogito_optional_cstr(YisVal v, YisStr** tmp) {
  if (v.tag == EVT_NULL) return NULL;
  if (v.tag == EVT_STR) return ((YisStr*)v.as.p)->data;
  YisStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : NULL;
}

static void cogito_set_handler(CogitoHandle* h, YisVal* slot, YisVal handler) {
  bool had = slot->tag != EVT_NULL;
  if (had) yis_release_val(*slot);
  *slot = handler;
  bool has = handler.tag != EVT_NULL;
  if (has) yis_retain_val(handler);
  if (h) {
    if (!had && has) yis_retain_val(YV_OBJ(h));
    if (had && !has) yis_release_val(YV_OBJ(h));
  }
}

static void cogito_invoke_node_handler(YisVal handler, cogito_node* node) {
  if (handler.tag != EVT_FN) return;
  YisVal arg = cogito_wrap_node(node, COGITO_HANDLE_NODE);
  yis_retain_val(arg);
  YisVal ret = yis_call(handler,
  1, &arg);
  yis_release_val(arg);
  yis_release_val(ret);
}

static void cogito_invoke_index_handler(YisVal handler, int idx) {
  if (handler.tag != EVT_FN) return;
  YisVal arg = YV_INT(idx);
  YisVal ret = yis_call(handler,
  1, &arg);
  yis_release_val(ret);
}

static void cogito_invoke_draw_handler(YisVal handler, cogito_node* node,
                                       int width, int height) {
  if (handler.tag != EVT_FN) return;
  YisVal args[3] = {cogito_wrap_node(node, COGITO_HANDLE_NODE), YV_INT(width),
                     YV_INT(height)};
  yis_retain_val(args[0]);
  YisVal ret = yis_call(handler, 3, args);
  yis_release_val(args[0]);
  yis_release_val(ret);
}

static void cogito_cb_click(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_click, node);
}

static void cogito_cb_change(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_change, node);
}

static void cogito_cb_action(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_action, node);
}

static void cogito_cb_draw(cogito_node* node, int width, int height, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_draw_handler(h->on_draw, node, width, height);
}

static void cogito_cb_select(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_select, idx);
}

static void cogito_cb_activate(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_activate, idx);
}

static void cogito_cb_builder(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->builder, node);
}

static CogitoMenuHandler* cogito_menu_handler_new(YisVal handler) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)calloc(1, sizeof(*mh));
  mh->fn = handler;
  if (handler.tag != EVT_NULL) yis_retain_val(handler);
  return mh;
}

static void cogito_cb_menu(cogito_node* node, void* user) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)user;
  if (!mh) return;
  cogito_invoke_node_handler(mh->fn, node);
}

static void cogito_state_drop(YisObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    yis_release_val(s->value);
    s->value = YV_NULLV;
  }
}

static YisVal cogito_state_new_val(YisVal initial) {
  CogitoState* s = (CogitoState*)yis_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) yis_retain_val(initial);
  return YV_OBJ(s);
}

static YisVal cogito_state_get_val(YisVal sv) {
  if (sv.tag != EVT_OBJ) yis_trap("cogito.state_get expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  YisVal v = s->value;
  if (v.tag != EVT_NULL) yis_retain_val(v);
  return v;
}

static void cogito_state_set_val(YisVal sv, YisVal nv) {
  if (sv.tag != EVT_OBJ) yis_trap("cogito.state_set expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  if (s->value.tag != EVT_NULL) yis_release_val(s->value);
  s->value = nv;
  if (nv.tag != EVT_NULL) yis_retain_val(nv);
  cogito_rebuild_active_window();
}

static YisVal __cogito_app(void) {
  cogito_app* app = cogito_app_new();
  CogitoHandle* h = cogito_handle_new(app, COGITO_HANDLE_APP);
  return YV_OBJ(h);
}

static void __cogito_app_set_appid(YisVal appv, YisVal idv) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_app_set_appid(app, id);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_app_set_app_name(YisVal appv, YisVal namev) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* name = cogito_optional_cstr(namev, &tmp);
  if (name) cogito_app_set_app_name(app, name);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_app_set_accent_color(YisVal appv, YisVal colorv, YisVal overridev) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* color = cogito_optional_cstr(colorv, &tmp);
  bool ov = overridev.tag == EVT_BOOL ? overridev.as.b : false;
  if (color) cogito_app_set_accent_color(app, color, ov);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_app_set_dark_mode(YisVal appv, YisVal darkv, YisVal follow_systemv) {
  cogito_app* app = cogito_app_from_val(appv);
  bool dark = darkv.tag == EVT_BOOL ? darkv.as.b : yis_as_bool(darkv);
  bool follow_system = follow_systemv.tag == EVT_BOOL ? follow_systemv.as.b : false;
  if (app) cogito_app_set_dark_mode(app, dark, follow_system);
}

static YisVal __cogito_app_set_accent_from_image(YisVal appv, YisVal pathv, YisVal follow_systemv) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* path = cogito_optional_cstr(pathv, &tmp);
  bool follow_system = follow_systemv.tag == EVT_BOOL ? follow_systemv.as.b : false;
  const char* hex = (app && path && path[0]) ? cogito_app_set_accent_from_image(app, path, follow_system) : NULL;
  if (tmp) yis_release_val(YV_STR(tmp));
  if (!hex || !hex[0]) return YV_NULLV;
  return YV_STR(stdr_str_lit(hex));
}

static void __cogito_app_set_ensor_variant(YisVal appv, YisVal variantv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) {
    cogito_app_set_ensor_variant(app, (int)yis_as_int(variantv));
  }
}

static void __cogito_app_set_contrast(YisVal appv, YisVal contrastv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) {
    cogito_app_set_contrast(app, yis_as_float(contrastv));
  }
}

static void __cogito_app_set_icon(YisVal appv, YisVal pathv) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* path = cogito_optional_cstr(pathv, &tmp);
  if (path) cogito_app_set_icon(app, path);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_app_get_icon(YisVal appv) {
  cogito_app* app = cogito_app_from_val(appv);
  const char* path = cogito_app_get_icon(app);
  return YV_STR(stdr_str_lit(path ? path : ""));
}

static YisVal __cogito_app_copy_to_clipboard(YisVal appv, YisVal textv) {
  cogito_app* app = cogito_app_from_val(appv);
  YisStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  bool ok = false;
  if (app && text) ok = cogito_app_copy_to_clipboard(app, text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __cogito_window(YisVal titlev, YisVal wv, YisVal hv) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  int w = (int)yis_as_int(wv);
  int h = (int)yis_as_int(hv);
  cogito_window* win = cogito_window_new(title, w, h);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static void __cogito_window_set_resizable(YisVal winv, YisVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_resizable(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_autosize(YisVal winv, YisVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_autosize(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_a11y_label(YisVal winv, YisVal labelv) {
  cogito_window* win = cogito_window_from_val(winv);
  YisStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_window_set_a11y_label(win, label);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_window_set_builder(YisVal winv, YisVal builder) {
  cogito_window* win = cogito_window_from_val(winv);
  CogitoHandle* h = (CogitoHandle*)winv.as.p;
  if (builder.tag != EVT_FN) {
    cogito_set_handler(h, &h->builder, builder);
    cogito_window_set_builder(win, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->builder, builder);
  cogito_window_set_builder(win, cogito_cb_builder, h);
}

static YisVal __cogito_label(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_label_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_button(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_button_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_iconbtn(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_iconbtn_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_fab(YisVal iconv) {
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_fab_new(icon);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

// Forward declarations for FAB Menu
struct CogitoNode* cogito_fab_menu_new(const char* icon);
extern void cogito_fab_menu_add_item(YisVal fab_menuv, YisVal iconv, YisVal action);

static void __cogito_fab_set_size(YisVal fabv, YisVal sizev) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int size = (int)yis_as_int(sizev);
  if (size < 0) size = 0;
  if (size > 2) size = 2;
  cogito_fab_set_size(fab, size);
}

static void __cogito_fab_set_color(YisVal fabv, YisVal colorv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int color = (int)yis_as_int(colorv);
  if (color < 0) color = 0;
  if (color > 3) color = 3;
  cogito_fab_set_color(fab, color);
}

static void __cogito_fab_menu_set_color(YisVal fabv, YisVal colorv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  int color = (int)yis_as_int(colorv);
  if (color < 0) color = 0;
  if (color > 3) color = 3;
  cogito_fab_menu_set_color(fab, color);
}

static void __cogito_fab_set_extended(YisVal fabv, YisVal extendedv, YisVal labelv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  bool extended = yis_as_bool(extendedv);
  YisStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  cogito_fab_set_extended(fab, extended, label ? label : "");
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_fab_on_click(YisVal fabv, YisVal handler) {
  cogito_node* fab = cogito_node_from_val(fabv);
  CogitoHandle* h = (CogitoHandle*)fabv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_fab_on_click(fab, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_fab_on_click(fab, cogito_cb_click, h);
}

static YisVal __cogito_fab_menu(YisVal iconv) {
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_fab_menu_new(icon);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_fab_menu_add_item(YisVal fabv, YisVal iconv, YisVal actionv) {
  cogito_node* node = cogito_node_from_val(fabv);
  cogito_fab_menu_add_item(YV_OBJ(node), iconv, actionv);
}

static YisVal __cogito_chip(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_chip_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_chip_set_selected(YisVal chipv, YisVal sel) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_selected(n, yis_as_bool(sel));
}

static YisVal __cogito_chip_get_selected(YisVal chipv) {
  cogito_node* n = cogito_node_from_val(chipv);
  return YV_BOOL(cogito_chip_get_selected(n));
}

static void __cogito_chip_set_closable(YisVal chipv, YisVal closable) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_closable(n, yis_as_bool(closable));
}

static void __cogito_chip_on_click(YisVal chipv, YisVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_chip_on_click(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_chip_on_click(chip, cogito_cb_click, h);
}

static void __cogito_chip_on_close(YisVal chipv, YisVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_chip_on_close(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_chip_on_close(chip, cogito_cb_action, h);
}
// --- Divider ---
static YisVal __cogito_divider(YisVal orientationv, YisVal insetv) {
  YisStr* tmp = NULL;
  const char* orientation = NULL;
  if (orientationv.tag != EVT_NULL) {
    orientation = cogito_required_cstr(orientationv, &tmp);
  }
  bool inset = yis_as_bool(insetv);
  cogito_node* n = cogito_divider_new(orientation, inset);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- Card ---
static YisVal __cogito_card(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_card_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- Avatar ---
static YisVal __cogito_avatar(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = NULL;
  if (textv.tag != EVT_NULL) {
    text = cogito_required_cstr(textv, &tmp);
  }
  cogito_node* n = cogito_avatar_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_avatar_set_image(YisVal avatarv, YisVal pathv) {
  cogito_node* n = cogito_node_from_val(avatarv);
  YisStr* tmp = NULL;
  const char* path = cogito_required_cstr(pathv, &tmp);
  cogito_avatar_set_image(n, path);
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- Badge ---
static YisVal __cogito_badge(YisVal countv) {
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  cogito_node* n = cogito_badge_new(count);
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_badge_set_count(YisVal badgev, YisVal countv) {
  cogito_node* n = cogito_node_from_val(badgev);
  int count = 0;
  if (countv.tag == EVT_INT) count = (int)countv.as.i;
  else if (countv.tag == EVT_FLOAT) count = (int)countv.as.f;
  cogito_badge_set_count(n, count);
}

static YisVal __cogito_badge_get_count(YisVal badgev) {
  cogito_node* n = cogito_node_from_val(badgev);
  return YV_INT(cogito_badge_get_count(n));
}
// --- Banner ---
static YisVal __cogito_banner(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_banner_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_banner_set_action(YisVal bannerv, YisVal textv, YisVal handlerv) {
  cogito_node* banner = cogito_node_from_val(bannerv);
  CogitoHandle* h = (CogitoHandle*)bannerv.as.p;
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_banner_set_action(banner, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_banner_set_action(banner, text, cogito_cb_action, h);
  }
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_banner_set_icon(YisVal bannerv, YisVal iconv) {
  cogito_node* banner = cogito_node_from_val(bannerv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_banner_set_icon(banner, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- BottomSheet ---
static YisVal __cogito_bottom_sheet(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_bottom_sheet_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_side_sheet(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = NULL;
  if (titlev.tag != EVT_NULL) {
    title = cogito_required_cstr(titlev, &tmp);
  }
  cogito_node* n = cogito_side_sheet_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_side_sheet_set_mode(YisVal sheetv, YisVal modev) {
  cogito_node* n = cogito_node_from_val(sheetv);
  cogito_side_sheet_set_mode(n, (int)yis_as_int(modev));
}
// --- TimePicker ---
static YisVal __cogito_timepicker(void) {
  cogito_node* n = cogito_timepicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_timepicker_on_change(YisVal tpv, YisVal handler) {
  cogito_node* tp = cogito_node_from_val(tpv);
  CogitoHandle* h = (CogitoHandle*)tpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_timepicker_on_change(tp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_timepicker_on_change(tp, cogito_cb_change, h);
}

static YisVal __cogito_timepicker_get_hour(YisVal tpv) {
  cogito_node* n = cogito_node_from_val(tpv);
  return YV_INT(cogito_timepicker_get_hour(n));
}

static YisVal __cogito_timepicker_get_minute(YisVal tpv) {
  cogito_node* n = cogito_node_from_val(tpv);
  return YV_INT(cogito_timepicker_get_minute(n));
}

static void __cogito_timepicker_set_time(YisVal tpv, YisVal hourv, YisVal minv) {
  cogito_node* n = cogito_node_from_val(tpv);
  int hour = 0, minute = 0;
  if (hourv.tag == EVT_INT) hour = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) hour = (int)hourv.as.f;
  if (minv.tag == EVT_INT) minute = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) minute = (int)minv.as.f;
  cogito_timepicker_set_time(n, hour, minute);
}
// --- ActiveIndicator ---
static YisVal __cogito_active_indicator(void) {
  cogito_node* n = cogito_active_indicator_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SwitchBar ---
static YisVal __cogito_switchbar(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_switchbar_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static YisVal __cogito_switchbar_get_checked(YisVal sbv) {
  cogito_node* n = cogito_node_from_val(sbv);
  return YV_BOOL(cogito_switchbar_get_checked(n));
}
static void __cogito_switchbar_set_checked(YisVal sbv, YisVal val) {
  cogito_node* n = cogito_node_from_val(sbv);
  cogito_switchbar_set_checked(n, yis_as_bool(val));
}
static void __cogito_switchbar_on_change(YisVal sbv, YisVal handler) {
  cogito_node* sb = cogito_node_from_val(sbv);
  CogitoHandle* h = (CogitoHandle*)sbv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_switchbar_on_change(sb, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_switchbar_on_change(sb, cogito_cb_change, h);
}
// --- DrawingArea ---
static YisVal __cogito_drawing_area(void) {
  cogito_node* n = cogito_drawing_area_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_drawing_area_on_press(YisVal dav, YisVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_drawing_area_on_press(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_drawing_area_on_press(da, cogito_cb_click, h);
}
static void __cogito_drawing_area_on_drag(YisVal dav, YisVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_drawing_area_on_drag(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_drawing_area_on_drag(da, cogito_cb_change, h);
}
static void __cogito_drawing_area_on_release(YisVal dav, YisVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_drawing_area_on_release(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_drawing_area_on_release(da, cogito_cb_action, h);
}
static void __cogito_drawing_area_on_draw(YisVal dav, YisVal handler) {
  cogito_node* da = cogito_node_from_val(dav);
  CogitoHandle* h = (CogitoHandle*)dav.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_draw, handler);
    cogito_drawing_area_on_draw(da, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_draw, handler);
  cogito_drawing_area_on_draw(da, cogito_cb_draw, h);
}
static YisVal __cogito_drawing_area_get_x(YisVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_INT(cogito_drawing_area_get_x(da));
}
static YisVal __cogito_drawing_area_get_y(YisVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_INT(cogito_drawing_area_get_y(da));
}
static YisVal __cogito_drawing_area_get_pressed(YisVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  return YV_BOOL(cogito_drawing_area_get_pressed(da));
}
static void __cogito_drawing_area_clear(YisVal dav) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_drawing_area_clear(da);
}
static void __cogito_canvas_set_color(YisVal dav, YisVal colorv) {
  cogito_node* da = cogito_node_from_val(dav);
  YisStr* tmp = NULL;
  const char* color = cogito_required_cstr(colorv, &tmp);
  cogito_canvas_set_color(da, color);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_canvas_set_line_width(YisVal dav, YisVal widthv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_set_line_width(da, (int)yis_as_int(widthv));
}
static void __cogito_canvas_line(YisVal dav, YisVal x1v, YisVal y1v,
                                 YisVal x2v, YisVal y2v) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_line(da, (int)yis_as_int(x1v), (int)yis_as_int(y1v),
                     (int)yis_as_int(x2v), (int)yis_as_int(y2v));
}
static void __cogito_canvas_rect(YisVal dav, YisVal xv, YisVal yv,
                                 YisVal wv, YisVal hv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_rect(da, (int)yis_as_int(xv), (int)yis_as_int(yv),
                     (int)yis_as_int(wv), (int)yis_as_int(hv));
}
static void __cogito_canvas_fill_rect(YisVal dav, YisVal xv, YisVal yv,
                                      YisVal wv, YisVal hv) {
  cogito_node* da = cogito_node_from_val(dav);
  cogito_canvas_fill_rect(da, (int)yis_as_int(xv), (int)yis_as_int(yv),
                          (int)yis_as_int(wv), (int)yis_as_int(hv));
}
// --- Shape ---
static YisVal __cogito_shape(YisVal presetv) {
  cogito_node* n = cogito_shape_new((int)yis_as_int(presetv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_shape_set_preset(YisVal shapev, YisVal presetv) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_preset(n, (int)yis_as_int(presetv));
}
static YisVal __cogito_shape_get_preset(YisVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_preset(n));
}
static void __cogito_shape_set_size(YisVal shapev, YisVal sizev) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_size(n, (int)yis_as_int(sizev));
}
static YisVal __cogito_shape_get_size(YisVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_size(n));
}
static void __cogito_shape_set_color(YisVal shapev, YisVal colorv) {
  cogito_node* n = cogito_node_from_val(shapev);
  if (colorv.tag == EVT_INT || colorv.tag == EVT_FLOAT) {
    cogito_shape_set_color_style(n, (int)yis_as_int(colorv));
    return;
  }
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(colorv, &tmp);
  cogito_shape_set_color(n, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_shape_set_color_style(YisVal shapev, YisVal stylev) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_color_style(n, (int)yis_as_int(stylev));
}
static YisVal __cogito_shape_get_color_style(YisVal shapev) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_INT(cogito_shape_get_color_style(n));
}
static void __cogito_shape_set_vertex(YisVal shapev, YisVal idxv, YisVal xv, YisVal yv) {
  cogito_node* n = cogito_node_from_val(shapev);
  cogito_shape_set_vertex(n, (int)yis_as_int(idxv), (float)yis_as_float(xv),
                          (float)yis_as_float(yv));
}
static YisVal __cogito_shape_get_vertex_x(YisVal shapev, YisVal idxv) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_FLOAT((double)cogito_shape_get_vertex_x(n, (int)yis_as_int(idxv)));
}
static YisVal __cogito_shape_get_vertex_y(YisVal shapev, YisVal idxv) {
  cogito_node* n = cogito_node_from_val(shapev);
  return YV_FLOAT((double)cogito_shape_get_vertex_y(n, (int)yis_as_int(idxv)));
}
// --- ContentList ---
static YisVal __cogito_content_list(void) {
  cogito_node* n = cogito_content_list_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- EmptyPage ---
static YisVal __cogito_empty_page(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_empty_page_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_empty_page_set_description(YisVal epv, YisVal descv) {
  cogito_node* n = cogito_node_from_val(epv);
  YisStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_empty_page_set_description(n, desc);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_empty_page_set_icon(YisVal epv, YisVal iconv) {
  cogito_node* n = cogito_node_from_val(epv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_empty_page_set_icon(n, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_empty_page_set_action(YisVal epv, YisVal textv, YisVal handlerv) {
  cogito_node* ep = cogito_node_from_val(epv);
  CogitoHandle* h = (CogitoHandle*)epv.as.p;
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_empty_page_set_action(ep, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_empty_page_set_action(ep, text, cogito_cb_action, h);
  }
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- TipView ---
static YisVal __cogito_tip_view(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_tip_view_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_tip_view_set_title(YisVal tvv, YisVal titlev) {
  cogito_node* n = cogito_node_from_val(tvv);
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_tip_view_set_title(n, title);
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- SettingsWindow ---
static YisVal __cogito_settings_window(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_window_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsPage ---
static YisVal __cogito_settings_page(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_page_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsList ---
static YisVal __cogito_settings_list(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_settings_list_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SettingsRow ---
static YisVal __cogito_settings_row(YisVal labelv) {
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  cogito_node* n = cogito_settings_row_new(label);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- WelcomeScreen ---
static YisVal __cogito_welcome_screen(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_welcome_screen_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_welcome_screen_set_description(YisVal wsv, YisVal descv) {
  cogito_node* n = cogito_node_from_val(wsv);
  YisStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_welcome_screen_set_description(n, desc);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_welcome_screen_set_icon(YisVal wsv, YisVal iconv) {
  cogito_node* n = cogito_node_from_val(wsv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_welcome_screen_set_icon(n, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_welcome_screen_set_action(YisVal wsv, YisVal textv, YisVal handlerv) {
  cogito_node* ws = cogito_node_from_val(wsv);
  CogitoHandle* h = (CogitoHandle*)wsv.as.p;
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handlerv.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_welcome_screen_set_action(ws, text, NULL, NULL);
  } else {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_welcome_screen_set_action(ws, text, cogito_cb_action, h);
  }
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- ViewDual ---
static YisVal __cogito_view_dual(void) {
  cogito_node* n = cogito_view_dual_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_view_dual_set_ratio(YisVal vdv, YisVal ratiov) {
  cogito_node* n = cogito_node_from_val(vdv);
  double ratio = yis_as_float(ratiov);
  cogito_view_dual_set_ratio(n, ratio);
}
// --- ViewChooser ---
static YisVal __cogito_view_chooser(void) {
  cogito_node* n = cogito_view_chooser_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_view_chooser_set_items(YisVal vcv, YisVal arrv) {
  cogito_node* n = cogito_node_from_val(vcv);
  if (arrv.tag != EVT_ARR) yis_trap("view_chooser_set_items expects array");
  YisArr* a = (YisArr*)arrv.as.p;
  size_t len = (size_t)a->len;
  const char** items = (const char**)malloc(sizeof(const char*) * len);
  YisStr** tmps = (YisStr**)malloc(sizeof(YisStr*) * len);
  for (size_t i = 0; i < len; i++) {
    YisVal v = yis_arr_get(a, (int64_t)i);
    tmps[i
    ] = NULL;
    items[i
    ] = cogito_required_cstr(v, &tmps[i
    ]);
  }
  cogito_view_chooser_set_items(n, items, len);
  for (size_t i = 0; i < len; i++) {
    if (tmps[i
    ]) yis_release_val(YV_STR(tmps[i
    ]));
  }
  free(items);
  free(tmps);
}
static void __cogito_view_chooser_bind(YisVal vcv, YisVal vsv) {
  cogito_node* vc = cogito_node_from_val(vcv);
  cogito_node* vs = cogito_node_from_val(vsv);
  cogito_view_chooser_bind(vc, vs);
}
// --- AboutWindow ---
static YisVal __cogito_about_window(YisVal namev, YisVal versionv) {
  YisStr* tmp1 = NULL;
  YisStr* tmp2 = NULL;
  const char* name = cogito_required_cstr(namev, &tmp1);
  const char* version = cogito_required_cstr(versionv, &tmp2);
  cogito_node* n = cogito_about_window_new(name, version);
  if (tmp1) yis_release_val(YV_STR(tmp1));
  if (tmp2) yis_release_val(YV_STR(tmp2));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_about_window_set_icon(YisVal awv, YisVal iconv) {
  cogito_node* n = cogito_node_from_val(awv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_about_window_set_icon(n, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_description(YisVal awv, YisVal descv) {
  cogito_node* n = cogito_node_from_val(awv);
  YisStr* tmp = NULL;
  const char* desc = cogito_required_cstr(descv, &tmp);
  cogito_about_window_set_description(n, desc);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_website(YisVal awv, YisVal urlv) {
  cogito_node* n = cogito_node_from_val(awv);
  YisStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_about_window_set_website(n, url);
  if (tmp) yis_release_val(YV_STR(tmp));
}
static void __cogito_about_window_set_issue_url(YisVal awv, YisVal urlv) {
  cogito_node* n = cogito_node_from_val(awv);
  YisStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_about_window_set_issue_url(n, url);
  if (tmp) yis_release_val(YV_STR(tmp));
}
// --- MenuButton ---
static YisVal __cogito_menu_button(YisVal iconv) {
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_menu_button_new(icon);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
// --- SplitButton ---
static YisVal __cogito_split_button(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_split_button_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}
static void __cogito_split_button_add_menu(YisVal btnv, YisVal labelv, YisVal handlerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  if (handlerv.tag == EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_split_button_add_menu(btn, label, cogito_cb_action, h);
  } else {
    cogito_split_button_add_menu(btn, label, NULL, NULL);
  }
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_split_button_add_menu_section(YisVal btnv, YisVal labelv, YisVal handlerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  if (handlerv.tag == EVT_FN) {
    cogito_set_handler(h, &h->on_action, handlerv);
    cogito_split_button_add_menu_section(btn, label, cogito_cb_action, h);
  } else {
    cogito_split_button_add_menu_section(btn, label, NULL, NULL);
  }
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_split_button_set_size(YisVal btnv, YisVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  int size = (int)yis_as_int(sizev);
  cogito_split_button_set_size(btn, size);
}

static void __cogito_split_button_set_variant(YisVal btnv, YisVal variantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  int variant = (int)yis_as_int(variantv);
  cogito_split_button_set_variant(btn, variant);
}

static YisVal __cogito_image(YisVal iconv) {
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_image_new(icon);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_image_set_icon(YisVal imgv, YisVal iconv) {
  cogito_node* n = cogito_node_from_val(imgv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_image_set_icon(n, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_image_set_source(YisVal imgv, YisVal sourcev) {
  cogito_node* n = cogito_node_from_val(imgv);
  YisStr* tmp = NULL;
  const char* source = cogito_required_cstr(sourcev, &tmp);
  cogito_image_set_source(n, source);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_image_set_size(YisVal imgv, YisVal wv, YisVal hv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int w = (int)yis_as_int(wv);
  int h = (int)yis_as_int(hv);
  cogito_image_set_size(n, w, h);
}

static void __cogito_image_set_radius(YisVal imgv, YisVal rv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int r = (int)yis_as_int(rv);
  cogito_image_set_radius(n, r);
}

static void __cogito_image_set_alt_text(YisVal imgv, YisVal altv) {
  cogito_node* n = cogito_node_from_val(imgv);
  YisStr* tmp = NULL;
  const char* alt = cogito_optional_cstr(altv, &tmp);
  if (alt) cogito_image_set_alt_text(n, alt);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_webview(YisVal urlv) {
  YisStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_node* n = cogito_webview_new(url);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_webview_set_url(YisVal wvv, YisVal urlv) {
  cogito_node* n = cogito_node_from_val(wvv);
  YisStr* tmp = NULL;
  const char* url = cogito_required_cstr(urlv, &tmp);
  cogito_webview_set_url(n, url);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_webview_get_url(YisVal wvv) {
  cogito_node* n = cogito_node_from_val(wvv);
  const char* url = cogito_webview_get_url(n);
  return YV_STR(stdr_str_lit(url ? url : ""));
}

static void __cogito_webview_set_open_external_on_click(YisVal wvv, YisVal onv) {
  cogito_node* n = cogito_node_from_val(wvv);
  cogito_webview_set_open_external_on_click(n, yis_as_bool(onv));
}

static YisVal __cogito_webview_get_open_external_on_click(YisVal wvv) {
  cogito_node* n = cogito_node_from_val(wvv);
  return YV_BOOL(cogito_webview_get_open_external_on_click(n));
}

static YisVal __cogito_webview_open(YisVal wvv) {
  cogito_node* n = cogito_node_from_val(wvv);
  return YV_BOOL(cogito_webview_open(n));
}

static YisVal __cogito_appbar(YisVal titlev, YisVal subtitlev) {
  YisStr* ttmp = NULL;
  YisStr* stmp = NULL;
  const char* title = cogito_required_cstr(titlev, &ttmp);
  const char* subtitle = cogito_required_cstr(subtitlev, &stmp);
  cogito_node* n = cogito_appbar_new(title, subtitle);
  if (ttmp) yis_release_val(YV_STR(ttmp));
  if (stmp) yis_release_val(YV_STR(stmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_appbar_add_button(YisVal appbarv, YisVal iconv, YisVal handler) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* btn = cogito_appbar_add_button(appbar, icon, NULL, NULL);
  if (tmp) yis_release_val(YV_STR(tmp));
  YisVal btnv = cogito_wrap_node(btn, COGITO_HANDLE_NODE);
  if (handler.tag == EVT_FN) __cogito_button_on_click(btnv, handler);
  return btnv;
}

static void __cogito_appbar_set_controls(YisVal appbarv, YisVal layoutv) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  YisStr* tmp = NULL;
  const char* layout = cogito_optional_cstr(layoutv, &tmp);
  if (layout) cogito_appbar_set_controls(appbar, layout);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_appbar_set_title(YisVal appbarv, YisVal titlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  YisStr* tmp = NULL;
  const char* title = cogito_optional_cstr(titlev, &tmp);
  cogito_appbar_set_title(appbar, title ? title : "");
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_appbar_set_subtitle(YisVal appbarv, YisVal subtitlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  YisStr* tmp = NULL;
  const char* subtitle = cogito_optional_cstr(subtitlev, &tmp);
  cogito_appbar_set_subtitle(appbar, subtitle ? subtitle : "");
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_dialog(YisVal titlev) {
  YisStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_dialog_new(title);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_dialog_close(YisVal dialogv) {
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_close(dialog);
}

static void __cogito_dialog_remove(YisVal dialogv) {
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_remove(dialog);
}

static YisVal __cogito_dialog_slot(void) {
  cogito_node* n = cogito_dialog_slot_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_dialog_slot_show(YisVal slotv, YisVal dialogv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_slot_show(slot, dialog);
}

static void __cogito_dialog_slot_clear(YisVal slotv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_dialog_slot_clear(slot);
}

static void __cogito_window_set_dialog(YisVal winv, YisVal dialogv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_window_set_dialog(win, dialog);
}

static void __cogito_window_clear_dialog(YisVal winv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_clear_dialog(win);
}

static void __cogito_window_set_side_sheet(YisVal winv, YisVal side_sheetv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_node* side_sheet = cogito_node_from_val(side_sheetv);
  cogito_window_set_side_sheet(win, side_sheet);
}

static void __cogito_window_clear_side_sheet(YisVal winv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_clear_side_sheet(win);
}

static YisVal __cogito_node_window(YisVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_window* win = cogito_node_window(n);
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static YisVal __cogito_find_parent(YisVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_node* p = cogito_node_get_parent(n);
  if (!p) return YV_NULLV;
  return cogito_wrap_node(p, COGITO_HANDLE_NODE);
}

static YisVal __cogito_find_children(YisVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  size_t count = cogito_node_get_child_count(n);
  YisArr* arr = stdr_arr_new((int)count);
  for (size_t i = 0; i < count; i++) {
    cogito_node* child = cogito_node_get_child(n, i);
    yis_arr_add(arr, cogito_wrap_node(child, COGITO_HANDLE_NODE));
  }
  return YV_ARR(arr);
}

static void __cogito_label_set_class(YisVal labelv, YisVal classv) {
  cogito_node* n = cogito_node_from_val(labelv);
  YisStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_label_set_text(YisVal labelv, YisVal textv) {
  cogito_node* n = cogito_node_from_val(labelv);
  YisStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_text(n, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_node_set_class(YisVal nodev, YisVal classv) {
  cogito_node* n = cogito_node_from_val(nodev);
  YisStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_node_set_a11y_label(YisVal nodev, YisVal labelv) {
  cogito_node* n = cogito_node_from_val(nodev);
  YisStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_node_set_a11y_label(n, label);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_node_set_a11y_role(YisVal nodev, YisVal rolev) {
  cogito_node* n = cogito_node_from_val(nodev);
  YisStr* tmp = NULL;
  const char* role = cogito_optional_cstr(rolev, &tmp);
  if (role) cogito_node_set_a11y_role(n, role);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_node_set_tooltip(YisVal nodev, YisVal textv) {
  cogito_node* n = cogito_node_from_val(nodev);
  YisStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_tooltip(n, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_pointer_capture(YisVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_release();
    return;
  }
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_pointer_capture(n);
}

static void __cogito_pointer_release(void) {
  cogito_pointer_release();
}

static void __cogito_label_set_wrap(YisVal labelv, YisVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_wrap(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_ellipsis(YisVal labelv, YisVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_ellipsis(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_align(YisVal labelv, YisVal alignv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_align(n, (int)yis_as_int(alignv));
}

static YisVal __cogito_checkbox(YisVal textv, YisVal groupv) {
  YisStr* ttmp = NULL;
  YisStr* gtmp = NULL;
  const char* text = cogito_required_cstr(textv, &ttmp);
  const char* group = cogito_optional_cstr(groupv, &gtmp);
  cogito_node* n = cogito_checkbox_new(text, group);
  if (ttmp) yis_release_val(YV_STR(ttmp));
  if (gtmp) yis_release_val(YV_STR(gtmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_switch(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_switch_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_textfield(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textfield_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_textview(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textview_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_searchfield(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_searchfield_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_searchfield_set_text(YisVal sfv, YisVal textv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_searchfield_set_text(sf, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_searchfield_get_text(YisVal sfv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  const char* text = cogito_searchfield_get_text(sf);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_searchfield_on_change(YisVal sfv, YisVal handler) {
  cogito_node* sf = cogito_node_from_val(sfv);
  CogitoHandle* h = (CogitoHandle*)sfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_searchfield_on_change(sf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_searchfield_on_change(sf, cogito_cb_change, h);
}

static YisVal __cogito_dropdown(void) {
  cogito_node* n = cogito_dropdown_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_datepicker(void) {
  cogito_node* n = cogito_datepicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_datepicker_on_change(YisVal dpv, YisVal handler) {
  cogito_node* dp = cogito_node_from_val(dpv);
  CogitoHandle* h = (CogitoHandle*)dpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_datepicker_on_change(dp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_datepicker_on_change(dp, cogito_cb_change, h);
}

static YisVal __cogito_stepper(YisVal minv, YisVal maxv, YisVal valuev, YisVal stepv) {
  cogito_node* n = cogito_stepper_new(yis_as_float(minv), yis_as_float(maxv), yis_as_float(valuev), yis_as_float(stepv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_slider(YisVal minv, YisVal maxv, YisVal valuev) {
  cogito_node* n = cogito_slider_new(yis_as_float(minv), yis_as_float(maxv), yis_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_slider_range(YisVal minv, YisVal maxv, YisVal startv, YisVal endv) {
  cogito_node* n = cogito_slider_range_new(yis_as_float(minv), yis_as_float(maxv), yis_as_float(startv), yis_as_float(endv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_tabs(void) {
  cogito_node* n = cogito_tabs_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_nav_rail(void) {
  cogito_node* n = cogito_nav_rail_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_buttongroup(void) {
  cogito_node* n = cogito_buttongroup_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_view_switcher(void) {
  cogito_node* n = cogito_view_switcher_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_progress(YisVal valuev) {
  cogito_node* n = cogito_progress_new(yis_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_treeview(void) {
  cogito_node* n = cogito_treeview_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_colorpicker(void) {
  cogito_node* n = cogito_colorpicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_colorpicker_on_change(YisVal cpv, YisVal handler) {
  cogito_node* cp = cogito_node_from_val(cpv);
  CogitoHandle* h = (CogitoHandle*)cpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_colorpicker_on_change(cp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_colorpicker_on_change(cp, cogito_cb_change, h);
}

static void __cogito_colorpicker_set_hex(YisVal cpv, YisVal hexv) {
  cogito_node* cp = cogito_node_from_val(cpv);
  YisStr* tmp = NULL;
  const char* hex = cogito_optional_cstr(hexv, &tmp);
  if (hex) cogito_colorpicker_set_hex(cp, hex);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_colorpicker_get_hex(YisVal cpv) {
  cogito_node* cp = cogito_node_from_val(cpv);
  const char* hex = cogito_colorpicker_get_hex(cp);
  return YV_STR(stdr_str_lit(hex ? hex : ""));
}

static YisVal __cogito_toasts(void) {
  cogito_node* n = cogito_toasts_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_toast(YisVal textv) {
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_toast_new(text);
  if (tmp) yis_release_val(YV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static YisVal __cogito_toolbar(void) {
  cogito_node* n = cogito_toolbar_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_toolbar_set_vibrant(YisVal toolbarv, YisVal vibrantv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  cogito_toolbar_set_vibrant(toolbar, yis_as_bool(vibrantv));
}

static YisVal __cogito_toolbar_get_vibrant(YisVal toolbarv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  return YV_BOOL(cogito_toolbar_get_vibrant(toolbar));
}

static void __cogito_toolbar_set_vertical(YisVal toolbarv, YisVal verticalv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  cogito_toolbar_set_vertical(toolbar, yis_as_bool(verticalv));
}

static YisVal __cogito_toolbar_get_vertical(YisVal toolbarv) {
  cogito_node* toolbar = cogito_node_from_val(toolbarv);
  return YV_BOOL(cogito_toolbar_get_vertical(toolbar));
}

static YisVal __cogito_vstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_VSTACK), COGITO_HANDLE_NODE);
}

static YisVal __cogito_hstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_HSTACK), COGITO_HANDLE_NODE);
}

static YisVal __cogito_zstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_ZSTACK), COGITO_HANDLE_NODE);
}

static YisVal __cogito_fixed(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_FIXED), COGITO_HANDLE_NODE);
}

static YisVal __cogito_scroller(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_SCROLLER), COGITO_HANDLE_NODE);
}

static YisVal __cogito_carousel(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_CAROUSEL), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_set_active_index(YisVal carouselv, YisVal idxv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  cogito_carousel_set_active_index(carousel, (int)yis_as_int(idxv));
}

static YisVal __cogito_carousel_get_active_index(YisVal carouselv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  int idx = cogito_carousel_get_active_index(carousel);
  return YV_INT(idx);
}

static YisVal __cogito_carousel_item(void) {
  return cogito_wrap_node(cogito_carousel_item_new(), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_item_set_text(YisVal itemv, YisVal textv) {
  cogito_node* item = cogito_node_from_val(itemv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_carousel_item_set_text(item, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_carousel_item_set_halign(YisVal itemv, YisVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_halign(item, (int)yis_as_int(alignv));
}

static void __cogito_carousel_item_set_valign(YisVal itemv, YisVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_valign(item, (int)yis_as_int(alignv));
}

static YisVal __cogito_list(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_LIST), COGITO_HANDLE_NODE);
}

static YisVal __cogito_grid(YisVal cols) {
  return cogito_wrap_node(cogito_grid_new_with_cols((int)yis_as_int(cols)), COGITO_HANDLE_NODE);
}

static void __cogito_container_add(YisVal parentv, YisVal childv) {
  cogito_node* parent = cogito_node_from_val(parentv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_node_add(parent, child);
}

static void __cogito_container_set_margins(YisVal nodev, YisVal top, YisVal right, YisVal bottom, YisVal left) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_margins(node, (int)yis_as_int(top), (int)yis_as_int(right), (int)yis_as_int(bottom), (int)yis_as_int(left));
}

static void __cogito_container_set_align(YisVal nodev, YisVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_align(node, (int)yis_as_int(align));
}

static void __cogito_container_set_halign(YisVal nodev, YisVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_halign(node, (int)yis_as_int(align));
}

static void __cogito_container_set_valign(YisVal nodev, YisVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_valign(node, (int)yis_as_int(align));
}

static void __cogito_container_set_hexpand(YisVal nodev, YisVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_hexpand(node, yis_as_bool(expand));
}

static void __cogito_container_set_vexpand(YisVal nodev, YisVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  bool expand_bool = yis_as_bool(expand);
  cogito_node_set_vexpand(node, expand_bool);
}

static void __cogito_container_set_gap(YisVal nodev, YisVal gap) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_gap(node, (int)yis_as_int(gap));
}

static void __cogito_container_set_padding(YisVal nodev, YisVal top, YisVal right, YisVal bottom, YisVal left) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_padding(node, (int)yis_as_int(top), (int)yis_as_int(right), (int)yis_as_int(bottom), (int)yis_as_int(left));
}

static void __cogito_fixed_set_pos(YisVal fixedv, YisVal childv, YisVal xv, YisVal yv) {
  cogito_node* fixed = cogito_node_from_val(fixedv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_fixed_set_pos(fixed, child, (int)yis_as_int(xv), (int)yis_as_int(yv));
}

static void __cogito_scroller_set_axes(YisVal scv, YisVal hv, YisVal vv) {
  cogito_node* sc = cogito_node_from_val(scv);
  bool h = hv.tag == EVT_BOOL ? hv.as.b : false;
  bool v = vv.tag == EVT_BOOL ? vv.as.b : false;
  cogito_scroller_set_axes(sc, h, v);
}

static void __cogito_scroller_set_sync(YisVal scv, YisVal otherv) {
  cogito_node* sc = cogito_node_from_val(scv);
  cogito_node* other = NULL;
  if (otherv.tag == EVT_OBJ && otherv.as.p) {
    other = cogito_node_from_val(otherv);
  }
  cogito_scroller_set_sync(sc, other);
}

static void __cogito_grid_set_gap(YisVal gridv, YisVal xv, YisVal yv) {
  cogito_node* grid = cogito_node_from_val(gridv);
  cogito_grid_set_gap(grid, (int)yis_as_int(xv), (int)yis_as_int(yv));
}

static void __cogito_grid_set_span(YisVal childv, YisVal col_span, YisVal row_span) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_span(child, (int)yis_as_int(col_span), (int)yis_as_int(row_span));
}

static void __cogito_grid_set_align(YisVal childv, YisVal halign, YisVal valign) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_align(child, (int)yis_as_int(halign), (int)yis_as_int(valign));
}

static void __cogito_node_set_disabled(YisVal nodev, YisVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_disabled(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_node_set_editable(YisVal nodev, YisVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_editable(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static YisVal __cogito_node_get_editable(YisVal nodev) {
  cogito_node* node = cogito_node_from_val(nodev);
  return YV_BOOL(cogito_node_get_editable(node));
}

static void __cogito_node_set_id(YisVal nodev, YisVal idv) {
  cogito_node* node = cogito_node_from_val(nodev);
  YisStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_node_set_id(node, id);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_button_set_text(YisVal btnv, YisVal textv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_button_set_text(btn, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_button_set_size(YisVal btnv, YisVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_size(btn, (int)yis_as_int(sizev));
}

static YisVal __cogito_button_get_size(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_size(btn));
}

static void __cogito_button_on_click(YisVal btnv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(btn, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(btn, cogito_cb_click, h);
}

static void __cogito_button_add_menu(YisVal btnv, YisVal labelv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_button_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_button_add_menu_section(YisVal btnv, YisVal labelv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_button_add_menu_section(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_iconbtn_add_menu(YisVal btnv, YisVal labelv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_iconbtn_add_menu_section(YisVal btnv, YisVal labelv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  YisStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu_section(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) yis_release_val(YV_STR(tmp));
}

// Menu item property setters (work on any node with menu items)
// Use public API (cogito_menu_set_*) so these compile against the opaque cogito_node.
static void __cogito_menu_set_icon(YisVal nodev, YisVal iconv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  YisStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_menu_set_icon(n, icon);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_menu_set_shortcut(YisVal nodev, YisVal shortcutv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  YisStr* tmp = NULL;
  const char* shortcut = cogito_required_cstr(shortcutv, &tmp);
  cogito_menu_set_shortcut(n, shortcut);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_menu_set_submenu(YisVal nodev, YisVal submenuv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  cogito_menu_set_submenu(n, yis_as_bool(submenuv));
}

static void __cogito_menu_set_toggled(YisVal nodev, YisVal toggledv) {
  cogito_node* n = cogito_node_from_val(nodev);
  if (!n) return;
  cogito_menu_set_toggled(n, yis_as_bool(toggledv));
}

// Icon button setters/getters
static void __cogito_iconbtn_set_shape(YisVal btnv, YisVal shapev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_shape(btn, (int)yis_as_int(shapev));
}

static YisVal __cogito_iconbtn_get_shape(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_shape(btn));
}

static void __cogito_iconbtn_set_color_style(YisVal btnv, YisVal stylev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_color_style(btn, (int)yis_as_int(stylev));
}

static YisVal __cogito_iconbtn_get_color_style(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_color_style(btn));
}

static void __cogito_iconbtn_set_width(YisVal btnv, YisVal widthv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_width(btn, (int)yis_as_int(widthv));
}

static YisVal __cogito_iconbtn_get_width(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_width(btn));
}

static void __cogito_iconbtn_set_toggle(YisVal btnv, YisVal togglev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_toggle(btn, yis_as_bool(togglev));
}

static YisVal __cogito_iconbtn_get_toggle(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_iconbtn_get_toggle(btn));
}

static void __cogito_iconbtn_set_checked(YisVal btnv, YisVal checkedv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_checked(btn, yis_as_bool(checkedv));
}

static YisVal __cogito_iconbtn_get_checked(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_iconbtn_get_checked(btn));
}

static void __cogito_iconbtn_set_size(YisVal btnv, YisVal sizev) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_iconbtn_set_size(btn, (int)yis_as_int(sizev));
}

static YisVal __cogito_iconbtn_get_size(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_iconbtn_get_size(btn));
}

static void __cogito_iconbtn_set_menu_divider(YisVal btnv, YisVal dividerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_divider(btn, yis_as_bool(dividerv));
}

static YisVal __cogito_iconbtn_get_menu_divider(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_divider(btn));
}

static void __cogito_iconbtn_set_menu_item_gap(YisVal btnv, YisVal gapv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_item_gap(btn, (int)yis_as_int(gapv));
}

static YisVal __cogito_iconbtn_get_menu_item_gap(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_menu_item_gap(btn));
}

static void __cogito_button_set_menu_divider(YisVal btnv, YisVal dividerv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_divider(btn, yis_as_bool(dividerv));
}

static YisVal __cogito_button_get_menu_divider(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_divider(btn));
}

static void __cogito_button_set_menu_item_gap(YisVal btnv, YisVal gapv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_item_gap(btn, (int)yis_as_int(gapv));
}

static YisVal __cogito_button_get_menu_item_gap(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_INT(cogito_button_get_menu_item_gap(btn));
}

static void __cogito_iconbtn_set_menu_vibrant(YisVal btnv, YisVal vibrantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_vibrant(btn, yis_as_bool(vibrantv));
}

static YisVal __cogito_iconbtn_get_menu_vibrant(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_vibrant(btn));
}

static void __cogito_button_set_menu_vibrant(YisVal btnv, YisVal vibrantv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  cogito_button_set_menu_vibrant(btn, yis_as_bool(vibrantv));
}

static YisVal __cogito_button_get_menu_vibrant(YisVal btnv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  return YV_BOOL(cogito_button_get_menu_vibrant(btn));
}

static void __cogito_iconbtn_on_click(YisVal btnv, YisVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(btn, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(btn, cogito_cb_click, h);
}

static void __cogito_checkbox_set_checked(YisVal cbv, YisVal checkedv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  cogito_checkbox_set_checked(cb, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static YisVal __cogito_checkbox_get_checked(YisVal cbv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  return YV_BOOL(cogito_checkbox_get_checked(cb));
}

static void __cogito_switch_set_checked(YisVal swv, YisVal checkedv) {
  cogito_node* sw = cogito_node_from_val(swv);
  cogito_switch_set_checked(sw, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static YisVal __cogito_switch_get_checked(YisVal swv) {
  cogito_node* sw = cogito_node_from_val(swv);
  return YV_BOOL(cogito_switch_get_checked(sw));
}

static void __cogito_checkbox_on_change(YisVal cbv, YisVal handler) {
  cogito_node* cb = cogito_node_from_val(cbv);
  CogitoHandle* h = (CogitoHandle*)cbv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_checkbox_on_change(cb, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_checkbox_on_change(cb, cogito_cb_change, h);
}

static void __cogito_switch_on_change(YisVal swv, YisVal handler) {
  cogito_node* sw = cogito_node_from_val(swv);
  CogitoHandle* h = (CogitoHandle*)swv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_switch_on_change(sw, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_switch_on_change(sw, cogito_cb_change, h);
}

static void __cogito_textfield_set_text(YisVal tfv, YisVal textv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textfield_set_text(tf, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_textfield_get_text(YisVal tfv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  const char* text = cogito_textfield_get_text(tf);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textfield_set_hint(YisVal tfv, YisVal hintv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  YisStr* tmp = NULL;
  const char* hint = cogito_required_cstr(hintv, &tmp);
  cogito_textfield_set_hint(tf, hint);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_textfield_get_hint(YisVal tfv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  const char* hint = cogito_textfield_get_hint(tf);
  return YV_STR(stdr_str_lit(hint ? hint : ""));
}

static void __cogito_textfield_on_change(YisVal tfv, YisVal handler) {
  cogito_node* tf = cogito_node_from_val(tfv);
  CogitoHandle* h = (CogitoHandle*)tfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textfield_on_change(tf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textfield_on_change(tf, cogito_cb_change, h);
}

static void __cogito_textview_set_text(YisVal tvv, YisVal textv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textview_set_text(tv, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static YisVal __cogito_textview_get_text(YisVal tvv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  const char* text = cogito_textview_get_text(tv);
  return YV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textview_on_change(YisVal tvv, YisVal handler) {
  cogito_node* tv = cogito_node_from_val(tvv);
  CogitoHandle* h = (CogitoHandle*)tvv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textview_on_change(tv, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textview_on_change(tv, cogito_cb_change, h);
}

static void __cogito_dropdown_set_items(YisVal ddv, YisVal itemsv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  if (itemsv.tag != EVT_ARR) yis_trap("cogito.dropdown_set_items expects array");
  YisArr* arr = (YisArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  YisStr** temps = (YisStr**)calloc(count, sizeof(YisStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_dropdown_set_items(dd, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) yis_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_dropdown_set_selected(YisVal ddv, YisVal idxv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  cogito_dropdown_set_selected(dd, (int)yis_as_int(idxv));
}

static YisVal __cogito_dropdown_get_selected(YisVal ddv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  return YV_INT(cogito_dropdown_get_selected(dd));
}

static void __cogito_dropdown_on_change(YisVal ddv, YisVal handler) {
  cogito_node* dd = cogito_node_from_val(ddv);
  CogitoHandle* h = (CogitoHandle*)ddv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_dropdown_on_change(dd, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_dropdown_on_change(dd, cogito_cb_change, h);
}

static void __cogito_slider_set_value(YisVal slv, YisVal valuev) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_value(sl, yis_as_float(valuev));
}

static YisVal __cogito_slider_get_value(YisVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_value(sl));
}

static void __cogito_slider_set_size(YisVal slv, YisVal sizev) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_size(sl, (int)yis_as_int(sizev));
}

static YisVal __cogito_slider_get_size(YisVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_INT(cogito_slider_get_size(sl));
}

static void __cogito_slider_set_icon(YisVal slv, YisVal iconv) {
  cogito_node* sl = cogito_node_from_val(slv);
  if (iconv.tag == EVT_NULL) {
    cogito_slider_set_icon(sl, NULL);
    return;
  }
  if (iconv.tag == EVT_STR) {
    YisStr* s = (YisStr*)iconv.as.p;
    cogito_slider_set_icon(sl, s ? s->data : NULL);
    return;
  }
  YisStr* s = stdr_to_string(iconv);
  cogito_slider_set_icon(sl, s ? s->data : NULL);
  if (s) yis_release_val(YV_STR(s));
}

static void __cogito_slider_set_centered(YisVal slv, YisVal onv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_centered(sl, yis_as_bool(onv));
}

static YisVal __cogito_slider_get_centered(YisVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_BOOL(cogito_slider_get_centered(sl));
}

static void __cogito_slider_set_range(YisVal slv, YisVal startv, YisVal endv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range(sl, yis_as_float(startv), yis_as_float(endv));
}

static void __cogito_slider_set_range_start(YisVal slv, YisVal startv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range_start(sl, yis_as_float(startv));
}

static void __cogito_slider_set_range_end(YisVal slv, YisVal endv) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_range_end(sl, yis_as_float(endv));
}

static YisVal __cogito_slider_get_range_start(YisVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_range_start(sl));
}

static YisVal __cogito_slider_get_range_end(YisVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return YV_FLOAT(cogito_slider_get_range_end(sl));
}

static void __cogito_slider_on_change(YisVal slv, YisVal handler) {
  cogito_node* sl = cogito_node_from_val(slv);
  CogitoHandle* h = (CogitoHandle*)slv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_slider_on_change(sl, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_slider_on_change(sl, cogito_cb_change, h);
}

static void __cogito_stepper_set_value(YisVal stv, YisVal valuev) {
  cogito_node* st = cogito_node_from_val(stv);
  cogito_stepper_set_value(st, yis_as_float(valuev));
}

static YisVal __cogito_stepper_get_value(YisVal stv) {
  cogito_node* st = cogito_node_from_val(stv);
  return YV_FLOAT(cogito_stepper_get_value(st));
}

static void __cogito_stepper_on_change(YisVal stv, YisVal handler) {
  cogito_node* st = cogito_node_from_val(stv);
  CogitoHandle* h = (CogitoHandle*)stv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_stepper_on_change(st, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_stepper_on_change(st, cogito_cb_change, h);
}

static void __cogito_buttongroup_on_select(YisVal segv, YisVal handler) {
  cogito_node* seg = cogito_node_from_val(segv);
  CogitoHandle* h = (CogitoHandle*)segv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_buttongroup_on_select(seg, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_buttongroup_on_select(seg, cogito_cb_select, h);
}

static void __cogito_buttongroup_set_size(YisVal bgv, YisVal sizev) {
  cogito_node* bg = cogito_node_from_val(bgv);
  int size = (int)yis_as_int(sizev);
  if (size < 0) size = 0;
  if (size > 4) size = 4;
  cogito_buttongroup_set_size(bg, size);
}

static YisVal __cogito_buttongroup_get_size(YisVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_INT(cogito_buttongroup_get_size(bg));
}

static void __cogito_buttongroup_set_shape(YisVal bgv, YisVal shapev) {
  cogito_node* bg = cogito_node_from_val(bgv);
  int shape = (int)yis_as_int(shapev);
  if (shape < 0) shape = 0;
  if (shape > 1) shape = 1;
  cogito_buttongroup_set_shape(bg, shape);
}

static YisVal __cogito_buttongroup_get_shape(YisVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_INT(cogito_buttongroup_get_shape(bg));
}

static void __cogito_buttongroup_set_connected(YisVal bgv, YisVal connectedv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  cogito_buttongroup_set_connected(bg, yis_as_bool(connectedv));
}

static YisVal __cogito_buttongroup_get_connected(YisVal bgv) {
  cogito_node* bg = cogito_node_from_val(bgv);
  return YV_BOOL(cogito_buttongroup_get_connected(bg));
}

static void __cogito_tabs_set_items(YisVal tabsv, YisVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) yis_trap("cogito.tabs_set_items expects array");
  YisArr* arr = (YisArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  YisStr** temps = (YisStr**)calloc(count, sizeof(YisStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_tabs_set_items(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) yis_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_ids(YisVal tabsv, YisVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) yis_trap("cogito.tabs_set_ids expects array");
  YisArr* arr = (YisArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  YisStr** temps = (YisStr**)calloc(count, sizeof(YisStr*));
  for (size_t i = 0; i < count; i++) {
    items[i
    ] = cogito_required_cstr(arr->items[i
    ], &temps[i
    ]);
  }
  cogito_tabs_set_ids(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i
    ]) yis_release_val(YV_STR(temps[i
    ]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_selected(YisVal tabsv, YisVal idxv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_tabs_set_selected(tabs, (int)yis_as_int(idxv));
}

static YisVal __cogito_tabs_get_selected(YisVal tabsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  return YV_INT(cogito_tabs_get_selected(tabs));
}

static void __cogito_tabs_on_change(YisVal tabsv, YisVal handler) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  CogitoHandle* h = (CogitoHandle*)tabsv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_tabs_on_change(tabs, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_tabs_on_change(tabs, cogito_cb_change, h);
}

static void __cogito_tabs_bind(YisVal tabsv, YisVal viewv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_node* view = cogito_node_from_val(viewv);
  cogito_tabs_bind(tabs, view);
}

static void __cogito_nav_rail_set_items(YisVal railv, YisVal labelsv, YisVal iconsv) {
  cogito_node* rail = cogito_node_from_val(railv);
  if (labelsv.tag != EVT_ARR) yis_trap("cogito.nav_rail_set_items expects array of labels");
  YisArr* labels = (YisArr*)labelsv.as.p;
  size_t count = labels->len;

  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  YisStr** label_temps = (YisStr**)calloc(count, sizeof(YisStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i
    ] = cogito_required_cstr(labels->items[i
    ], &label_temps[i
    ]);
  }

  const char** icon_strs = NULL;
  YisStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    YisArr* icons = (YisArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (YisStr**)calloc(icon_count, sizeof(YisStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i
      ] = cogito_required_cstr(icons->items[i
      ], &icon_temps[i
      ]);
    }
    cogito_nav_rail_set_items(rail, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i
      ]) yis_release_val(YV_STR(icon_temps[i
      ]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_nav_rail_set_items(rail, label_strs, NULL, count);
  }

  for (size_t i = 0; i < count; i++) {
    if (label_temps[i
    ]) yis_release_val(YV_STR(label_temps[i
    ]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_nav_rail_set_badges(YisVal railv, YisVal badgesv) {
  cogito_node* rail = cogito_node_from_val(railv);
  if (badgesv.tag == EVT_NULL) {
    cogito_nav_rail_set_badges(rail, NULL,
    0);
    return;
  }
  if (badgesv.tag != EVT_ARR) yis_trap("cogito.nav_rail_set_badges expects array");
  YisArr* badges = (YisArr*)badgesv.as.p;
  size_t count = badges->len;
  int* vals = (int*)calloc(count, sizeof(int));
  for (size_t i = 0; i < count; i++) {
    YisVal bv = badges->items[i
    ];
    int v = 0;
    switch (bv.tag) {
      case EVT_NULL:
        v = 0;
        break;
      case EVT_BOOL:
        v = bv.as.b ? -1 : 0;
        break;
      case EVT_INT:
        v = (int)bv.as.i;
        if (v < 0) v = -1;
        break;
      case EVT_FLOAT: {
        int rounded = (int)(bv.as.f >= 0.0 ? bv.as.f + 0.5 : bv.as.f - 0.5);
        v = rounded < 0 ? -1 : rounded;
        break;
      }
      default:
        free(vals);
        yis_trap("cogito.nav_rail_set_badges expects int/bool/null entries");
    }
    vals[i] = v;
  }
  cogito_nav_rail_set_badges(rail, vals, count);
  free(vals);
}

static void __cogito_nav_rail_set_toggle(YisVal railv, YisVal onv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_toggle(rail, yis_as_bool(onv));
}

static void __cogito_nav_rail_set_divider(YisVal railv, YisVal onv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_divider(rail, yis_as_bool(onv));
}

static void __cogito_nav_rail_set_selected(YisVal railv, YisVal idxv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_selected(rail, (int)yis_as_int(idxv));
}

static YisVal __cogito_nav_rail_get_selected(YisVal railv) {
  cogito_node* rail = cogito_node_from_val(railv);
  return YV_INT(cogito_nav_rail_get_selected(rail));
}

static YisVal __cogito_bottom_nav(void) {
  cogito_node* n = cogito_bottom_nav_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_nav_rail_on_change(YisVal railv, YisVal handler) {
  cogito_node* rail = cogito_node_from_val(railv);
  CogitoHandle* h = (CogitoHandle*)railv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_nav_rail_on_change(rail, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_nav_rail_on_change(rail, cogito_cb_select, h);
}

static void __cogito_bottom_nav_set_items(YisVal navv, YisVal labelsv, YisVal iconsv) {
  cogito_node* nav = cogito_node_from_val(navv);
  if (labelsv.tag != EVT_ARR) yis_trap("cogito.bottom_nav_set_items expects array of labels");
  YisArr* labels = (YisArr*)labelsv.as.p;
  size_t count = labels->len;
  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  YisStr** label_temps = (YisStr**)calloc(count, sizeof(YisStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i] = cogito_required_cstr(labels->items[i], &label_temps[i]);
  }
  const char** icon_strs = NULL;
  YisStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    YisArr* icons = (YisArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (YisStr**)calloc(icon_count, sizeof(YisStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i] = cogito_required_cstr(icons->items[i], &icon_temps[i]);
    }
    cogito_bottom_nav_set_items(nav, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i]) yis_release_val(YV_STR(icon_temps[i]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_bottom_nav_set_items(nav, label_strs, NULL, count);
  }
  for (size_t i = 0; i < count; i++) {
    if (label_temps[i]) yis_release_val(YV_STR(label_temps[i]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_bottom_nav_set_selected(YisVal navv, YisVal idxv) {
  cogito_node* nav = cogito_node_from_val(navv);
  cogito_bottom_nav_set_selected(nav, (int)yis_as_int(idxv));
}

static YisVal __cogito_bottom_nav_get_selected(YisVal navv) {
  cogito_node* nav = cogito_node_from_val(navv);
  return YV_INT(cogito_bottom_nav_get_selected(nav));
}

static void __cogito_bottom_nav_on_change(YisVal navv, YisVal handler) {
  cogito_node* nav = cogito_node_from_val(navv);
  CogitoHandle* h = (CogitoHandle*)navv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_bottom_nav_on_change(nav, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_bottom_nav_on_change(nav, cogito_cb_select, h);
}

static void __cogito_view_switcher_set_active(YisVal viewv, YisVal idv) {
  cogito_node* view = cogito_node_from_val(viewv);
  YisStr* tmp = NULL;
  const char* id = cogito_required_cstr(idv, &tmp);
  cogito_view_switcher_set_active(view, id);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_view_switcher_add(YisVal vsv, YisVal idv, YisVal builderv) {
  cogito_node* nvs = cogito_node_from_val(vsv);
  cogito_view_switcher_add_lazy_yis(nvs, idv, builderv);
}

static void __cogito_view_switcher_add_lazy(YisVal vsv, YisVal idv, YisVal builderv) {
  __cogito_view_switcher_add(vsv, idv, builderv);
}

static void __cogito_progress_set_value(YisVal pv, YisVal valuev) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_value(p, yis_as_float(valuev));
}

static YisVal __cogito_progress_get_value(YisVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_FLOAT(cogito_progress_get_value(p));
}

static void __cogito_progress_set_indeterminate(YisVal pv, YisVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_indeterminate(p, yis_as_bool(onv));
}

static YisVal __cogito_progress_get_indeterminate(YisVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_indeterminate(p));
}

static void __cogito_progress_set_thickness(YisVal pv, YisVal pxv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_thickness(p, (int)yis_as_int(pxv));
}

static YisVal __cogito_progress_get_thickness(YisVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_INT(cogito_progress_get_thickness(p));
}

static void __cogito_progress_set_wavy(YisVal pv, YisVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_wavy(p, yis_as_bool(onv));
}

static YisVal __cogito_progress_get_wavy(YisVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_wavy(p));
}

static void __cogito_progress_set_circular(YisVal pv, YisVal onv) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_circular(p, yis_as_bool(onv));
}

static YisVal __cogito_progress_get_circular(YisVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return YV_BOOL(cogito_progress_get_circular(p));
}

static void __cogito_toast_set_text(YisVal tv, YisVal textv) {
  cogito_node* t = cogito_node_from_val(tv);
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_toast_set_text(t, text);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_toast_on_click(YisVal tv, YisVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_toast_on_click(t, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_toast_on_click(t, cogito_cb_click, h);
}

static void __cogito_toast_set_action(YisVal tv, YisVal textv, YisVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  YisStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_toast_set_action(t, text, NULL, NULL);
    if (tmp) yis_release_val(YV_STR(tmp));
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_toast_set_action(t, text, cogito_cb_action, h);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_list_on_select(YisVal listv, YisVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_list_on_select(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_list_on_select(list, cogito_cb_select, h);
}

static void __cogito_content_list_on_activate(YisVal listv, YisVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_list_on_activate(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_list_on_activate(list, cogito_cb_activate, h);
}

static void __cogito_grid_on_select(YisVal gridv, YisVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_grid_on_select(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_grid_on_select(grid, cogito_cb_select, h);
}

static void __cogito_grid_on_activate(YisVal gridv, YisVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_grid_on_activate(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_grid_on_activate(grid, cogito_cb_activate, h);
}

static void __cogito_build(YisVal nodev, YisVal builder) {
  if (builder.tag != EVT_FN) yis_trap("cogito.build expects function");
  YisVal arg = nodev;
  yis_retain_val(arg);
  YisVal ret = yis_call(builder, 1, &arg);
  yis_release_val(arg);
  yis_release_val(ret);
}

static YisVal __cogito_state_new(YisVal initial) { return cogito_state_new_val(initial);
}
static YisVal __cogito_state_get(YisVal state) { return cogito_state_get_val(state);
}
static void __cogito_state_set(YisVal state, YisVal value) { cogito_state_set_val(state, value);
}

static void __cogito_timer_cb_call_yis(void* user) {
  YisFn* fn = (YisFn*)user;
  if (!fn) return;
  YisVal ret = yis_call(YV_FN(fn), 0, NULL);
  yis_release_val(ret);
}

static void __cogito_timer_cb_release_yis(void* user) {
  YisFn* fn = (YisFn*)user;
  if (!fn) return;
  yis_release_val(YV_FN(fn));
}

static YisVal __cogito_timer_timeout(YisVal delayv, YisVal handlerv) {
  if (handlerv.tag != EVT_FN) yis_trap("cogito.set_timeout expects function");
  int64_t delay = yis_as_int(delayv);
  if (delay < 0) delay = 0;
  YisFn* fn = (YisFn*)handlerv.as.p;
  yis_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_timeout_ex((uint32_t)delay,
                                            __cogito_timer_cb_call_yis, fn,
                                            __cogito_timer_cb_release_yis);
  if (id == 0) yis_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static YisVal __cogito_timer_interval(YisVal delayv, YisVal handlerv) {
  if (handlerv.tag != EVT_FN) yis_trap("cogito.set_interval expects function");
  int64_t delay = yis_as_int(delayv);
  if (delay < 0) delay = 0;
  YisFn* fn = (YisFn*)handlerv.as.p;
  yis_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_interval_ex((uint32_t)delay,
                                             __cogito_timer_cb_call_yis, fn,
                                             __cogito_timer_cb_release_yis);
  if (id == 0) yis_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static YisVal __cogito_timer_timeout_for(YisVal ownerv, YisVal delayv, YisVal handlerv) {
  if (handlerv.tag != EVT_FN) yis_trap("cogito.set_timeout_for expects function");
  cogito_node* owner = cogito_node_from_val(ownerv);
  int64_t delay = yis_as_int(delayv);
  if (delay < 0) delay = 0;
  YisFn* fn = (YisFn*)handlerv.as.p;
  yis_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_timeout_for_ex(
      owner, (uint32_t)delay, __cogito_timer_cb_call_yis, fn,
      __cogito_timer_cb_release_yis);
  if (id == 0) yis_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static YisVal __cogito_timer_interval_for(YisVal ownerv, YisVal delayv, YisVal handlerv) {
  if (handlerv.tag != EVT_FN) yis_trap("cogito.set_interval_for expects function");
  cogito_node* owner = cogito_node_from_val(ownerv);
  int64_t delay = yis_as_int(delayv);
  if (delay < 0) delay = 0;
  YisFn* fn = (YisFn*)handlerv.as.p;
  yis_retain_val(YV_FN(fn));
  uint64_t id = cogito_timer_set_interval_for_ex(
      owner, (uint32_t)delay, __cogito_timer_cb_call_yis, fn,
      __cogito_timer_cb_release_yis);
  if (id == 0) yis_release_val(YV_FN(fn));
  return YV_INT((int64_t)id);
}

static YisVal __cogito_timer_cancel(YisVal timer_id_v) {
  cogito_timer_id id = (cogito_timer_id)yis_as_int(timer_id_v);
  bool ok = cogito_timer_clear(id);
  return YV_BOOL(ok);
}

static void __cogito_timer_cancel_for(YisVal ownerv) {
  cogito_node* owner = cogito_node_from_val(ownerv);
  cogito_timer_clear_for(owner);
}

static void __cogito_timer_cancel_all(void) {
  cogito_timer_clear_all();
}

static void __cogito_run(YisVal appv, YisVal winv) {
  cogito_app* app = cogito_app_from_val(appv);
  cogito_window* win = cogito_window_from_val(winv);
  cogito_app_run(app, win);
}

static void __cogito_set_ensor_variant(YisVal appv, YisVal variantv) {
  cogito_app* app = cogito_app_from_val(appv);
  if (app) cogito_app_set_ensor_variant(app, (int)yis_as_int(variantv));
}

static void __cogito_load_sum(YisVal pathv) {
  YisStr* tmp = NULL;
  const char* path = cogito_required_cstr(pathv, &tmp);
  cogito_load_sum_file(path);
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_set_script_dir(YisVal dirv) {
  YisStr* tmp = NULL;
  const char* dir = cogito_optional_cstr(dirv, &tmp);
  if (dir && dir[0]) cogito_set_script_dir(dir); // Call the actual cogito library function
  if (tmp) yis_release_val(YV_STR(tmp));
}

static void __cogito_set_direction(YisVal dirv) {
  int dir = (int)yis_as_int(dirv);
  cogito_set_direction(dir);
}

static YisVal __cogito_get_direction(void) {
  return YV_INT(cogito_get_direction());
}

static YisVal __cogito_open_url(YisVal urlv) {
  YisStr* tmp = NULL;
  const char* url = cogito_optional_cstr(urlv, &tmp);
  bool ok = false;
  if (url && url[0]) ok = cogito_open_url(url);
  if (tmp) yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __cogito_gst_init(void) {
  return YV_BOOL(cogito_gst_init());
}

static void __cogito_gst_shutdown(void) {
  cogito_gst_shutdown();
}

static YisVal __cogito_gst_load(YisVal path_or_uri_v) {
  YisStr* tmp = NULL;
  const char* path_or_uri = cogito_optional_cstr(path_or_uri_v, &tmp);
  bool ok = false;
  if (path_or_uri && path_or_uri[0]) ok = cogito_gst_load(path_or_uri);
  if (tmp) yis_release_val(YV_STR(tmp));
  return YV_BOOL(ok);
}

static YisVal __cogito_gst_play(void) { return YV_BOOL(cogito_gst_play()); }

static YisVal __cogito_gst_pause(void) { return YV_BOOL(cogito_gst_pause()); }

static YisVal __cogito_gst_stop(void) { return YV_BOOL(cogito_gst_stop()); }

static YisVal __cogito_gst_seek_ms(YisVal position_ms_v) {
  int64_t position_ms = yis_as_int(position_ms_v);
  return YV_BOOL(cogito_gst_seek_ms(position_ms));
}

static YisVal __cogito_gst_get_position_ms(void) {
  return YV_INT(cogito_gst_get_position_ms());
}

static YisVal __cogito_gst_get_duration_ms(void) {
  return YV_INT(cogito_gst_get_duration_ms());
}

static YisVal __cogito_gst_set_volume(YisVal value_v) {
  double value = yis_as_float(value_v);
  return YV_BOOL(cogito_gst_set_volume(value));
}

static YisVal __cogito_gst_get_volume(void) {
  return YV_FLOAT(cogito_gst_get_volume());
}

static YisVal __cogito_gst_poll_event(void) {
  return YV_INT((int64_t)cogito_gst_poll_event());
}

static YisVal __cogito_gst_last_error(void) {
  const char* err = cogito_gst_last_error();
  if (!err || !err[0]) return YV_STR(stdr_str_lit(""));
  return YV_STR(stdr_str_lit(err));
}

static YisVal __cogito_gst_last_cover_path(void) {
  const char* path = cogito_gst_last_cover_path();
  if (!path || !path[0]) return YV_STR(stdr_str_lit(""));
  return YV_STR(stdr_str_lit(path));
}
// ---- Codegen aliases ----
#define cogito_app_new __cogito_app
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_app_set_dark_mode __cogito_app_set_dark_mode
#define cogito_app_set_contrast __cogito_app_set_contrast
#define cogito_app_set_ensor_variant __cogito_app_set_ensor_variant
#define cogito_app_set_icon __cogito_app_set_icon
#define cogito_app_get_icon __cogito_app_get_icon
#define cogito_open_url __cogito_open_url
#define cogito_gst_init __cogito_gst_init
#define cogito_gst_shutdown __cogito_gst_shutdown
#define cogito_gst_load __cogito_gst_load
#define cogito_gst_play __cogito_gst_play
#define cogito_gst_pause __cogito_gst_pause
#define cogito_gst_stop __cogito_gst_stop
#define cogito_gst_seek_ms __cogito_gst_seek_ms
#define cogito_gst_get_position_ms __cogito_gst_get_position_ms
#define cogito_gst_get_duration_ms __cogito_gst_get_duration_ms
#define cogito_gst_set_volume __cogito_gst_set_volume
#define cogito_gst_get_volume __cogito_gst_get_volume
#define cogito_gst_poll_event __cogito_gst_poll_event
#define cogito_gst_last_error __cogito_gst_last_error
#define cogito_gst_last_cover_path __cogito_gst_last_cover_path
#define cogito_window_new __cogito_window
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_a11y_label __cogito_window_set_a11y_label
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_button_new __cogito_button
#define cogito_iconbtn_new __cogito_iconbtn
#define cogito_fab_new __cogito_fab
#define cogito_label_new __cogito_label
#define cogito_dialog_new __cogito_dialog
#define cogito_dialog_slot_new __cogito_dialog_slot
#define cogito_image_new __cogito_image
#define cogito_checkbox_new __cogito_checkbox
#define cogito_switch_new __cogito_switch
#define cogito_textfield_new __cogito_textfield
#define cogito_searchfield_new __cogito_searchfield
#define cogito_textview_new __cogito_textview
#define cogito_dropdown_new __cogito_dropdown
#define cogito_datepicker_new __cogito_datepicker
#define cogito_stepper_new __cogito_stepper
#define cogito_slider_new __cogito_slider
#define cogito_slider_range_new __cogito_slider_range
#define cogito_tabs_new __cogito_tabs
#define cogito_nav_rail_new __cogito_nav_rail
#define cogito_bottom_nav_new __cogito_bottom_nav
#define cogito_buttongroup_new __cogito_buttongroup
#define cogito_view_switcher_new __cogito_view_switcher
#define cogito_progress_new __cogito_progress
#define cogito_treeview_new __cogito_treeview
#define cogito_colorpicker_new __cogito_colorpicker
#define cogito_toasts_new __cogito_toasts
#define cogito_toast_new __cogito_toast
#define cogito_appbar_new __cogito_appbar
#define cogito_toolbar_new __cogito_toolbar
#define cogito_vstack_new __cogito_vstack
#define cogito_hstack_new __cogito_hstack
#define cogito_zstack_new __cogito_zstack
#define cogito_fixed_new __cogito_fixed
#define cogito_scroller_new __cogito_scroller
#define cogito_carousel_new __cogito_carousel
#define cogito_carousel_item_new __cogito_carousel_item
#define cogito_carousel_item_set_text __cogito_carousel_item_set_text
#define cogito_carousel_item_set_halign __cogito_carousel_item_set_halign
#define cogito_carousel_item_set_valign __cogito_carousel_item_set_valign
#define cogito_carousel_set_active_index __cogito_carousel_set_active_index
#define cogito_carousel_get_active_index __cogito_carousel_get_active_index
#define cogito_list_new __cogito_list
#define cogito_grid_new __cogito_grid
#define cogito_container_add __cogito_container_add
#define cogito_container_set_margins __cogito_container_set_margins
#define cogito_container_set_align __cogito_container_set_align
#define cogito_container_set_halign __cogito_container_set_halign
#define cogito_container_set_valign __cogito_container_set_valign
#define cogito_container_set_hexpand __cogito_container_set_hexpand
#define cogito_container_set_vexpand __cogito_container_set_vexpand
#define cogito_container_set_gap __cogito_container_set_gap
#define cogito_dialog_slot_show __cogito_dialog_slot_show
#define cogito_dialog_slot_clear __cogito_dialog_slot_clear
#define cogito_container_set_padding __cogito_container_set_padding
#define cogito_fixed_set_pos __cogito_fixed_set_pos
#define cogito_scroller_set_axes __cogito_scroller_set_axes
#define cogito_scroller_set_sync __cogito_scroller_set_sync
#define cogito_grid_set_gap __cogito_grid_set_gap
#define cogito_grid_set_span __cogito_grid_set_span
#define cogito_grid_set_align __cogito_grid_set_align
#define cogito_label_set_class __cogito_label_set_class
#define cogito_label_set_text __cogito_label_set_text
#define cogito_label_set_wrap __cogito_label_set_wrap
#define cogito_label_set_ellipsis __cogito_label_set_ellipsis
#define cogito_label_set_align __cogito_label_set_align
#define cogito_node_set_disabled __cogito_node_set_disabled
#define cogito_node_set_editable __cogito_node_set_editable
#define cogito_node_get_editable __cogito_node_get_editable
#define cogito_node_set_id __cogito_node_set_id
#define cogito_node_set_class __cogito_node_set_class
#define cogito_node_set_a11y_label __cogito_node_set_a11y_label
#define cogito_node_set_a11y_role __cogito_node_set_a11y_role
#define cogito_node_set_tooltip_val __cogito_node_set_tooltip
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_app_set_dark_mode __cogito_app_set_dark_mode
#define cogito_app_set_contrast __cogito_app_set_contrast
#define cogito_app_set_ensor_variant __cogito_app_set_ensor_variant
#define cogito_app_set_icon __cogito_app_set_icon
#define cogito_app_get_icon __cogito_app_get_icon
#define cogito_pointer_capture_set __cogito_pointer_capture
#define cogito_pointer_capture_clear __cogito_pointer_release
#define cogito_view_switcher_set_active __cogito_view_switcher_set_active
#define cogito_view_switcher_add __cogito_view_switcher_add
#define cogito_textfield_set_text __cogito_textfield_set_text
#define cogito_textfield_get_text __cogito_textfield_get_text
#define cogito_textfield_set_hint __cogito_textfield_set_hint
#define cogito_textfield_get_hint __cogito_textfield_get_hint
#define cogito_searchfield_set_text __cogito_searchfield_set_text
#define cogito_searchfield_get_text __cogito_searchfield_get_text
#define cogito_searchfield_on_change __cogito_searchfield_on_change
#define cogito_textfield_on_change __cogito_textfield_on_change
#define cogito_textview_set_text __cogito_textview_set_text
#define cogito_textview_get_text __cogito_textview_get_text
#define cogito_textview_on_change __cogito_textview_on_change
#define cogito_datepicker_on_change __cogito_datepicker_on_change
#define cogito_dropdown_set_items __cogito_dropdown_set_items
#define cogito_dropdown_set_selected __cogito_dropdown_set_selected
#define cogito_dropdown_get_selected __cogito_dropdown_get_selected
#define cogito_dropdown_on_change __cogito_dropdown_on_change
#define cogito_slider_set_value __cogito_slider_set_value
#define cogito_slider_get_value __cogito_slider_get_value
#define cogito_slider_set_size __cogito_slider_set_size
#define cogito_slider_get_size __cogito_slider_get_size
#define cogito_slider_set_icon __cogito_slider_set_icon
#define cogito_slider_set_centered __cogito_slider_set_centered
#define cogito_slider_get_centered __cogito_slider_get_centered
#define cogito_slider_set_range __cogito_slider_set_range
#define cogito_slider_set_range_start __cogito_slider_set_range_start
#define cogito_slider_set_range_end __cogito_slider_set_range_end
#define cogito_slider_get_range_start __cogito_slider_get_range_start
#define cogito_slider_get_range_end __cogito_slider_get_range_end
#define cogito_slider_on_change __cogito_slider_on_change
#define cogito_colorpicker_on_change __cogito_colorpicker_on_change
#define cogito_tabs_set_items __cogito_tabs_set_items
#define cogito_tabs_set_ids __cogito_tabs_set_ids
#define cogito_tabs_set_selected __cogito_tabs_set_selected
#define cogito_tabs_get_selected __cogito_tabs_get_selected
#define cogito_tabs_on_change __cogito_tabs_on_change
#define cogito_tabs_bind __cogito_tabs_bind
#define cogito_nav_rail_set_items __cogito_nav_rail_set_items
#define cogito_nav_rail_set_badges __cogito_nav_rail_set_badges
#define cogito_nav_rail_set_toggle __cogito_nav_rail_set_toggle
#define cogito_nav_rail_set_divider __cogito_nav_rail_set_divider
#define cogito_nav_rail_set_selected __cogito_nav_rail_set_selected
#define cogito_nav_rail_get_selected __cogito_nav_rail_get_selected
#define cogito_nav_rail_on_change __cogito_nav_rail_on_change
#define cogito_nav_rail_set_no_label __cogito_nav_rail_set_no_label
#define cogito_nav_rail_get_no_label __cogito_nav_rail_get_no_label
#define cogito_bottom_nav_set_items __cogito_bottom_nav_set_items
#define cogito_bottom_nav_set_selected __cogito_bottom_nav_set_selected
#define cogito_bottom_nav_get_selected __cogito_bottom_nav_get_selected
#define cogito_bottom_nav_on_change __cogito_bottom_nav_on_change
#define cogito_progress_set_value __cogito_progress_set_value
#define cogito_progress_get_value __cogito_progress_get_value
#define cogito_progress_set_indeterminate __cogito_progress_set_indeterminate
#define cogito_progress_get_indeterminate __cogito_progress_get_indeterminate
#define cogito_progress_set_thickness __cogito_progress_set_thickness
#define cogito_progress_get_thickness __cogito_progress_get_thickness
#define cogito_progress_set_wavy __cogito_progress_set_wavy
#define cogito_progress_get_wavy __cogito_progress_get_wavy
#define cogito_progress_set_circular __cogito_progress_set_circular
#define cogito_progress_get_circular __cogito_progress_get_circular
#define cogito_toast_set_text __cogito_toast_set_text
#define cogito_toast_on_click __cogito_toast_on_click
#define cogito_toast_set_action __cogito_toast_set_action
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_dialog __cogito_window_set_dialog
#define cogito_window_clear_dialog __cogito_window_clear_dialog
#define cogito_window_set_side_sheet __cogito_window_set_side_sheet
#define cogito_window_clear_side_sheet __cogito_window_clear_side_sheet
#define cogito_side_sheet_set_mode __cogito_side_sheet_set_mode
#define cogito_node_window_val __cogito_node_window
#define cogito_find_parent __cogito_find_parent
#define cogito_find_children __cogito_find_children
#define cogito_build __cogito_build
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_state_new __cogito_state_new
#define cogito_state_get __cogito_state_get
#define cogito_state_set __cogito_state_set
#define cogito_button_set_text __cogito_button_set_text
#define cogito_button_set_size __cogito_button_set_size
#define cogito_button_get_size __cogito_button_get_size
#define cogito_image_set_icon __cogito_image_set_icon
#define cogito_image_set_size __cogito_image_set_size
#define cogito_image_set_radius __cogito_image_set_radius
#define cogito_image_set_alt_text __cogito_image_set_alt_text
#define cogito_webview_new __cogito_webview
#define cogito_webview_set_url __cogito_webview_set_url
#define cogito_webview_get_url __cogito_webview_get_url
#define cogito_webview_set_open_external_on_click                            \
  __cogito_webview_set_open_external_on_click
#define cogito_webview_get_open_external_on_click                            \
  __cogito_webview_get_open_external_on_click
#define cogito_webview_open __cogito_webview_open
#define cogito_checkbox_set_checked __cogito_checkbox_set_checked
#define cogito_checkbox_get_checked __cogito_checkbox_get_checked
#define cogito_switch_set_checked __cogito_switch_set_checked
#define cogito_switch_get_checked __cogito_switch_get_checked
#define cogito_checkbox_on_change __cogito_checkbox_on_change
#define cogito_switch_on_change __cogito_switch_on_change
#define cogito_list_on_select __cogito_list_on_select
#define cogito_list_on_activate __cogito_content_list_on_activate
#define cogito_grid_on_select __cogito_grid_on_select
#define cogito_grid_on_activate __cogito_grid_on_activate
void __cogito_button_on_click(YisVal btn, YisVal handler);
#define cogito_button_on_click __cogito_button_on_click
#define cogito_button_add_menu __cogito_button_add_menu
#define cogito_button_add_menu_section __cogito_button_add_menu_section
#define cogito_iconbtn_add_menu __cogito_iconbtn_add_menu
#define cogito_iconbtn_add_menu_section __cogito_iconbtn_add_menu_section
#define cogito_iconbtn_set_shape __cogito_iconbtn_set_shape
#define cogito_iconbtn_get_shape __cogito_iconbtn_get_shape
#define cogito_iconbtn_set_color_style __cogito_iconbtn_set_color_style
#define cogito_iconbtn_get_color_style __cogito_iconbtn_get_color_style
#define cogito_iconbtn_set_width __cogito_iconbtn_set_width
#define cogito_iconbtn_get_width __cogito_iconbtn_get_width
#define cogito_iconbtn_set_toggle __cogito_iconbtn_set_toggle
#define cogito_iconbtn_get_toggle __cogito_iconbtn_get_toggle
#define cogito_iconbtn_set_checked __cogito_iconbtn_set_checked
#define cogito_iconbtn_get_checked __cogito_iconbtn_get_checked
#define cogito_iconbtn_set_size __cogito_iconbtn_set_size
#define cogito_iconbtn_get_size __cogito_iconbtn_get_size
#define cogito_iconbtn_on_click __cogito_iconbtn_on_click
#define cogito_fab_set_size __cogito_fab_set_size
#define cogito_fab_set_color __cogito_fab_set_color
#define cogito_fab_set_extended __cogito_fab_set_extended
#define cogito_fab_on_click __cogito_fab_on_click
#define cogito_fab_menu_new __cogito_fab_menu
#define cogito_fab_menu_add_item __cogito_fab_menu_add_item
#define cogito_fab_menu_set_color __cogito_fab_menu_set_color
#define cogito_chip_new __cogito_chip
#define cogito_chip_set_selected __cogito_chip_set_selected
#define cogito_chip_get_selected __cogito_chip_get_selected
#define cogito_chip_set_closable __cogito_chip_set_closable
#define cogito_chip_on_click __cogito_chip_on_click
#define cogito_chip_on_close __cogito_chip_on_close
#define cogito_divider_new __cogito_divider
#define cogito_card_new __cogito_card
#define cogito_avatar_new __cogito_avatar
#define cogito_avatar_set_image __cogito_avatar_set_image
#define cogito_badge_new __cogito_badge
#define cogito_badge_set_count __cogito_badge_set_count
#define cogito_badge_get_count __cogito_badge_get_count
#define cogito_banner_new __cogito_banner
#define cogito_banner_set_action __cogito_banner_set_action
#define cogito_banner_set_icon __cogito_banner_set_icon
#define cogito_bottom_sheet_new __cogito_bottom_sheet
#define cogito_side_sheet_new __cogito_side_sheet
#define cogito_timepicker_new __cogito_timepicker
#define cogito_timepicker_on_change __cogito_timepicker_on_change
#define cogito_timepicker_get_hour __cogito_timepicker_get_hour
#define cogito_timepicker_get_minute __cogito_timepicker_get_minute
#define cogito_timepicker_set_time __cogito_timepicker_set_time
#define cogito_appbar_add_button __cogito_appbar_add_button
#define cogito_appbar_set_controls __cogito_appbar_set_controls
#define cogito_appbar_set_title __cogito_appbar_set_title
#define cogito_appbar_set_subtitle __cogito_appbar_set_subtitle
#define cogito_iconbtn_add_menu __cogito_iconbtn_add_menu
#define cogito_run __cogito_run
#define cogito_timer_timeout __cogito_timer_timeout
#define cogito_timer_interval __cogito_timer_interval
#define cogito_timer_timeout_for __cogito_timer_timeout_for
#define cogito_timer_interval_for __cogito_timer_interval_for
#define cogito_timer_cancel __cogito_timer_cancel
#define cogito_timer_cancel_for __cogito_timer_cancel_for
#define cogito_timer_cancel_all __cogito_timer_cancel_all
#define cogito_load_sum __cogito_load_sum
#define cogito_set_script_dir __cogito_set_script_dir
#define cogito_set_direction __cogito_set_direction
#define cogito_get_direction __cogito_get_direction
#define cogito_active_indicator_new __cogito_active_indicator
#define cogito_switchbar_new __cogito_switchbar
#define cogito_switchbar_get_checked __cogito_switchbar_get_checked
#define cogito_switchbar_set_checked __cogito_switchbar_set_checked
#define cogito_switchbar_on_change __cogito_switchbar_on_change
#define cogito_drawing_area_new __cogito_drawing_area
#define cogito_drawing_area_on_press __cogito_drawing_area_on_press
#define cogito_drawing_area_on_drag __cogito_drawing_area_on_drag
#define cogito_drawing_area_on_release __cogito_drawing_area_on_release
#define cogito_drawing_area_on_draw __cogito_drawing_area_on_draw
#define cogito_drawing_area_get_x __cogito_drawing_area_get_x
#define cogito_drawing_area_get_y __cogito_drawing_area_get_y
#define cogito_drawing_area_get_pressed __cogito_drawing_area_get_pressed
#define cogito_drawing_area_clear __cogito_drawing_area_clear
#define cogito_canvas_set_color __cogito_canvas_set_color
#define cogito_canvas_set_line_width __cogito_canvas_set_line_width
#define cogito_canvas_line __cogito_canvas_line
#define cogito_canvas_rect __cogito_canvas_rect
#define cogito_canvas_fill_rect __cogito_canvas_fill_rect
#define cogito_shape_new __cogito_shape
#define cogito_shape_set_preset __cogito_shape_set_preset
#define cogito_shape_get_preset __cogito_shape_get_preset
#define cogito_shape_set_size __cogito_shape_set_size
#define cogito_shape_get_size __cogito_shape_get_size
#define cogito_shape_set_color __cogito_shape_set_color
#define cogito_shape_set_color_style __cogito_shape_set_color_style
#define cogito_shape_get_color_style __cogito_shape_get_color_style
#define cogito_shape_set_vertex __cogito_shape_set_vertex
#define cogito_shape_get_vertex_x __cogito_shape_get_vertex_x
#define cogito_shape_get_vertex_y __cogito_shape_get_vertex_y
#define cogito_content_list_new __cogito_content_list
#define cogito_empty_page_new __cogito_empty_page
#define cogito_empty_page_set_description __cogito_empty_page_set_description
#define cogito_empty_page_set_icon __cogito_empty_page_set_icon
#define cogito_empty_page_set_action __cogito_empty_page_set_action
#define cogito_tip_view_new __cogito_tip_view
#define cogito_tip_view_set_title __cogito_tip_view_set_title
#define cogito_settings_window_new __cogito_settings_window
#define cogito_settings_page_new __cogito_settings_page
#define cogito_settings_list_new __cogito_settings_list
#define cogito_settings_row_new __cogito_settings_row
#define cogito_welcome_screen_new __cogito_welcome_screen
#define cogito_welcome_screen_set_description __cogito_welcome_screen_set_description
#define cogito_welcome_screen_set_icon __cogito_welcome_screen_set_icon
#define cogito_welcome_screen_set_action __cogito_welcome_screen_set_action
#define cogito_view_dual_new __cogito_view_dual
#define cogito_view_dual_set_ratio __cogito_view_dual_set_ratio
#define cogito_view_chooser_new __cogito_view_chooser
#define cogito_view_chooser_set_items __cogito_view_chooser_set_items
#define cogito_view_chooser_bind __cogito_view_chooser_bind
#define cogito_about_window_new __cogito_about_window
#define cogito_about_window_set_icon __cogito_about_window_set_icon
#define cogito_about_window_set_description __cogito_about_window_set_description
#define cogito_about_window_set_website __cogito_about_window_set_website
#define cogito_about_window_set_issue_url __cogito_about_window_set_issue_url
#define cogito_menu_button_new __cogito_menu_button
#define cogito_menu_set_icon __cogito_menu_set_icon
#define cogito_menu_set_shortcut __cogito_menu_set_shortcut
#define cogito_menu_set_submenu __cogito_menu_set_submenu
#define cogito_menu_set_toggled __cogito_menu_set_toggled
#define cogito_split_button_new __cogito_split_button
#define cogito_split_button_add_menu __cogito_split_button_add_menu
#define cogito_split_button_add_menu_section __cogito_split_button_add_menu_section
#define cogito_split_button_set_size __cogito_split_button_set_size
#define cogito_split_button_set_variant __cogito_split_button_set_variant
#define cogito_buttongroup_set_size __cogito_buttongroup_set_size
#define cogito_buttongroup_get_size __cogito_buttongroup_get_size
#define cogito_buttongroup_set_shape __cogito_buttongroup_set_shape
#define cogito_buttongroup_get_shape __cogito_buttongroup_get_shape
#define cogito_buttongroup_set_connected __cogito_buttongroup_set_connected
#define cogito_buttongroup_get_connected __cogito_buttongroup_get_connected
// cogito_set_script_dir is provided by the cogito library
