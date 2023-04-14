#include "todolist.h"
#include "../global.h"
#include "todo.h"

// GtkWidget* TodoList()
// {
//     todos_list = gtk_list_box_new();
//     gtk_widget_add_css_class(todos_list, "boxed-list");
//     g_object_set(G_OBJECT(todos_list),
//         "valign", GTK_ALIGN_START,
//         "selection-mode", 0,
//         "margin-top", 20,
//         "margin-bottom", 20,
//         "margin-start", 50,
//         "margin-end", 50,
//         NULL);
//     // Fill new ones from gsettings
//     g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
//     for (int i = 0; todos[i]; i++) {
//         gtk_list_box_append(GTK_LIST_BOX(todos_list), Todo(todos[i]));
//     }
//     UpdateTodoListVisibility();
//     // Add list to the scrolled container
//     GtkWidget* scrolled_win = gtk_scrolled_window_new();
//     g_object_set(G_OBJECT(scrolled_win),
//         "vexpand", TRUE,
//         "hexpand", TRUE,
//         NULL);
//     gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), todos_list);

//     return scrolled_win;
// }

GtkWidget* TodoList()
{
    todos_list = adw_preferences_page_new();
    // Load todos as array [['todo1', 'sub1', 'sub2'], ['todo2', 'sub1', 'sub2']]
    GVariant* todos = g_settings_get_value(settings, "todos");
    // For each sub-array:
    for (int i = 0; i < g_variant_n_children(todos); i++) {
        // Get string array { 'todo1', 'sub1', 'sub2', NULL }
        const gchar** todo_items = g_variant_get_strv(g_variant_get_child_value(todos, i), NULL);
        // Create new todo row and pass array as arg
        adw_preferences_page_add(ADW_PREFERENCES_PAGE(todos_list), Todo(todo_items));
    }

    return todos_list;
}
