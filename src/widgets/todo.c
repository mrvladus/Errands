#include "todo.h"
#include "../global.h"
#include "entry.h"
#include "todolist.h"

AdwPreferencesGroup* Todo(const char* text)
{
    GtkWidget* todo_group = adw_preferences_group_new();

    GtkWidget* todo_row = adw_expander_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(todo_row), text);
    adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), Entry());

    adw_preferences_group_add(ADW_PREFERENCES_GROUP(todo_group), todo_row);

    GtkWidget* del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_object_set(G_OBJECT(del_btn), "valign", GTK_ALIGN_CENTER, NULL);
    // g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteTodo), todo);

    adw_expander_row_add_prefix(ADW_EXPANDER_ROW(todo_row), del_btn);

    return ADW_PREFERENCES_GROUP(todo_group);
}
