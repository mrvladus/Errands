# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.sync import Sync
from errands.widgets.tasks_list import TasksList
from gi.repository import Adw, Gtk, Gio, GObject


class Lists(Adw.Bin):
    def __init__(self, window, stack: Gtk.Stack):
        super().__init__()
        self.stack: Gtk.Stack = stack
        self.window = window
        self._build_ui()
        self.load_lists()

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
        self.lists.connect("row-selected", self.switch_list)
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
        # Toolbar view
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(child=box, propagate_natural_height=True)
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def add_list(self, name, uid):
        row = Gtk.ListBoxRow(
            child=Gtk.Label(
                label=name,
                halign="start",
                margin_start=6,
                margin_end=6,
                hexpand=True,
            )
        )
        row.uid = uid
        row.name = name
        self.stack.add_titled(
            child=TasksList(self.window, uid, self), name=name, title=name
        )
        self.lists.append(row)
        self.lists.select_row(row)

    def on_add_btn_clicked(self, btn):
        def entry_changed(entry, _, dialog):
            text = entry.props.text.strip(" \n\t")
            names = [i["uid"] for i in UserData.get_lists_as_dicts()]
            dialog.set_response_enabled("add", not text == "" and text not in names)

        def _confirm(_, res, entry):
            if res == "cancel":
                Log.debug("Adding new list is cancelled")
                return

            name = entry.props.text.rstrip().lstrip()
            uid = UserData.add_list(name)
            row = Gtk.ListBoxRow(
                child=Gtk.Label(
                    label=name,
                    halign="start",
                    margin_start=6,
                    margin_end=6,
                    hexpand=True,
                )
            )
            row.uid = uid
            row.name = name
            self.stack.add_titled(
                child=TasksList(self.window, uid, self), name=name, title=name
            )
            self.lists.append(row)
            self.lists.select_row(row)
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

    def get_lists(self) -> list[TasksList]:
        lists: list[TasksList] = []
        pages: Adw.ViewStackPages = self.stack.get_pages()
        for i in range(pages.get_n_items()):
            child = pages.get_item(i).get_child()
            if isinstance(child, TasksList):
                lists.append(child)
        return lists

    def load_lists(self):
        for list in UserData.get_lists_as_dicts():
            row = Gtk.ListBoxRow(
                child=Gtk.Label(
                    label=list["name"],
                    halign="start",
                    margin_start=6,
                    margin_end=6,
                    hexpand=True,
                )
            )
            row.uid = list["uid"]
            row.name = list["name"]
            self.lists.append(row)
            self.stack.add_titled(
                child=TasksList(self.window, list["uid"], self),
                name=list["name"],
                title=list["name"],
            )
            if GSettings.get("last-open-list") == row.name:
                self.lists.select_row(row)

    def switch_list(self, _, row):
        if row:
            self.stack.set_visible_child_name(row.name)
            self.window.split_view.set_show_content(True)
            GSettings.set("last-open-list", "s", row.name)
            self.status_page.set_visible(False)
        else:
            self.stack.set_visible_child_name("status")
            self.status_page.set_visible(True)

    def delete_list(self, widget: Gtk.Widget):
        Log.info(f"Delete list {widget.list_uid}")
        UserData.run_sql(
            f"UPDATE lists SET deleted = 1 WHERE uid = '{widget.list_uid}'",
            f"DELETE FROM tasks WHERE list_uid = '{widget.list_uid}'",
        )
        self.stack.remove(widget)
        # Switch row
        rows = get_children(self.lists)
        row = self.lists.get_selected_row()
        idx = rows.index(row)
        self.lists.select_row(rows[idx - 1])
        self.lists.remove(row)
        Sync.sync()

    def rename_list(self, widget, name):
        Log.info(f"Rename list {widget.list_uid}")
        UserData.run_sql(
            f"UPDATE lists SET name = '{name}' WHERE uid = '{widget.list_uid}'",
        )
        page: Gtk.StackPage = self.stack.get_page(widget)
        page.set_name(name)
        page.set_title(name)
        row = self.lists.get_selected_row()
        row.get_child().set_label(name)
        row.name = name

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
                row = Gtk.ListBoxRow(
                    child=Gtk.Label(
                        label=list["name"],
                        halign="start",
                        margin_start=6,
                        margin_end=6,
                        hexpand=True,
                    )
                )
                row.uid = list["uid"]
                row.name = list["name"]
                self.stack.add_titled(
                    child=TasksList(self.window, list["uid"], self),
                    name=list["name"],
                    title=list["name"],
                )
                self.lists.append(row)
                self.lists.select_row(row)
