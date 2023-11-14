# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.widgets.tasks_list import TasksList
from gi.repository import Adw, Gtk, Gio, GObject


class Lists(Adw.Bin):
    def __init__(self, window, stack: Gtk.Stack):
        super().__init__()
        self.stack: Gtk.Stack = stack
        self.window = window
        self.build_ui()
        self.load_lists()

    def build_ui(self):
        self.props.width_request = 260
        self.props.height_request = 200
        hb = Adw.HeaderBar(
            title_widget=Gtk.Label(label="Errands", css_classes=["heading"])
        )
        # Add list button
        self.add_btn = Gtk.ToggleButton(
            icon_name="list-add-symbolic",
            tooltip_text=_("Add List"),  # type:ignore
        )
        hb.pack_start(self.add_btn)
        # Main menu
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
            placeholder_text=_("Add new List"),  # type:ignore
            margin_start=6,
            margin_end=6,
            margin_top=5,
        )
        entry.connect("activate", self.on_list_added)
        entry_rev = Gtk.Revealer(child=entry)
        entry_rev.bind_property(
            "reveal-child",
            self.add_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        # Lists
        self.lists = Gtk.StackSidebar(stack=self.stack, vexpand=True)
        # Toolbar view
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(child=self.lists, propagate_natural_height=True)
        )
        toolbar_view.add_top_bar(hb)
        toolbar_view.add_top_bar(entry_rev)
        self.set_child(toolbar_view)

    def on_list_added(self, entry):
        text: str = entry.props.text
        if text.strip(" \n\t") == "":
            return
        uid = UserData.add_list(text)
        self.stack.add_titled(child=TasksList(self.window, uid), name=text, title=text)
        entry.props.text = ""

    def load_lists(self):
        for list in UserData.get_lists():
            self.stack.add_titled(
                child=TasksList(self.window, list[0]), name=list[1], title=list[1]
            )

    def delete_list(self):
        pass

    def rename_list(self):
        pass
