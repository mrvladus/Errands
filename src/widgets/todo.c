#include "todo.h"
#include "../global.h"
#include "todolist.h"

static void DeleteSubTodo(GtkButton* btn)
{
    const char* parent_text = adw_preferences_row_get_title(
        ADW_PREFERENCES_ROW(g_object_get_data(G_OBJECT(btn), "parent-todo")));
    // Get text
    const char* sub_todo_text = (const char*)g_object_get_data(G_OBJECT(btn), "sub-todo-text");

    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);

    GVariant* old_todos = g_settings_get_value(settings, "todos");
    // For each sub array
    for (int i = 0; i < g_variant_n_children(old_todos); i++) {
        // Get sub array
        const gchar** todo_items = g_variant_get_strv(
            g_variant_get_child_value(old_todos, i), NULL);

        g_autoptr(GStrvBuilder) sub_builder = g_strv_builder_new();
        // Add main todo text
        g_strv_builder_add(sub_builder, todo_items[0]);
        // Add sub-todos exept deleted
        if (g_strcmp0(todo_items[0], parent_text) == 0) {
            for (int j = 1; todo_items[j]; j++)
                if (g_strcmp0(todo_items[j], sub_todo_text) != 0)
                    g_strv_builder_add(sub_builder, todo_items[j]);
        } else {
            // Add other sub-todos
            for (int j = 1; todo_items[j]; j++)
                g_strv_builder_add(sub_builder, todo_items[j]);
        }
        // Finish building sub-array
        g_auto(GStrv) array = g_strv_builder_end(sub_builder);
        GVariant* new_sub_todo = g_variant_new_strv((const char* const*)array, -1);
        // Add sub array to array of todos
        g_variant_builder_add_value(&g_var_builder, new_sub_todo);
        // Free memory
        g_free(todo_items);
    }
    // Finish building new todos array
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    // Save new todos to gsettings
    g_settings_set_value(settings, "todos", new_todos);
    // Remove row
    adw_expander_row_remove(
        ADW_EXPANDER_ROW(g_object_get_data(G_OBJECT(btn), "parent-todo")),
        g_object_get_data(G_OBJECT(btn), "sub-todo"));
}

static GtkWidget* SubTodo(const char* text, GtkWidget* parent)
{
    GtkWidget* sub_todo = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(sub_todo), text);
    // Add delete button
    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    g_object_set(G_OBJECT(del_btn), "valign", GTK_ALIGN_CENTER, NULL);
    g_object_set_data(G_OBJECT(del_btn), "parent-todo", parent);
    g_object_set_data(G_OBJECT(del_btn), "sub-todo", sub_todo);
    g_object_set_data(G_OBJECT(del_btn), "sub-todo-text", (gpointer)text);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteSubTodo), NULL);
    adw_action_row_add_prefix(ADW_ACTION_ROW(sub_todo), del_btn);

    return sub_todo;
}

static void SubEntryActivated(GtkEntry* entry, GtkWidget* parent)
{
    const char* text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
    // Check for empty string
    if (strlen(text) == 0)
        return;
    adw_expander_row_add_row(ADW_EXPANDER_ROW(parent), SubTodo(text, parent));
    // Clear entry
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), "", -1);
}

static GtkWidget* SubEntry(GtkWidget* parent)
{
    GtkWidget* sub_entry = gtk_entry_new();
    g_object_set(G_OBJECT(sub_entry),
        "secondary-icon-name", "list-add-symbolic",
        "secondary-icon-activatable", TRUE,
        "margin-start", 5,
        "margin-end", 5,
        "margin-top", 5,
        "margin-bottom", 5,
        NULL);

    g_signal_connect(sub_entry, "activate", G_CALLBACK(SubEntryActivated), parent);
    g_signal_connect(sub_entry, "icon-release", G_CALLBACK(SubEntryActivated), parent);

    return sub_entry;
}

static void DeleteTodo(GtkButton* btn)
{
    // Create builder for new todos array
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    // Get todos from settings
    GVariant* old_todos = g_settings_get_value(settings, "todos");
    // For each todo
    for (int i = 0; i < g_variant_n_children(old_todos); i++) {
        const gchar** todo_items = g_variant_get_strv(
            g_variant_get_child_value(old_todos, i), NULL);

        const char* todo_text = adw_preferences_row_get_title(
            ADW_PREFERENCES_ROW(g_object_get_data(G_OBJECT(btn), "todo-row")));
        // Skip deleted todo
        if (g_strcmp0(todo_items[0], todo_text) == 0) {
            g_free(todo_items);
            continue;
        } else {
            // Add other todos
            g_variant_builder_add_value(&g_var_builder, g_variant_get_child_value(old_todos, i));
            g_free(todo_items);
        }
    }
    // Finish building new todos array
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    // Save new todos to gsettings
    g_settings_set_value(settings, "todos", new_todos);
    // Remove row
    adw_preferences_page_remove(ADW_PREFERENCES_PAGE(todos_list),
        ADW_PREFERENCES_GROUP(g_object_get_data(G_OBJECT(btn), "todo-group")));
}

AdwPreferencesGroup* Todo(const gchar** todo_items)
{
    // Create new todo group
    GtkWidget* todo_group = adw_preferences_group_new();
    // Create expander row
    GtkWidget* todo_row = adw_expander_row_new();
    // Set main todo text as first item of todo_items
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(todo_row), todo_items[0]);
    // Add delete button
    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_object_set(G_OBJECT(del_btn), "valign", GTK_ALIGN_CENTER, NULL);
    g_object_set_data(G_OBJECT(del_btn), "todo-row", todo_row);
    g_object_set_data(G_OBJECT(del_btn), "todo-group", todo_group);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteTodo), NULL);
    adw_expander_row_add_prefix(ADW_EXPANDER_ROW(todo_row), del_btn);
    // Add entry for sub-todos
    adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), SubEntry(todo_row));
    // Add sub todos begining from second element
    for (int i = 1; todo_items[i]; i++) {
        adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), SubTodo(todo_items[i], todo_row));
    }
    // Adw expander row to group
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(todo_group), todo_row);

    return ADW_PREFERENCES_GROUP(todo_group);
}
