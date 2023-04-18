#include "utils.h"
#include "global.h"

// Convert to a new gsettings todos-v2 format from versions 44.1.x to 44.2.x
void ConvertSettings()
{
    // Get old todos and transfer them to a new key "todos-v2"
    g_auto(GStrv) old_todos = g_settings_get_strv(settings, "todos");
    if (g_strv_length(old_todos) == 0)
        return;
    GVariantBuilder g_var_builder;
    g_variant_builder_init(&g_var_builder, G_VARIANT_TYPE_ARRAY);
    for (int i = 0; old_todos[i]; i++) {
        g_autoptr(GStrvBuilder) sub_builder = g_strv_builder_new();
        g_strv_builder_add(sub_builder, old_todos[i]);
        g_auto(GStrv) array = g_strv_builder_end(sub_builder);
        GVariant* new_todo = g_variant_new_strv((const char* const*)array, -1);
        g_variant_builder_add_value(&g_var_builder, new_todo);
    }
    GVariant* new_todos = g_variant_builder_end(&g_var_builder);
    g_settings_set_value(settings, "todos-v2", new_todos);
    g_settings_set(settings, "todos", "as", (const gchar**) { NULL });
}
