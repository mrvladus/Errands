# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.sync import Sync
from errands.widgets.lists_item import ListItem
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio, GObject


class Lists(Adw.Bin):
    def __init__(self, window, stack: Gtk.Stack):
        super().__init__()
        self.stack: Gtk.Stack = stack
        self.window = window
        self._build_ui()
        self._load_lists()

    def _build_ui(self):
        hb = Adw.HeaderBar(
            title_widget=Gtk.Label(label="Errands", css_classes=["heading"])
        )
        # Add list button
        self.add_btn = Gtk.Button(
            icon_name="list-add-symbolic",
            tooltip_text=_("Add List"),  # type:ignore
        )
        self.add_btn.connect("clicked", self.on_add_btn_clicked)
        hb.pack_start(self.add_btn)
        # Main menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Preferences"), "app.preferences")  # type:ignore
        menu.append(_("Keyboard Shortcuts"), "app.shortcuts")  # type:ignore
        menu.append(_("Sync / Fetch Tasks"), "app.sync")  # type:ignore
        menu.append(_("About Errands"), "app.about")  # type:ignore
        menu_btn = Gtk.MenuButton(
            menu_model=menu,
            primary=True,
            icon_name="open-menu-symbolic",
            tooltip_text=_("Main Menu"),  # type:ignore
        )
        hb.pack_end(menu_btn)
        # Lists
        self.lists = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self.lists.connect("row-selected", self.on_list_swiched)
        # Status page
        self.status_page = Adw.StatusPage(
            title=_("Add new List"),  # type:ignore
            description=_('Click "+" button'),  # type:ignore
            icon_name="view-list-bullet-rtl-symbolic",
            css_classes=["compact"],
            vexpand=True,
        )
        box = Gtk.Box(orientation="vertical")
        box.append(self.lists)
        box.append(self.status_page)
        # Trash button
        trash_btn = Gtk.Button(
            child=Adw.ButtonContent(
                icon_name="user-trash-symbolic",
                label=_("Trash"),  # type:ignore
                halign="center",
            ),
            css_classes=["flat"],
            margin_top=6,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
        )
        trash_btn.connect("clicked", self.on_trash_btn_clicked)
        # Toolbar view
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(child=box, propagate_natural_height=True)
        )
        toolbar_view.add_top_bar(hb)
        toolbar_view.add_bottom_bar(trash_btn)
        self.set_child(toolbar_view)

    def add_list(self, name, uid) -> Gtk.ListBoxRow:
        task_list = TaskList(self.window, uid, self)
        self.stack.add_titled(child=task_list, name=name, title=name)
        row = ListItem(name, uid, task_list, self.lists, self, self.window)
        self.lists.append(row)
        return row

    def on_add_btn_clicked(self, btn):
        def entry_changed(entry, _, dialog):
            text = entry.props.text.strip(" \n\t")
            names = [i["name"] for i in UserData.get_lists_as_dicts()]
            dialog.set_response_enabled("add", text and text not in names)

        def _confirm(_, res, entry):
            if res == "cancel":
                Log.debug("Adding new list is cancelled")
                return

            name = entry.props.text.rstrip().lstrip()
            uid = UserData.add_list(name)
            row = self.add_list(name, uid)
            row.activate()
            Sync.sync()

        entry = Gtk.Entry(placeholder_text=_("New List Name"))  # type:ignore
        dialog = Adw.MessageDialog(
            transient_for=self.window,
            hide_on_close=True,
            heading=_("Add List"),  # type:ignore
            default_response="add",
            close_response="cancel",
            extra_child=entry,
        )
        dialog.add_response("cancel", _("Cancel"))  # type:ignore
        dialog.add_response("add", _("Add"))  # type:ignore
        dialog.set_response_enabled("add", False)
        dialog.set_response_appearance("add", Adw.ResponseAppearance.SUGGESTED)
        dialog.connect("response", _confirm, entry)
        entry.connect("notify::text", entry_changed, dialog)
        dialog.present()

    def on_trash_btn_clicked(self, _btn):
        self.stack.set_visible_child_name("trash")
        self.window.split_view.set_show_content(True)

    def on_list_swiched(self, _, row: Gtk.ListBoxRow):
        if row:
            self.stack.set_visible_child_name(row.name)
            self.window.split_view.set_show_content(True)
            GSettings.set("last-open-list", "s", row.name)
            self.status_page.set_visible(False)

    def get_lists(self) -> list[TaskList]:
        lists: list[TaskList] = []
        pages: Adw.ViewStackPages = self.stack.get_pages()
        for i in range(pages.get_n_items()):
            child = pages.get_item(i).get_child()
            if isinstance(child, TaskList):
                lists.append(child)
        return lists

    def _load_lists(self):
        # Clear deleted tasks and lists
        if GSettings.get("sync-provider") == 0:
            UserData.run_sql(
                "DELETE FROM lists WHERE deleted = 1",
                "DELETE FROM tasks WHERE deleted = 1",
            )

        # Add lists
        lists = [i for i in UserData.get_lists_as_dicts() if not i["deleted"]]
        for list in lists:
            row = self.add_list(list["name"], list["uid"])
            if GSettings.get("last-open-list") == list["name"]:
                self.lists.select_row(row)
        self.status_page.set_visible(len(lists) == 0)

    def update_ui(self):
        Log.debug("Lists: Update UI")

        # Delete lists
        lists_uids = [i["uid"] for i in UserData.get_lists_as_dicts()]
        for list in self.get_lists():
            if list.list_uid not in lists_uids:
                self.stack.remove(list)
                rows = get_children(self.lists)
                row = self.lists.get_selected_row()
                idx = rows.index(row)
                self.lists.select_row(rows[idx - 1])
                self.lists.remove(row)

        # Update old lists
        for list in self.get_lists():
            list.update_ui()

        # Create new lists
        old_uids = [row.uid for row in get_children(self.lists)]
        new_lists = UserData.get_lists_as_dicts()
        for list in new_lists:
            if list["uid"] not in old_uids:
                row = self.add_list(list["name"], list["uid"])
                self.lists.select_row(row)
                self.stack.set_visible_child_name(list["name"])
                self.status_page.set_visible(False)
