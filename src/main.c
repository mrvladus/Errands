#include "global.h"
#include "utils.h"
#include "widgets/entry.h"
#include "widgets/headerbar.h"
#include "widgets/todolist.h"

// ---------- Main window ---------- //

void CreateWindow()
{
    // Load gsettings
    settings = g_settings_new(APP_ID);
    ConvertSettings();
    // Main window
    GtkWidget* win = adw_application_window_new(GTK_APPLICATION(g_application_get_default()));
    g_object_set(G_OBJECT(win), "title", "List", NULL);
    gtk_widget_set_size_request(win, 500, 500);
    g_settings_bind(settings, "width", win, "default-width", 0);
    g_settings_bind(settings, "height", win, "default-height", 0);
    // Construct interface
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_box_append(GTK_BOX(box), HeaderBar());
    gtk_box_append(GTK_BOX(box), Entry());
    gtk_box_append(GTK_BOX(box), TodoList());
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(win), box);
    // Show window
    gtk_window_present(GTK_WINDOW(win));
}

// ---------- Application creation ---------- //

int main(int argc, char* argv[])
{
    AdwApplication* app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(CreateWindow), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
