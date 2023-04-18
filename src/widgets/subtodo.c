#include "subtodo.h"
#include "../global.h"

static void DeleteSubTodo(GtkButton* btn)
{
    // Get main todo text
    const char* parent_text = adw_preferences_row_get_title(
        ADW_PREFERENCES_ROW(g_object_get_data(G_OBJECT(btn), "parent-todo")));
    // Get sub-todo text
    const char* sub_todo_text = (const char*)g_object_get_data(G_OBJECT(btn), "sub-todo-text");
    // Create GVariant array builder
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    // Get todos from gsettings
    GVariant* old_todos = g_settings_get_value(settings, "todos-v2");
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
    g_settings_set_value(settings, "todos-v2", new_todos);
    // Remove sub todo row from main todo
    adw_expander_row_remove(
        ADW_EXPANDER_ROW(g_object_get_data(G_OBJECT(btn), "parent-todo")),
        g_object_get_data(G_OBJECT(btn), "sub-todo"));
}

GtkWidget* SubTodo(const char* text, GtkWidget* parent)
{
    // Sub todo row
    GtkWidget* sub_todo = adw_action_row_new();
    g_object_set(G_OBJECT(sub_todo), "title", text, NULL);

    // Add delete button
    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    g_object_set(G_OBJECT(del_btn),
        "valign", GTK_ALIGN_CENTER,
        "tooltip-text", "Delete sub-task",
        NULL);
    g_object_set_data(G_OBJECT(del_btn), "parent-todo", parent);
    g_object_set_data(G_OBJECT(del_btn), "sub-todo", sub_todo);
    g_object_set_data(G_OBJECT(del_btn), "sub-todo-text", (gpointer)text);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteSubTodo), NULL);
    adw_action_row_add_prefix(ADW_ACTION_ROW(sub_todo), del_btn);

    return sub_todo;
}
