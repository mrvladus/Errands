# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
from uuid import uuid4
from icalendar import Calendar, Todo
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.sync import Sync
from errands.widgets.components import Box
from errands.widgets.task_list import TaskList
from gi.repository import Adw, Gtk, Gio, GObject


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

        def _backup_create(*args):
            pass

        def _backup_load(*args):
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
                self.window.add_toast(_("Imported"))
                Sync.sync()

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(default_filter=filter)
            dialog.open(self.window, None, _confirm)

        _create_action("add", _add)
        _create_action("backup_create", _backup_create)
        _create_action("backup_load", _backup_load)
        _create_action("import", _import)

    def _build_ui(self):
        hb = Adw.HeaderBar(
            title_widget=Gtk.Label(
                label=_("Errands"),
                css_classes=["heading"],
            )
        )
        # Import menu
        import_menu: Gio.Menu = Gio.Menu.new()
        import_menu.append(_("Import List"), "lists.import")
        # Add list button
        self.add_list_btn = Adw.SplitButton(
            icon_name="list-add-symbolic",
            tooltip_text=_("Add List"),
            menu_model=import_menu,
            dropdown_tooltip=_("More Options"),
        )
        self.add_list_btn.connect("clicked", self.on_add_btn_clicked)
        hb.pack_start(self.add_list_btn)
        # Main menu
        menu: Gio.Menu = Gio.Menu.new()
        top_section = Gio.Menu.new()
        top_section.append(_("Sync / Fetch Tasks"), "app.sync")
        backup_submenu = Gio.Menu.new()
        backup_submenu.append(_("Create"), "lists.backup_create")
        backup_submenu.append(_("Load"), "lists.backup_load")
        # top_section.append_submenu(_("Backup"), backup_submenu)
        menu.append_section(None, top_section)
        bottom_section = Gio.Menu.new()
        bottom_section.append(_("Preferences"), "app.preferences")
        bottom_section.append(_("Keyboard Shortcuts"), "win.show-help-overlay")
        bottom_section.append(_("About Errands"), "app.about")
        menu.append_section(None, bottom_section)
        menu_btn = Gtk.MenuButton(
            menu_model=menu,
            primary=True,
            icon_name="open-menu-symbolic",
            tooltip_text=_("Main Menu"),
        )
        hb.pack_end(menu_btn)
        # Lists
        self.lists = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self.lists.connect("row-selected", self.on_list_swiched)
        # Status page
        self.status_page = Adw.StatusPage(
            title=_("Add new List"),
            description=_('Click "+" button'),
            icon_name="view-list-bullet-rtl-symbolic",
            css_classes=["compact"],
            vexpand=True,
        )
        # Trash button
        self.trash_btn = Gtk.Button(
            child=Adw.ButtonContent(
                icon_name="errands-trash-symbolic",
                label=_("Trash"),
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
        page: Adw.ViewStackPage = self.stack.add_titled(
            child=task_list, name=name, title=name
        )
        task_list.title.bind_property(
            "title", page, "title", GObject.BindingFlags.SYNC_CREATE
        )
        task_list.title.bind_property(
            "title", page, "name", GObject.BindingFlags.SYNC_CREATE
        )
        row = ListItem(task_list, self.lists, self, self.window)
        self.lists.append(row)
        return row

    def on_add_btn_clicked(self, btn):
        def _entry_activated(_, dialog):
            if dialog.get_response_enabled("add"):
                dialog.response("add")
                dialog.close()

        def _entry_changed(entry, _, dialog):
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

        entry = Gtk.Entry(placeholder_text=_("New List Name"))
        dialog = Adw.MessageDialog(
            transient_for=self.window,
            hide_on_close=True,
            heading=_("Add List"),
            default_response="add",
            close_response="cancel",
            extra_child=entry,
        )
        dialog.add_response("cancel", _("Cancel"))
        dialog.add_response("add", _("Add"))
        dialog.set_response_enabled("add", False)
        dialog.set_response_appearance("add", Adw.ResponseAppearance.SUGGESTED)
        dialog.connect("response", _confirm, entry)
        entry.connect("activate", _entry_activated, dialog)
        entry.connect("notify::text", _entry_changed, dialog)
        dialog.present()

    def on_trash_btn_clicked(self, _btn):
        self.lists.unselect_all()
        self.stack.set_visible_child_name("trash")
        self.window.split_view.set_show_content(True)
        self.window.split_view_inner.set_show_sidebar(False)

    def on_list_swiched(self, _, row: Gtk.ListBoxRow):
        if row:
            name = row.label.get_label()
            self.stack.set_visible_child_name(name)
            self.window.split_view.set_show_content(True)
            GSettings.set("last-open-list", "s", name)
            self.status_page.set_visible(False)
        self.window.details.status.set_visible(True)

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

        # Update details
        tasks = []
        for list in self.get_lists():
            tasks.extend(list.get_all_tasks())
        if (
            self.window.details.parent in tasks
            and not self.window.details.parent.get_prop("trash")
        ):
            self.window.details.update_info(self.window.details.parent)
        else:
            self.window.details.status.set_visible(True)


class ListItem(Gtk.ListBoxRow):
    def __init__(self, task_list, list_box, lists, window) -> None:
        super().__init__()
        self.task_list = task_list
        self.uid = task_list.list_uid
        self.window = window
        self.list_box = list_box
        self.lists = lists
        self._build_ui()
        self._add_actions()

    def _add_actions(self):
        group = Gio.SimpleActionGroup()
        self.insert_action_group(name="list_item", group=group)

        def _create_action(name: str, callback: callable) -> None:
            action: Gio.SimpleAction = Gio.SimpleAction.new(name, None)
            action.connect("activate", callback)
            group.add_action(action)

        def _delete(*args):
            def _confirm(_, res):
                if res == "cancel":
                    Log.debug("ListItem: Deleting list is cancelled")
                    return

                Log.info(f"Lists: Delete list '{self.uid}'")
                UserData.run_sql(
                    f"UPDATE lists SET deleted = 1 WHERE uid = '{self.uid}'",
                    f"DELETE FROM tasks WHERE list_uid = '{self.uid}'",
                )
                self.window.stack.remove(self.task_list)
                # Switch row
                next_row = self.get_next_sibling()
                prev_row = self.get_prev_sibling()
                self.list_box.remove(self)
                if next_row or prev_row:
                    self.list_box.select_row(next_row or prev_row)
                else:
                    self.window.stack.set_visible_child_name("status")
                    self.lists.status_page.set_visible(True)

                # Remove items from trash
                for item in get_children(self.window.trash.trash_list):
                    if item.task_widget.list_uid == self.uid:
                        self.window.trash.trash_list.remove(item)

                Sync.sync()

            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Are you sure?"),
                body=_("List will be permanently deleted"),
                default_response="delete",
                close_response="cancel",
            )
            dialog.add_response("cancel", _("Cancel"))
            dialog.add_response("delete", _("Delete"))
            dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE)
            dialog.connect("response", _confirm)
            dialog.present()

        def _rename(*args):
            def _entry_activated(_, dialog):
                if dialog.get_response_enabled("save"):
                    dialog.response("save")
                    dialog.close()

            def _entry_changed(entry, _, dialog):
                text = entry.props.text.strip(" \n\t")
                names = [i["name"] for i in UserData.get_lists_as_dicts()]
                dialog.set_response_enabled("save", text and text not in names)

            def _confirm(_, res, entry):
                if res == "cancel":
                    Log.debug("ListItem: Editing list name is cancelled")
                    return
                Log.info(f"ListItem: Rename list {self.uid}")

                text = entry.props.text.rstrip().lstrip()
                UserData.run_sql(
                    f"""UPDATE lists SET name = '{text}', synced = 0
                    WHERE uid = '{self.uid}'"""
                )
                self.task_list.title.set_title(text)
                page: Adw.ViewStackPage = self.window.stack.get_page(self.task_list)
                page.set_name(text)
                page.set_title(text)
                Sync.sync()

            entry = Gtk.Entry(placeholder_text=_("New Name"))
            entry.get_buffer().props.text = self.label.get_label()
            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Rename List"),
                default_response="save",
                close_response="cancel",
                extra_child=entry,
            )
            dialog.add_response("cancel", _("Cancel"))
            dialog.add_response("save", _("Save"))
            dialog.set_response_enabled("save", False)
            dialog.set_response_appearance("save", Adw.ResponseAppearance.SUGGESTED)
            dialog.connect("response", _confirm, entry)
            entry.connect("activate", _entry_activated, dialog)
            entry.connect("notify::text", _entry_changed, dialog)
            dialog.present()

        def _export(*args):
            def _confirm(dialog, res):
                try:
                    file = dialog.save_finish(res)
                except:
                    Log.debug("List: Export cancelled")
                    return

                Log.info(f"List: Export '{self.uid}'")

                tasks = UserData.get_tasks_as_dicts(self.uid)
                calendar = Calendar()
                calendar.add("x-wr-calname", self.label.get_label())
                for task in tasks:
                    event = Todo()
                    event.add("uid", task["uid"])
                    event.add("related-to", task["parent"])
                    event.add("summary", task["text"])
                    if task["notes"]:
                        event.add("description", task["notes"])
                    event.add("priority", task["priority"])
                    if task["tags"]:
                        event.add("categories", task["tags"])
                    event.add("percent-complete", task["percent_complete"])
                    if task["color"]:
                        event.add("x-errands-color", task["color"])
                    event.add(
                        "dtstart",
                        datetime.fromisoformat(task["start_date"])
                        if task["start_date"]
                        else datetime.now(),
                    )
                    if task["end_date"]:
                        event.add(
                            "due",
                            datetime.fromisoformat(task["end_date"])
                            if task["end_date"]
                            else datetime.now(),
                        )
                    calendar.add_component(event)

                try:
                    with open(file.get_path(), "wb") as f:
                        f.write(calendar.to_ical())
                except Exception as e:
                    Log.error(f"List: Export failed. {e}")
                    self.window.add_toast(_("Export failed"))

                self.window.add_toast(_("Exported"))

            filter = Gtk.FileFilter()
            filter.add_pattern("*.ics")
            dialog = Gtk.FileDialog(
                initial_name=f"{self.uid}.ics", default_filter=filter
            )
            dialog.save(self.window, None, _confirm)

        _create_action("delete", _delete)
        _create_action("rename", _rename)
        _create_action("export", _export)

    def _build_ui(self):
        # Label
        self.label = Gtk.Label(
            halign="start",
            hexpand=True,
            ellipsize=3,
        )
        self.task_list.title.bind_property(
            "title",
            self.label,
            "label",
            GObject.BindingFlags.SYNC_CREATE,
        )
        # Menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Rename"), "list_item.rename")
        menu.append(_("Delete"), "list_item.delete")
        menu.append(_("Export"), "list_item.export")
        # Click controller
        ctrl = Gtk.GestureClick()
        ctrl.connect("released", self._on_click)
        self.add_controller(ctrl)
        self.set_child(
            Box(
                children=[
                    self.label,
                    Gtk.MenuButton(
                        menu_model=menu,
                        icon_name="view-more-symbolic",
                        tooltip_text=_("Menu"),
                    ),
                ],
                css_classes=["toolbar"],
            )
        )

    def _on_click(self, *args):
        self.window.stack.set_visible_child_name(self.label.get_label())
        self.window.split_view.set_show_content(True)
