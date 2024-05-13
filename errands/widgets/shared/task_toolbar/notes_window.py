# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Adw, GObject, Gtk, GtkSource  # type:ignore

from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView

if TYPE_CHECKING:
    from errands.widgets.task import Task


class NotesWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
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
        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar()],
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
        )

    # ------ PUBLIC METHODS ------ #

    def show(self, task: Task):
        self.task = task
        self.buffer.props.text = self.task.task_data.notes
        self.present(State.main_window)

    # ------ SIGNAL HANDLERS ------ #

    def do_closed(self):
        text: str = self.buffer.props.text
        if text == self.task.task_data.notes:
            return
        Log.info("Task: Change notes")
        self.task.update_props(["notes", "synced"], [self.buffer.props.text, False])
        self.task.update_toolbar()
        Sync.sync()
