# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from gi.repository import Adw, Gtk  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.shared.components.buttons import ErrandsButton


class TrashItem(Adw.ActionRow):
    def __init__(self, task: TaskData) -> None:
        super().__init__()
        self.task_dict: TaskData = task
        self.__build_ui()

    def __build_ui(self) -> None:
        self.props.height_request = 60
        self.set_title_selectable(True)
        self.set_margin_top(6)
        self.set_margin_bottom(6)
        self.add_css_class("card")
        self.add_suffix(
            ErrandsButton(
                tooltip_text=_("Restore"),
                icon_name="errands-check-toggle-symbolic",
                valign=Gtk.Align.CENTER,
                css_classes=["circular", "flat"],
                on_click=self._on_restore_btn_clicked,
            )
        )

    def _on_restore_btn_clicked(self, _) -> None:
        """Restore task and its parents"""

        Log.info(f"Restore task: {self.task_dict.uid}")

        parents_uids: list[str] = UserData.get_parents_uids_tree(
            self.task_dict.list_uid, self.task_dict.uid
        )
        parents_uids.append(self.task_dict.uid)
        for uid in parents_uids:
            UserData.update_props(self.task_dict.list_uid, uid, ["trash"], [False])
            State.get_task(self.task_dict.list_uid, uid).toggle_visibility(True)

        State.trash_page.update_ui()
        State.today_page.update_ui()
