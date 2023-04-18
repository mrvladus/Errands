#include "todo.h"
#include "../global.h"
#include "subtodo.h"
#include "todolist.h"

// Add sub todo when text is entered
static void SubEntryActivated(GtkEntry* entry, GtkWidget* parent)
{
    // Get entry text
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    // Check for empty string
    if (strlen(text) == 0)
        return;
    // Check if todo already exists
    const char* parent_text = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(parent));
    GVariant* old_todos = g_settings_get_value(settings, "todos-v2");
    for (int i = 0; i < g_variant_n_children(old_todos); i++) {
        // Get sub array
        GVariant* todo = g_variant_get_child_value(old_todos, i);
        const gchar** todos = g_variant_get_strv(todo, NULL);
        if (g_strcmp0(todos[0], parent_text) == 0)
            if (g_strv_length((gchar**)todos) > 1)
                for (int j = 1; todos[j]; j++)
                    // If sub-todo exists - return
                    if (g_strcmp0(todos[j], text) == 0) {
                        g_free(todos);
                        return;
                    }
        g_free(todos);
    }
    // Add new sub-todo to gsettings
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    // For each sub array
    for (int i = 0; i < g_variant_n_children(old_todos); i++) {
        // Get sub array
        GVariant* todo = g_variant_get_child_value(old_todos, i);
        const gchar** todos = g_variant_get_strv(todo, NULL);
        // Add new sub-todo
        if (g_strcmp0(todos[0], parent_text) == 0) {
            g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
            g_strv_builder_addv(builder, todos);
            g_strv_builder_add(builder, text);
            g_auto(GStrv) array = g_strv_builder_end(builder);
            GVariant* new_todo = g_variant_new_strv((const char* const*)array, -1);
            g_variant_builder_add_value(&g_var_builder, new_todo);
        } else {
            g_variant_builder_add_value(&g_var_builder, g_variant_get_child_value(old_todos, i));
        }
        g_free(todos);
    }
    // Save new todos
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    g_settings_set_value(settings, "todos-v2", new_todos);
    adw_expander_row_add_row(ADW_EXPANDER_ROW(parent), SubTodo(text, parent));
    // Clear entry
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

// Delete main todo
static void DeleteTodo(GtkButton* btn)
{
    // Create builder for new todos array
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    // Get todos from settings
    GVariant* old_todos = g_settings_get_value(settings, "todos-v2");
    int num_of_old_todos = g_variant_n_children(old_todos);
    // If only one todo left: just set empty array in gsettings
    if (num_of_old_todos == 1)
        g_settings_set(settings, "todos-v2", "aas", (const gchar**) { NULL });
    // Else remove only one from array
    else {
        // For each todo
        for (int i = 0; i < num_of_old_todos; i++) {
            // Get sub array
            const gchar** todo_items = g_variant_get_strv(
                g_variant_get_child_value(old_todos, i), NULL);
            // Get todo text
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
        g_settings_set_value(settings, "todos-v2", new_todos);
    }
    // Remove main todo row
    adw_preferences_page_remove(ADW_PREFERENCES_PAGE(todos_list),
        ADW_PREFERENCES_GROUP(g_object_get_data(G_OBJECT(btn), "todo-group")));
}

// Preferences group containing expander row with todo text, sub-entry, and
// sub-todos if provided
AdwPreferencesGroup* Todo(const gchar** todo_items)
{
    // Create new todo group
    GtkWidget* todo_group = adw_preferences_group_new();

    // Create expander row
    GtkWidget* todo_row = adw_expander_row_new();
    // Set main todo text as first item of todo_items
    g_object_set(G_OBJECT(todo_row), "title", todo_items[0], NULL);
    // Adw expander row to group
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(todo_group), todo_row);

    // Add delete button
    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    g_object_set(G_OBJECT(del_btn),
        "valign", GTK_ALIGN_CENTER,
        "tooltip-text", "Delete task",
        NULL);
    g_object_set_data(G_OBJECT(del_btn), "todo-row", todo_row);
    g_object_set_data(G_OBJECT(del_btn), "todo-group", todo_group);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteTodo), NULL);
    adw_expander_row_add_prefix(ADW_EXPANDER_ROW(todo_row), del_btn);

    // Add entry for sub-todos
    GtkWidget* sub_entry = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(sub_entry), "Add new sub-task");
    g_signal_connect(sub_entry, "apply", G_CALLBACK(SubEntryActivated), todo_row);
    g_signal_connect(sub_entry, "entry-activated", G_CALLBACK(SubEntryActivated), todo_row);
    adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), sub_entry);

    // Add sub todos begining from second element
    int i;
    for (i = 1; todo_items[i]; i++)
        adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), SubTodo(todo_items[i], todo_row));
    // Expand row if sub-todos exists
    if (i > 1)
        g_object_set(G_OBJECT(todo_row), "expanded", TRUE, NULL);

    return ADW_PREFERENCES_GROUP(todo_group);
}
