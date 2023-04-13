#include "todolist.h"
#include "../global.h"
#include "todo.h"

// Set list box insvisible if no todos (because of weird css shadow)
void UpdateTodoListVisibility()
{
    g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
    if (g_strv_length(todos) > 0) {
        gtk_widget_set_visible(GTK_WIDGET(todos_list), TRUE);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(todos_list), FALSE);
    }
}

GtkWidget* TodoList()
{
    todos_list = gtk_list_box_new();
    gtk_widget_add_css_class(todos_list, "boxed-list");
    g_object_set(G_OBJECT(todos_list),
        "valign", GTK_ALIGN_START,
        "selection-mode", 0,
        "margin-top", 20,
        "margin-bottom", 20,
        "margin-start", 50,
        "margin-end", 50,
        NULL);
    // Fill new ones from gsettings
    g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
    for (int i = 0; todos[i]; i++) {
        gtk_list_box_append(GTK_LIST_BOX(todos_list), Todo(todos[i]));
    }
    UpdateTodoListVisibility();
    // Add list to the scrolled container
    GtkWidget* scrolled_win = gtk_scrolled_window_new();
    g_object_set(G_OBJECT(scrolled_win),
        "vexpand", TRUE,
        "hexpand", TRUE,
        NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), todos_list);

    return scrolled_win;
}
