# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os
from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk, GtkSource  # type:ignore

from errands.lib.logging import Log

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class NotesWindow(Adw.Dialog):
    __gtype_name__ = "NotesWindow"

    buffer: GtkSource.Buffer = Gtk.Template.Child()

    def __init__(self, task: Task):
        super().__init__()
        self.task = task
        Adw.StyleManager.get_default().bind_property(
            "dark",
            self.buffer,
            "style-scheme",
            GObject.BindingFlags.SYNC_CREATE,
            lambda _, is_dark: self.buffer.set_style_scheme(
                GtkSource.StyleSchemeManager.get_default().get_scheme(
                    "Adwaita-dark" if is_dark else "Adwaita"
                )
            ),
        )
        self.buffer.set_language(
            GtkSource.LanguageManager.get_default().get_language("markdown")
        )

    def show(self):
        self.buffer.props.text = self.task.get_prop("notes")
        self.present(Adw.Application.get_default().get_active_window())

    def do_closed(self):
        text: str = self.buffer.props.text
        if text == self.task.get_prop("notes"):
            return
        Log.info("Task: Change notes")
        self.task.update_props(["notes", "synced"], [self.buffer.props.text, False])
        self.task.update_task_data()
        self.task.update_toolbar()
