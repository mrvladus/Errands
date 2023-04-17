#include "todolist.h"
#include "../global.h"
#include "todo.h"

GtkWidget* TodoList()
{
    todos_list = adw_preferences_page_new();
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
