#include <adwaita.h>

#define APP_ID "io.github.mrvladus.List"
#define VERSION "44.1"

// ---------- Functions defenition ---------- //

GtkWidget* HeaderBar();
GtkWidget* MainMenu();
GtkWidget* ThemeSwitcher();
void ThemeSwitcherToggle(GtkCheckButton* btn, int theme);
void ActionAbout();
void ActionQuit();
GtkWidget* Entry();
void EntryActivated(GtkEntry* entry);
GtkWidget* TodoList();
void UpdateTodoListVisibility();
GtkWidget* Todo(const char* text);
void DeleteTodo(GtkButton* btn, gpointer todo);
void CreateWindow();

// ---------- Global variables ---------- //

AdwApplication* app; // Application object
GSettings* settings; // GSettings object
GtkWidget* todos_list; // Todo's list box

// ---------- Header bar ---------- //

GtkWidget* HeaderBar()
{
    GtkWidget* hb = gtk_header_bar_new();
    gtk_widget_add_css_class(hb, "flat");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hb), MainMenu());

    return hb;
}

GtkWidget* MainMenu()
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
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));

    // Quit menu item
    g_menu_append(main_menu, "Quit", "app.quit");
    action = g_simple_action_new("quit", NULL);
    g_signal_connect(action, "activate", G_CALLBACK(ActionQuit), NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit",
        (const char*[]) { "<Primary>Q", NULL });

    // Menu button
    GtkWidget* menu_btn = gtk_menu_button_new();
    g_object_set(G_OBJECT(menu_btn),
        "icon-name", "open-menu-symbolic",
        "popover", popover,
        NULL);

    return menu_btn;
}

GtkWidget* ThemeSwitcher()
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

void ThemeSwitcherToggle(GtkCheckButton* btn, int theme)
{
    AdwStyleManager* mgr = adw_style_manager_get_default();
    adw_style_manager_set_color_scheme(mgr, theme);
    g_settings_set_int(settings, "theme", theme);
}

// Show about window
void ActionAbout()
{
    adw_show_about_window(
        gtk_application_get_active_window(GTK_APPLICATION(app)),
        "application-name", "List",
        "application-icon", APP_ID,
        "version", VERSION,
        "copyright", "© 2023 Vlad Krupinsky",
        "website", "https://github.com/mrvladus/List",
        "license-type", GTK_LICENSE_MIT_X11,
        NULL);
}

// Exit application
void ActionQuit()
{
    g_application_quit(G_APPLICATION(app));
}

// ---------- Todo entry field ---------- //

GtkWidget* Entry()
{
    GtkWidget* entry = gtk_entry_new();
    g_object_set(G_OBJECT(entry),
        "secondary-icon-name", "list-add-symbolic",
        "margin-start", 50,
        "margin-end", 50,
        NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(EntryActivated), NULL);

    return entry;
}

void EntryActivated(GtkEntry* entry)
{
    // Get text from entry
    const char* text = gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
    // Check for empty string
    if (strlen(text) == 0)
        return;
    // Check if todo already exists
    g_auto(GStrv) todos = g_settings_get_strv(settings, "todos");
    for (int i = 0; todos[i]; i++) {
        if (g_strcmp0(text, todos[i]) == 0)
            return;
    }
    // Prepend todo to todos in gsettings
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_add(builder, text);
    g_strv_builder_addv(builder, (const char**)todos);
    g_auto(GStrv) new_todos = g_strv_builder_end(builder);
    g_settings_set_strv(settings, "todos", (const gchar* const*)new_todos);
    // Add todo row
    gtk_list_box_prepend(GTK_LIST_BOX(todos_list), Todo(text));
    UpdateTodoListVisibility();
    // Clear entry
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), "", -1);
}

// ---------- Todo list ---------- //

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
// ---------- Todo  ---------- //

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

// Get all todos array and create new one without deleted todo
void DeleteTodo(GtkButton* btn, gpointer todo)
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

// ---------- Main window ---------- //

void CreateWindow()
{
    // Load gsettings
    settings = g_settings_new(APP_ID);
    // Main window
    GtkWidget* win = adw_application_window_new(GTK_APPLICATION(app));
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
    app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(CreateWindow), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
