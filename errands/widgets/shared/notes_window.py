# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk, GtkSource  # type:ignore

from errands.lib.logging import Log
from errands.state import State

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class NotesWindow(Adw.Dialog):
    def __init__(self, task: Task):
        super().__init__()
        self.task: Task = task
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_content_width(600)
        self.set_content_height(600)
        self.set_title(_("Notes"))

        # Buffer
        self.buffer: GtkSource.Buffer = GtkSource.Buffer()
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

        # Toolbar View
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(
            top_bar_style=Adw.ToolbarStyle.RAISED,
            content=Gtk.ScrolledWindow(
                propagate_natural_height=True,
                propagate_natural_width=True,
                child=GtkSource.View(
                    wrap_mode=3,
                    top_margin=6,
                    bottom_margin=6,
                    left_margin=6,
                    right_margin=6,
                    show_line_numbers=True,
                    buffer=self.buffer,
                ),
            ),
        )
        toolbar_view.add_top_bar(Adw.HeaderBar())
        self.set_child(toolbar_view)

    # ------ PUBLIC METHODS ------ #

    def show(self):
        self.buffer.props.text = self.task.task_data.notes
        self.present(State.main_window)

    # ------ SIGNAL HANDLERS ------ #

    def do_closed(self):
        text: str = self.buffer.props.text.strip()
        if text == self.task.task_data.notes:
            return
        Log.info("Task: Change notes")
        self.task.update_props(["notes", "synced"], [self.buffer.props.text, False])
        self.task.toolbar.update_ui()
