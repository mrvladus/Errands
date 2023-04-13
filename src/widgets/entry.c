#include "entry.h"
#include "../global.h"
#include "todo.h"
#include "todolist.h"

// void EntryActivated(GtkEntry* entry)
// {
//     // Get text from entry
//     const char* text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
//     // Check for empty string
//     if (strlen(text) == 0)
//         return;
//     // Check if todo already exists
//     g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
//     for (int i = 0; todos[i]; i++) {
//         if (g_strcmp0(text, todos[i]) == 0)
//             return;
//     }
//     // Prepend todo to todos in gsettings
//     g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
//     g_strv_builder_add(builder, text);
//     g_strv_builder_addv(builder, (const char**)todos);
//     g_auto(GStrv) new_todos = g_strv_builder_end(builder);
//     g_settings_set_strv(settings, "todos", (const gchar* const*)new_todos);
//     // Add todo row
//     gtk_list_box_prepend(GTK_LIST_BOX(todos_list), Todo(text));
//     UpdateTodoListVisibility();
//     // Clear entry
//     gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), "", -1);
// }

GtkWidget* Entry()
{
    GtkWidget* entry = gtk_entry_new();
    g_object_set(G_OBJECT(entry),
        "secondary-icon-name", "list-add-symbolic",
        "secondary-icon-activatable", TRUE,
        "margin-start", 50,
        "margin-end", 50,
        NULL);
    // g_signal_connect(entry, "activate", G_CALLBACK(EntryActivated), NULL);
    // g_signal_connect(entry, "icon-release", G_CALLBACK(EntryActivated), NULL);

    return entry;
}
