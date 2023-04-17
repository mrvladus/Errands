#include "global.h"
#include "widgets/entry.h"
#include "widgets/headerbar.h"
#include "widgets/todolist.h"

// ---------- Compatability functions ---------- //

// Convert to a new gsettings todos-v2 format from versions 44.1.x to 44.2.x
void ConvertSettings()
{
    // Get old todos and transfer them to a new key "todos-v2"
    g_auto(GStrv) old_todos = g_settings_get_strv(settings, "todos");
    // Create new array of arrays
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    for (int i = 0; old_todos[i]; i++) {
        g_autoptr(GStrvBuilder) sub_builder = g_strv_builder_new();
        g_strv_builder_add(sub_builder, old_todos[i]);
        g_auto(GStrv) array = g_strv_builder_end(sub_builder);
        GVariant* new_todo = g_variant_new_strv((const char* const*)array, -1);
        g_variant_builder_add_value(&g_var_builder, new_todo);
    }
    // Finish building new todos array
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    // Save new todos to gsettings
    g_settings_set_value(settings, "todos-v2", new_todos);
}

// ---------- Main window ---------- //

void CreateWindow()
{
    // Load gsettings
    settings = g_settings_new(APP_ID);
    ConvertSettings();
    // Main window
    GtkWidget* win = adw_application_window_new(GTK_APPLICATION(g_application_get_default()));
    g_object_set(G_OBJECT(win), "title", "List", NULL);
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
