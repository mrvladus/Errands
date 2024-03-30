# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gtk, Adw, GtkSource, GObject  # type:ignore

from errands.lib.logging import Log

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class NotesWindow(Adw.Dialog):
    def __init__(self, task: Task):
        super().__init__()
        self.task = task
        self.set_title(_("Notes"))
        self.set_content_width(600)
        self.set_content_height(600)
        self.buffer = GtkSource.Buffer()
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
        lm: GtkSource.LanguageManager = GtkSource.LanguageManager.get_default()
        self.buffer.set_language(lm.get_language("markdown"))
        self.text_view = GtkSource.View(
            wrap_mode=3,
            buffer=self.buffer,
            top_margin=6,
            bottom_margin=6,
            right_margin=6,
            left_margin=6,
            show_line_numbers=True,
        )
        view = Adw.ToolbarView(
            content=Gtk.ScrolledWindow(
                child=self.text_view,
                propagate_natural_height=True,
                propagate_natural_width=True,
            ),
            top_bar_style=1,
        )
        view.add_top_bar(Adw.HeaderBar())
        self.set_child(view)

    def show(self):
        self.buffer.props.text = self.task.get_prop("notes")
        self.present(self.task.window)

    def do_closed(self):
        text: str = self.buffer.props.text
        if text == self.task.get_prop("notes"):
            return
        Log.info("Task: Change notes")
        self.task.update_props(["notes", "synced"], [self.buffer.props.text, False])
        self.task.update_toolbar()
