#include "../main.h"

// Show about window
static void ActionAbout()
{
    adw_show_about_window(
        gtk_application_get_active_window(GTK_APPLICATION(g_application_get_default())),
        "application-name", "List",
        "application-icon", APP_ID,
        "version", VERSION,
        "copyright", "© 2023 Vlad Krupinsky",
        "website", "https://github.com/mrvladus/List",
        "license-type", GTK_LICENSE_MIT_X11,
        NULL);
}

// Exit application
static void ActionQuit()
{
    g_application_quit(g_application_get_default());
}

static void ThemeSwitcherToggle(GtkCheckButton* btn, int theme)
{
    AdwStyleManager* mgr = adw_style_manager_get_default();
    adw_style_manager_set_color_scheme(mgr, theme);
    g_settings_set_int(settings, "theme", theme);
}

static GtkWidget* ThemeSwitcher()
{
    GtkWidget* system_theme = gtk_check_button_new_with_label("System");
    g_signal_connect(system_theme, "toggled", G_CALLBACK(ThemeSwitcherToggle), (gpointer)0);

    GtkWidget* light_theme = gtk_check_button_new_with_label("Light");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(light_theme), GTK_CHECK_BUTTON(system_theme));
    g_signal_connect(light_theme, "toggled", G_CALLBACK(ThemeSwitcherToggle), (gpointer)1);

    GtkWidget* dark_theme = gtk_check_button_new_with_label("Dark");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(dark_theme), GTK_CHECK_BUTTON(system_theme));
    g_signal_connect(dark_theme, "toggled", G_CALLBACK(ThemeSwitcherToggle), (gpointer)4);

    // Set switcher button active depending on settings
    int theme = g_settings_get_int(settings, "theme");
    if (theme == 0) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(system_theme), TRUE);
    } else if (theme == 1) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(light_theme), TRUE);
    } else if (theme == 4) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(dark_theme), TRUE);
    }
    // Set app theme
    ThemeSwitcherToggle(NULL, theme);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), system_theme);
    gtk_box_append(GTK_BOX(box), light_theme);
    gtk_box_append(GTK_BOX(box), dark_theme);

    return box;
}

static GtkWidget* MainMenu()
{
    // Create menu model
    GMenu* main_menu = g_menu_new();
    // Create theme submenu
    GMenu* theme_menu = g_menu_new();
    g_menu_append_submenu(main_menu, "Theme", G_MENU_MODEL(theme_menu));

    // Create custom theme menu item
    GMenuItem* theme = g_menu_item_new("Theme", NULL);
    g_menu_item_set_attribute(theme, "custom", "s", "theme");
    g_menu_append_item(theme_menu, theme);

    // Create popover menu with model of main_menu
    GtkWidget* popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(main_menu));
    // Add theme switcher widget to theme submenu
    gtk_popover_menu_add_child(GTK_POPOVER_MENU(popover), ThemeSwitcher(), "theme");

    // About menu item
    g_menu_append(main_menu, "About", "app.about");
    GSimpleAction* action = g_simple_action_new("about", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(ActionAbout), NULL);
    g_action_map_add_action(G_ACTION_MAP(g_application_get_default()), G_ACTION(action));

    // Quit menu item
    g_menu_append(main_menu, "Quit", "app.quit");
    action = g_simple_action_new("quit", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(ActionQuit), NULL);
    g_action_map_add_action(G_ACTION_MAP(g_application_get_default()), G_ACTION(action));
    gtk_application_set_accels_for_action(GTK_APPLICATION(g_application_get_default()), "app.quit",
        (const char*[]) { "<Primary>Q", NULL });

    // Menu button
    GtkWidget* menu_btn = gtk_menu_button_new();
    g_object_set(G_OBJECT(menu_btn),
        "icon-name", "open-menu-symbolic",
        "popover", popover,
        NULL);

    return menu_btn;
}

GtkWidget* HeaderBar()
{
    GtkWidget* hb = gtk_header_bar_new();
    gtk_widget_add_css_class(hb, "flat");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hb), MainMenu());

    return hb;
}
