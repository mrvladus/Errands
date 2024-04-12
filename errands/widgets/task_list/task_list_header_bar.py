# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.animation import scroll
from errands.lib.data import UserData
from errands.lib.logging import Log

if TYPE_CHECKING:
    from errands.widgets.task_list import TaskList


class TaskListHeaderBar(Adw.Bin):
    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list: TaskList = task_list
        self.__build_ui()
        self.update_ui()

    def __repr__(self) -> str:
        return f"<class 'TaskListHeaderBar' {self.task_list.list_uid}>"

    def __build_ui(self):
        # Title
        self.title = Adw.WindowTitle()

        # Toggle completed btn
        self.toggle_completed_btn: Gtk.ToggleButton = Gtk.ToggleButton(
            icon_name="errands-check-toggle-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Toggle Completed Tasks"),
        )
        self.toggle_completed_btn.connect(
            "toggled", self._on_toggle_completed_btn_toggled
        )

        # Delete completed btn
        self.delete_completed_btn: Gtk.Button = Gtk.Button(
            icon_name="errands-delete-all-symbolic",
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Delete Completed Tasks"),
        )
        self.delete_completed_btn.bind_property(
            "visible",
            self.toggle_completed_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE,
        )
        self.delete_completed_btn.connect(
            "clicked", self._on_toggle_completed_btn_toggled
        )

        # Scroll up btn
        self.scroll_up_btn: Gtk.Button = Gtk.Button(
            icon_name="errands-up-symbolic",
            visible=False,
            valign=Gtk.Align.CENTER,
            tooltip_text=_("Scroll Up"),
        )
        self.scroll_up_btn.connect("clicked", self._on_scroll_up_btn_clicked)

        hb: Adw.HeaderBar = Adw.HeaderBar(title_widget=self.title)
        hb.pack_start(self.toggle_completed_btn)
        hb.pack_start(self.delete_completed_btn)

        self.set_child(hb)

    def update_ui(self) -> None:
        # Update title
        self.title.set_title(UserData.get_list_prop(self.task_list.list_uid, "name"))

        n_total, n_completed = UserData.get_status(self.task_list.list_uid)

        # Update headerbar subtitle
        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}" if n_total > 0 else ""
        )

        # Update sidebar item counter
        total = str(n_total) if n_total > 0 else ""
        completed = str(n_completed) if n_total > 0 else ""
        counter = completed + " / " + total if n_total > 0 else ""
        self.task_list.sidebar_row.size_counter.set_label(counter)

        # Update delete completed button
        self.delete_completed_btn.set_sensitive(n_completed > 0)

    def _on_delete_completed_btn_clicked(self, btn: Gtk.Button) -> None:
        """Hide completed tasks and move them to trash"""

        Log.info(f"Task List '{self.task_list.list_uid}': Delete completed tasks")
        for task in self.task_list.all_tasks:
            if not task.task_data.trash and task.task_data.completed:
                task.delete()
        self.update_ui()

    def _on_toggle_completed_btn_toggled(self, btn: Gtk.ToggleButton) -> None:
        self.task_list.content.completed_task_list.set_visible(btn.get_active())
        UserData.update_list_prop(
            self.task_list.list_uid, "show_completed", btn.get_active()
        )

    def _on_scroll_up_btn_clicked(self, btn: Gtk.ToggleButton) -> None:
        scroll(self.task_list.content, False)
