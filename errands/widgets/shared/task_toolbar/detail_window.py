# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING

from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.lib.sync.sync import Sync
from errands.state import State
from errands.widgets.shared.color_selector import ErrandsColorSelector
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsCheckButton
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView

if TYPE_CHECKING:
    from errands.widgets.task import Task
    from errands.widgets.today.today_task import TodayTask


class DetailWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
        self.task = None
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Details"))

        self.color_selector: ErrandsColorSelector = ErrandsColorSelector(
            on_color_selected=self.__on_accent_color_selected
        )
        self.created_label = Gtk.Label(
            label=_("Created:"),
            halign=Gtk.Align.START,
            margin_bottom=6,
            margin_end=6,
            margin_start=6,
            css_classes=["caption-heading"],
        )
        self.changed_label = Gtk.Label(
            label=_("Changed:"),
            halign=Gtk.Align.START,
            margin_end=6,
            margin_start=6,
            css_classes=["caption-heading"],
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar()],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    margin_bottom=6,
                    margin_top=6,
                    margin_end=6,
                    margin_start=6,
                    spacing=3,
                    children=[
                        self.color_selector,
                        self.created_label,
                        self.changed_label,
                    ],
                ),
            )
        )

    def __on_accent_color_selected(
        self, _color_selector, btn: ErrandsCheckButton, color: str
    ) -> None:
        if not btn.get_active() or self.task.block_signals:
            return
        Log.debug(f"Task: change color to '{color}'")
        if color != self.task.task_data.color:
            self.task.update_props(
                ["color", "synced"], [color if color != "none" else "", False]
            )
            self.task.update_color()
            Sync.sync()

    # ------ PUBLIC METHODS ------ #

    def show(self, task: Task | TodayTask):
        self.task = task

        # Update dates
        created_date: str = datetime.fromisoformat(
            self.task.task_data.created_at
        ).strftime("%Y.%m.%d %H:%M:%S")
        changed_date: str = datetime.fromisoformat(
            self.task.task_data.changed_at
        ).strftime("%Y.%m.%d %H:%M:%S")
        self.created_label.set_label(_("Created:") + " " + created_date)
        self.changed_label.set_label(_("Changed:") + " " + changed_date)

        # Update color
        self.task.block_signals = True
        self.color_selector.select_color(self.task.task_data.color)
        self.task.block_signals = False

        self.present(State.main_window)
