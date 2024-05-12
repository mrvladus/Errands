# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT
from __future__ import annotations

import os
from typing import TYPE_CHECKING

from gi.repository import Adw, Gio, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton
from errands.widgets.shared.components.header_bar import ErrandsHeaderBar
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView

if TYPE_CHECKING:
    from errands.widgets.task import Task


class ErrandsAttachmentsWindow(Adw.Dialog):
    def __init__(self):
        super().__init__()
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_follows_content_size(True)
        self.set_title(_("Attachments"))

        self.status_page: Adw.StatusPage = Adw.StatusPage(
            title=_("No Files Attached"),
            icon_name="errands-attachment-symbolic",
            css_classes=["compact"],
            vexpand=True,
        )

        self.attachments_list: Gtk.ListBox = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE,
            css_classes=["boxed-list"],
            margin_bottom=12,
            margin_top=6,
            margin_end=12,
            margin_start=12,
            valign=Gtk.Align.START,
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[
                    ErrandsHeaderBar(
                        start_children=[
                            ErrandsButton(
                                icon_name="errands-add-symbolic",
                                tooltip_text=_("Add Attachment"),
                                on_click=self.__on_attachment_btn_clicked,
                            )
                        ]
                    )
                ],
                content=Gtk.ScrolledWindow(
                    child=ErrandsBox(
                        orientation=Gtk.Orientation.VERTICAL,
                        children=[self.status_page, self.attachments_list],
                    ),
                    propagate_natural_height=True,
                ),
                width_request=360,
            )
        )

    def show(self, task: Task):
        self.task = task
        self.attachments_list.remove_all()
        for path in self.task.task_data.attachments:
            self.attachments_list.append(ErrandsAttachment(path))
        self.update_ui()
        self.present(State.main_window)

    def update_ui(self):
        size: int = len(self.task.task_data.attachments)
        self.status_page.set_visible(size == 0)
        self.attachments_list.set_visible(size > 0)

    def __on_attachment_btn_clicked(self, _btn: ErrandsButton):
        def __confirm(dialog: Gtk.FileDialog, res) -> None:
            try:
                file: Gio.File = dialog.open_finish(res)
            except Exception as e:
                Log.debug(f"Attachments: Selecting file cancelled. {e}")
                return

            path: str = file.get_path()
            if path not in self.task.task_data.attachments:
                new_attachments: list[str] = self.task.task_data.attachments
                new_attachments.append(path)
                self.task.update_props(["attachments"], [new_attachments])
                self.attachments_list.append(ErrandsAttachment(path))
                self.update_ui()

        dialog = Gtk.FileDialog()
        dialog.open(State.main_window, None, __confirm)


class ErrandsAttachment(Adw.ActionRow):
    def __init__(self, path: str):
        super().__init__()
        self.path: str = path
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_activatable(True)
        self.set_tooltip_text(_("Open File"))
        self.set_title(os.path.basename(self.path))
        self.add_prefix(
            ErrandsButton(
                valign=Gtk.Align.CENTER,
                icon_name="errands-trash-symbolic",
                css_classes=["error", "flat"],
                on_click=self.__on_delete_btn_clicked,
            )
        )
        self.connect("activated", self.__on_click)

    def __on_click(self, *_args):
        file: Gio.File = Gio.File.new_for_path(self.path)
        Gtk.FileLauncher(file=file).launch(State.main_window, None)

    def __on_delete_btn_clicked(self, _btn: ErrandsButton):
        task: Task = State.attachments_window.task
        new_attachments: list[str] = task.task_data.attachments
        new_attachments.remove(self.path)
        task.update_props(["attachments"], [new_attachments])
        State.attachments_window.attachments_list.remove(self)
        State.attachments_window.update_ui()
        del self
