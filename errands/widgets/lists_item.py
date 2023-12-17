# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk, Gio
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.logging import Log
from errands.utils.sync import Sync


class ListItem(Gtk.ListBoxRow):
    def __init__(self, name, uid, task_list, list_box, lists, window) -> None:
        super().__init__()
        self.name = name
        self.uid = uid
        self.task_list = task_list
        self.window = window
        self.list_box = list_box
        self.lists = lists
        self._build_ui()
        self._add_actions()

    def _add_actions(self):
        group = Gio.SimpleActionGroup()
        self.insert_action_group(name="list_item", group=group)

        def _create_action(name: str, callback: callable, shortcuts=None) -> None:
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
                self.lists.stack.remove(self.task_list)
                # Switch row
                rows = get_children(self.list_box)
                row = self.list_box.get_selected_row()
                idx = rows.index(row)
                self.list_box.select_row(rows[idx - 1])
                self.list_box.remove(row)
                Sync.sync()

            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Are you sure?"),  # type:ignore
                body=_("List will be permanently deleted"),  # type:ignore
                default_response="delete",
                close_response="cancel",
            )
            dialog.add_response("cancel", _("Cancel"))  # type:ignore
            dialog.add_response("delete", _("Delete"))  # type:ignore
            dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE)
            dialog.connect("response", _confirm)
            dialog.present()

        def _rename(*args):
            def entry_changed(entry, _, dialog):
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
                Sync.sync()

            entry = Gtk.Entry(placeholder_text=_("New Name"))  # type:ignore
            dialog = Adw.MessageDialog(
                transient_for=self.window,
                hide_on_close=True,
                heading=_("Rename List"),  # type:ignore
                default_response="save",
                close_response="cancel",
                extra_child=entry,
            )
            dialog.add_response("cancel", _("Cancel"))  # type:ignore
            dialog.add_response("save", _("Save"))  # type:ignore
            dialog.set_response_enabled("save", False)
            dialog.set_response_appearance("save", Adw.ResponseAppearance.SUGGESTED)
            dialog.connect("response", _confirm, entry)
            entry.connect("notify::text", entry_changed, dialog)
            dialog.present()

        def _export():
            pass

        _create_action("delete", _delete)
        _create_action("rename", _rename)
        _create_action("export", _export)

    def _build_ui(self):
        # Box
        box = Gtk.Box(css_classes=["toolbar"])
        # Label
        label = Gtk.Label(
            label=self.name,
            halign="start",
            hexpand=True,
            ellipsize=3,
        )
        self.task_list.title.connect(
            "notify::title",
            lambda *_: label.set_label(self.task_list.title.get_title()),
        )
        box.append(label)
        # Menu
        menu: Gio.Menu = Gio.Menu.new()
        menu.append(_("Rename"), "list_item.rename")  # type:ignore
        menu.append(_("Delete"), "list_item.delete")  # type:ignore
        menu.append(_("Export"), "list_item.export")  # type:ignore
        box.append(
            Gtk.MenuButton(
                menu_model=menu,
                icon_name="view-more-symbolic",
                tooltip_text=_("Menu"),  # type:ignore
            )
        )
        self.set_child(box)
