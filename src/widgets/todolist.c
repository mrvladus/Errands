#include "todolist.h"
#include "../global.h"
#include "todo.h"

static void EntryActivated(AdwEntryRow* entry)
{
    // Get text from entry
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    // Check for empty string
    if (strlen(text) == 0)
        return;
    // Add new todo to gsettings
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    GVariant* old_todos = g_settings_get_value(settings, "todos");
    // For each sub array
    for (int i = 0; i < g_variant_n_children(old_todos); i++) {
        g_variant_builder_add_value(&g_var_builder, g_variant_get_child_value(old_todos, i));
    }
    // Add todo
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_add(builder, text);
    g_auto(GStrv) array = g_strv_builder_end(builder);
    // Add new todo text
    GVariant* new_todo = g_variant_new_strv((const char* const*)array, -1);
    g_variant_builder_add_value(&g_var_builder, new_todo);
    // Save new todos
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    g_settings_set_value(settings, "todos", new_todos);
    // Add row
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(todos_list), Todo((const gchar**)array));
    // Clear entry
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

GtkWidget* TodoList()
{
    todos_list = adw_preferences_page_new();
    // Add entry
    GtkWidget* entry = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(entry), "Add new task");
    g_signal_connect(entry, "apply", G_CALLBACK(EntryActivated), NULL);
    g_signal_connect(entry, "entry-activated", G_CALLBACK(EntryActivated), NULL);
    GtkWidget* entry_group = adw_preferences_group_new();
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(entry_group), entry);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(todos_list), ADW_PREFERENCES_GROUP(entry_group));
    // Load todos as array [['todo1', 'sub1', 'sub2'], ['todo2', 'sub1', 'sub2']]
    GVariant* todos = g_settings_get_value(settings, "todos");
    // For each sub-array:
    for (int i = 0; i < g_variant_n_children(todos); i++) {
        // Get string array { 'todo1', 'sub1', 'sub2', NULL }
        const gchar** todo_items = g_variant_get_strv(g_variant_get_child_value(todos, i), NULL);
        // Add new todo row and pass array as arg
        adw_preferences_page_add(ADW_PREFERENCES_PAGE(todos_list), Todo((const gchar**)todo_items));
    }

    return todos_list;
}
