# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from uuid import uuid4
from icalendar import Calendar
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.sync import Sync
from errands.widgets.components.box import Box
from errands.widgets.lists_item import ListItem
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio


class Lists(Adw.Bin):
    def __init__(self, window, stack: Gtk.Stack):
        super().__init__()
        self.stack: Gtk.Stack = stack
        self.window = window
        self._build_ui()
        self._add_actions()
        self._load_lists()

    def _add_actions(self):
        group = Gio.SimpleActionGroup()
        self.insert_action_group(name="lists", group=group)

        def _create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def _add(*args):
            pass

        def _import(*args):
            def _confirm(dialog: Gtk.FileDialog, res):
                try:
                    file = dialog.open_finish(res)
                except:
                    Log.debug("Lists: Import cancelled")
                    return
                with open(file.get_path(), "r") as f:
                    calendar = Calendar.from_ical(f.read())
                    # List name
                    name = calendar.get(
                        "X-WR-CALNAME", file.get_basename().rstrip(".ics")
                    )
                    if name in [
                        i[0]
                        for i in UserData.run_sql("SELECT name FROM lists", fetch=True)
                    ]:
                        name = f"{name}_{uuid4()}"
                    # Create list
                    uid = UserData.add_list(name)
                    # Add tasks
                    for todo in calendar.walk("VTODO"):
                        # Tags
                        if (tags := todo.get("CATEGORIES", "")) != "":
                            tags = ",".join(
                                [
                                    i.to_ical().decode("utf-8")
                                    for i in (
                                        tags if isinstance(tags, list) else tags.cats
                                    )
                                ]
                            )
                        # Start
                        if (start := todo.get("DTSTART", "")) != "":
                            start = (
                                todo.get("DTSTART", "")
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            start = ""
                        # End
                        if (end := todo.get("DUE", todo.get("DTEND", ""))) != "":
                            end = (
                                todo.get("DUE", todo.get("DTEND", ""))
                                .to_ical()
                                .decode("utf-8")
                                .strip("Z")
                            )
                        else:
                            end = ""
                        UserData.add_task(
                            color=todo.get("X-ERRANDS-COLOR", ""),
                            completed=str(todo.get("STATUS", "")) == "COMPLETED",
                            end_date=end,
                            list_uid=uid,
                            notes=str(todo.get("DESCRIPTION", "")),
                            parent=str(todo.get("RELATED-TO", "")),
                            percent_complete=int(todo.get("PERCENT-COMPLETE", 0)),
                            priority=int(todo.get("PRIORITY", 0)),
                            start_date=start,
                            tags=tags,
                            text=str(todo.get("SUMMARY", "")),
                            uid=todo.get("UID", None),
                        )
                self.update_ui()
                self.window.add_toast(_("Imported"))  # type:ignore
                Sync.sync()

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(default_filter=filter)
            dialog.open(self.window, None, _confirm)

        _create_action("add", _add)
        _create_action("import", _import)

    def _build_ui(self):
        hb = Adw.HeaderBar(
            title_widget=Gtk.Label(label="Errands", css_classes=["heading"])
        )
        # Import menu
        import_menu: Gio.Menu = Gio.Menu.new()
        import_menu.append(_("Import List"), "lists.import")  # type:ignore
        # Add list button
        self.add_list_btn = Adw.SplitButton(
            icon_name="list-add-symbolic",
            tooltip_text=_("Add List"),  # type:ignore
            menu_model=import_menu,
        )
        self.add_list_btn.connect("clicked", self.on_add_btn_clicked)
        hb.pack_start(self.add_list_btn)
        # Main menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Sync / Fetch Tasks"), "app.sync")  # type:ignore
        menu.append(_("Preferences"), "app.preferences")  # type:ignore
        menu.append(_("Keyboard Shortcuts"), "win.show-help-overlay")  # type:ignore
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
        # Trash button
        self.trash_btn = Gtk.Button(
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
        self.trash_btn.connect("clicked", self.on_trash_btn_clicked)
        # Toolbar view
        toolbar_view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(
                child=Box(
                    children=[self.lists, self.status_page], orientation="vertical"
                ),
                propagate_natural_height=True,
            )
        )
        toolbar_view.add_top_bar(hb)
        toolbar_view.add_bottom_bar(self.trash_btn)
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
        self.lists.unselect_all()
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
        # Add lists
        lists = [i for i in UserData.get_lists_as_dicts() if not i["deleted"]]
        for list in lists:
            row = self.add_list(list["name"], list["uid"])
            if GSettings.get("last-open-list") == list["name"]:
                self.lists.select_row(row)
        self.status_page.set_visible(len(lists) == 0)

    def update_ui(self):
        Log.debug("Lists: Update UI...")

        # Delete lists
        uids = [i["uid"] for i in UserData.get_lists_as_dicts()]
        for row in get_children(self.lists):
            if row.uid not in uids:
                prev_child = row.get_prev_sibling()
                next_child = row.get_next_sibling()
                list = row.task_list
                self.stack.remove(list)
                if prev_child or next_child:
                    self.lists.select_row(prev_child or next_child)
                self.lists.remove(row)

        # Update old lists
        for list in self.get_lists():
            list.update_ui()

        # Create new lists
        old_uids = [row.uid for row in get_children(self.lists)]
        new_lists = UserData.get_lists_as_dicts()
        for list in new_lists:
            if list["uid"] not in old_uids:
                Log.debug(f"Lists: Add list '{list['uid']}'")
                row = self.add_list(list["name"], list["uid"])
                self.lists.select_row(row)
                self.stack.set_visible_child_name(list["name"])
                self.status_page.set_visible(False)

        # Show status
        lists = get_children(self.lists)
        self.status_page.set_visible(len(lists) == 0)
        if len(lists) == 0:
            self.stack.set_visible_child_name("status")
