# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from typing import TYPE_CHECKING

from gi.repository import Gtk  # type:ignore

from errands.lib.gsettings import GSettings

if TYPE_CHECKING:
    from errands.widgets.task.task import Task


class TaskProgressBar(Gtk.Revealer):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self):
        self.progress_bar: Gtk.ProgressBar = Gtk.ProgressBar(
            margin_start=12,
            margin_end=12,
            margin_bottom=2,
            css_classes=["osd", "dim-label"],
        )
        self.set_child(self.progress_bar)

    # ------ PUBLIC METHODS ------ #

    def update_ui(self) -> None:
        if GSettings.get("task-show-progressbar"):
            total, completed = self.task.get_status()
            pc: int = (
                completed / total * 100
                if total > 0
                else (100 if self.task.title.complete_btn.get_active() else 0)
            )
            if self.task.task_data.percent_complete != pc:
                self.task.update_props(["percent_complete", "synced"], [pc, False])

            self.progress_bar.set_fraction(pc / 100)
            self.set_reveal_child(total > 0)
