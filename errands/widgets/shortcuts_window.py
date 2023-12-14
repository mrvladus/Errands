# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk

xml = f"""
<interface>
<template class="ShortcutsWindow" parent="GtkShortcutsWindow">
<property name="hide-on-close">true</property>
<property name="modal">true</property>
<child>
<object class="GtkShortcutsSection">

<!-- General -->
<child>
<object class="GtkShortcutsGroup">
<property name="title" translatable="yes">{_("General")}</property>

<child>
<object class="GtkShortcutsShortcut">
<property name="title" translatable="yes">{_("Show keyboard shortcuts")}</property>
<property name="accelerator">&lt;Control&gt;question</property>
</object>
</child>

<child>
<object class="GtkShortcutsShortcut">
<property name="title" translatable="yes">{_("Toggle sidebar")}</property>
<property name="accelerator">F9</property>
</object>
</child>

<child>
<object class="GtkShortcutsShortcut">
<property name="title" translatable="yes">{_("Preferences")}</property>
<property name="accelerator">&lt;Control&gt;comma</property>
</object>
</child>

<child>
<object class="GtkShortcutsShortcut">
<property name="title" translatable="yes">{_("Quit")}</property>
<property name="accelerator">&lt;Control&gt;q &lt;Control&gt;w</property>
</object>
</child>

</object>
</child>
</object>
</child>
</template>
</interface>
"""


@Gtk.Template(string=xml)
class ShortcutsWindow(Gtk.ShortcutsWindow):
    __gtype_name__ = "ShortcutsWindow"

    def __init__(self, window: Adw.ApplicationWindow):
        super().__init__()
        self.set_transient_for(window)
        self.present()
