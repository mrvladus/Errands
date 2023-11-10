# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk, Gio


class Lists(Adw.Bin):
    def __init__(self):
        super().__init__()
        self.build_ui()

    def build_ui(self):
        self.props.width_request = 300
        self.props.height_request = 200
        hb = Adw.HeaderBar(
            title_widget=Gtk.Label(label="Errands", css_classes=["heading"])
        )
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Preferences"), "app.preferences")  # type:ignore
        menu.append(_("Keyboard Shortcuts"), "app.shortcuts")  # type:ignore
        menu.append(_("About Errands"), "app.about")  # type:ignore
        menu_btn = Gtk.MenuButton(
            menu_model=menu,
            primary=True,
            icon_name="open-menu-symbolic",
            tooltip_text=_("Main Menu"),  # type:ignore
        )
        hb.pack_end(menu_btn)
        # Entry
        entry = Gtk.Entry(
            margin_start=12,
            margin_end=12,
            placeholder_text=_("Add new List"),  # type:ignore
        )
        # Lists
        self.lists = Gtk.ListBox(selection_mode=0)
        self.lists.add_css_class("navigation-sidebar")
        scrl = Gtk.ScrolledWindow(child=self.lists)
        # Box
        box = Gtk.Box(orientation="vertical", spacing=12)
        box.append(entry)
        box.append(scrl)
        # Toolbar view
        toolbar_view = Adw.ToolbarView(
            content=box, width_request=360, height_request=200
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def add_list(self):
        pass

    def load_lists(self):
        pass

    def delete_list(self):
        pass

    def rename_list(self):
        pass
