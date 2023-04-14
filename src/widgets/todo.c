#include "todo.h"
#include "../global.h"
#include "entry.h"
#include "todolist.h"

static void DeleteSubTodo(GtkButton* btn)
{
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
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteSubTodo), NULL);
    adw_action_row_add_prefix(ADW_ACTION_ROW(sub_todo), del_btn);

    return sub_todo;
}

static void SubEntryActivated(GtkEntry* entry, GtkWidget* parent)
{
    const char* text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
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

static void DeleteTodo(GtkButton* btn, GtkWidget* todo)
{
    adw_preferences_page_remove(ADW_PREFERENCES_PAGE(todos_list), ADW_PREFERENCES_GROUP(todo));
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
    g_signal_connect(del_btn, "clicked", G_CALLBACK(DeleteTodo), todo_group);
    adw_expander_row_add_prefix(ADW_EXPANDER_ROW(todo_row), del_btn);
    // Add entry for sub-todos
    adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), SubEntry(todo_row));
    // Add sub todos begining from second element
    for (int i = 1; todo_items[i]; i++) {
        adw_expander_row_add_row(ADW_EXPANDER_ROW(todo_row), SubTodo(todo_items[i], todo_row));
    }
    // Adw expander row to group
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(todo_group), todo_row);
    // Free memory
    g_free(todo_items);

    return ADW_PREFERENCES_GROUP(todo_group);
}
