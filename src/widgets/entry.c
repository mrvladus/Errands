#include "entry.h"
#include "../global.h"
#include "todo.h"

void EntryActivated(GtkEntry* entry)
{
    // Get text from entry
    const char* text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
    // Check for empty string
    if (strlen(text) == 0)
        return;
    // Add todo
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_add(builder, text);
    gchar** array = g_strv_builder_end(builder);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(todos_list), Todo((const gchar**)array));
    // Clear entry
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), "", -1);
}

GtkWidget* Entry()
{
    GtkWidget* entry = gtk_entry_new();
    g_object_set(G_OBJECT(entry),
        "secondary-icon-name", "list-add-symbolic",
        "secondary-icon-activatable", TRUE,
        "margin-start", 50,
        "margin-end", 50,
        NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(EntryActivated), NULL);
    g_signal_connect(entry, "icon-release", G_CALLBACK(EntryActivated), NULL);

    return entry;
}
