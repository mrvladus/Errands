#include "data.h"
#include "settings.h"
#include "task-list.h"
#include "vendor/toolbox.h"

// static gboolean task_matches_search_query(ErrandsData *data, const char *query) {
//   if (!query || STR_EQUAL(query, "")) return true;
//   const char *text = errands_data_get_prop(data, PROP_TEXT);
//   const char *notes = errands_data_get_prop(data, PROP_NOTES);
//   g_auto(GStrv) tags = errands_data_get_strv(data, PROP_TAGS);
//   if ((text && STR_CONTAINS_CASE(text, query)) || (notes && STR_CONTAINS_CASE(notes, query)) ||
//       (tags && g_strv_contains((const gchar *const *)tags, query)))
//     return true;
//   return false;
// }

// static gboolean task_or_descendants_match_search_query(ErrandsData *data, const char *query, GPtrArray *all_tasks) {
//   if (task_matches_search_query(data, query)) return true;
//   const char *uid = errands_data_get_prop(data, PROP_UID);
//   for_range(i, 0, all_tasks->len) {
//     ErrandsData *child = all_tasks->pdata[i];
//     const char *parent = errands_data_get_prop(child, PROP_PARENT);
//     if (parent && STR_EQUAL(parent, uid))
//       if (task_or_descendants_match_search_query(child, query, all_tasks)) return true;
//   }

//   return false;
// }

// static bool task_is_due(ErrandsData *data) {
//   icaltimetype due = errands_data_get_prop(data, PROP_DUE_TIME);
//   if (!icaltime_is_null_time(due))
//     if (icaltime_compare(due, icaltime_today()) <= 0) return true;

//   return false;
// }

// static bool task_or_descendants_is_due(ErrandsData *data, GPtrArray *all_tasks) {
//   if (task_is_due(data)) return true;
//   const char *uid = errands_data_get_prop(data, PROP_UID);
//   for_range(i, 0, all_tasks->len) {
//     ErrandsData *child = all_tasks->pdata[i];
//     const char *parent = errands_data_get_prop(child, PROP_PARENT);
//     if (parent && STR_EQUAL(parent, uid))
//       if (task_or_descendants_is_due(child, all_tasks)) return true;
//   }

//   return false;
// }

// static gboolean base_toplevel_filter_func(GObject *item) {
//   return errands_data_get_prop(g_object_get_data(item, "data"), PROP_PARENT) ? false : true;
// }

// static gboolean base_today_filter_func(GObject *item) {
//   ErrandsData *data = g_object_get_data(item, "data");
//   bool has_parent = errands_data_get_prop(data, PROP_PARENT) != NULL;
//   return task_is_due(data) && !has_parent;
// }

// // ---------- FILTER FUNCTIONS ---------- //

// static gboolean master_filter_func(GObject *item, ErrandsTaskList *self) {
//   g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
//   if (!model_item) return false;
//   ErrandsData *data = g_object_get_data(model_item, "data");
//   // Completed
//   if (!errands_settings_get("show_completed", SETTING_TYPE_BOOL).b)
//     if (!icaltime_is_null_date(errands_data_get_prop(data, PROP_COMPLETED_TIME))) return false;
//   // Toplevel
//   if (self->page == ERRANDS_TASK_LIST_PAGE_TASK_LIST) {
//     ErrandsData *list_data = task_data_get_list(data);
//     if (list_data != self->data) return false;
//   }

//   return true;
// }

// static gboolean all_tasks_filter_func(GObject *item, ErrandsTaskList *self) {
//   g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
//   if (!model_item) return false;
//   ErrandsData *data = g_object_get_data(model_item, "data");
//   // Completed
//   if (!errands_settings_get("show_completed", SETTING_TYPE_BOOL).b)
//     if (!icaltime_is_null_date(errands_data_get_prop(data, PROP_COMPLETED_TIME))) return false;

//   return true;
// }

// static gboolean today_filter_func(GObject *item, ErrandsTaskList *self) {
//   // g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
//   // if (!model_item) return false;
//   // ErrandsData *data = g_object_get_data(model_item, "data");
//   // if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY)
//   //   if (!task_or_descendants_is_due(data, self->all_tasks)) return false;

//   return true;
// }

// static gboolean search_filter_func(GObject *item, ErrandsTaskList *self) {
//   // g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
//   // if (!model_item) return false;
//   // ErrandsData *data = g_object_get_data(model_item, "data");
//   // if (!data) return false;
//   // if (self->search_query && !STR_EQUAL(self->search_query, ""))
//   //   if (!task_or_descendants_match_search_query(data, self->search_query, self->all_tasks)) return false;

//   return true;
// }
