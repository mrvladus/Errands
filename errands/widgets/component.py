# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Callable
from gi.repository import Adw, Gio  # type:ignore


class ConfirmDialog(Adw.MessageDialog):
    """Confirmation dialog"""

    def __init__(
        self,
        text: str,
        confirm_text: str,
        style: Adw.ResponseAppearance,
        confirm_callback: Callable,
    ):
        super().__init__()
        self.__text = text
        self.__confirm_text = confirm_text
        self.__style = style
        self.__build_ui()
        self.connect("response", confirm_callback)
        self.present()

    def __build_ui(self):
        self.set_transient_for(Gio.Application.get_default().get_active_window())
        self.set_hide_on_close(True)
        self.set_heading(_("Are you sure?"))
        self.set_body(self.__text)
        self.set_default_response("confirm")
        self.set_close_response("cancel")
        self.add_response("cancel", _("Cancel"))
        self.add_response("confirm", self.__confirm_text)
        self.set_response_appearance("confirm", self.__style)
