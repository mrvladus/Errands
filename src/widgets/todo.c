#include "todo.h"
#include "../global.h"
#include "todolist.h"

// Get all todos array and create new one without deleted todo
static void DeleteTodo(GtkButton* btn, gpointer todo)
{
    // Get todo text
    const char* text = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(todo));
    // Get all todos
    g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
    // Remove text from todos
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (int i = 0; todos[i]; i++) {
        if (g_strcmp0(todos[i], text) != 0) {
            g_strv_builder_add(builder, todos[i]);
        }
    }
    g_auto(GStrv) new_todos = g_strv_builder_end(builder);
    // Save new todos
    g_settings_set_strv(settings, "todos", (const gchar* const*)new_todos);
    // Remove row
    gtk_list_box_remove(GTK_LIST_BOX(todos_list), todo);
    UpdateTodoListVisibility();
}

GtkWidget* Todo(const char* text)
{
    GtkWidget* todo = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(todo), text);

    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_object_set(G_OBJECT(del_btn), "valign", GTK_ALIGN_CENTER, NULL);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteTodo), todo);

    adw_action_row_add_suffix(ADW_ACTION_ROW(todo), del_btn);

    return todo;
}
