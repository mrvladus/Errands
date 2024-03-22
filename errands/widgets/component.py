# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
from typing import Callable
from errands.lib.utils import get_children
from gi.repository import Gtk, Adw, GObject, GLib, Gio  # type:ignore


class Box(Gtk.Box):
    """
    Gtk.Box with multiple children provided in list.
    """

    def __init__(self, children: list[Gtk.Widget] = [], **kwargs):
        super().__init__(**kwargs)
        for child in children:
            self.append(child)

    def for_each(self, callback: Callable):
        """Call function for each child as argument"""
        for child in get_children(self):
            callback(child)


class Button(Gtk.Button):
    """
    Gtk.Button which takes arguments:
    - label
    - icon
    - callback for "clicked" signal
    - shortcut.
    """

    def __init__(
        self,
        label: str = None,
        icon_name: str = None,
        on_click: Callable = None,
        shortcut: str = None,
        **kwargs,
    ):
        super().__init__(**kwargs)
        self.label: str = label
        content = Adw.ButtonContent()
        if icon_name:
            content.set_icon_name(icon_name)
        if label:
            content.set_label(label)
        if on_click:
            self.connect("clicked", on_click)
        if icon_name:
            self.set_child(content)
        else:
            self.set_label(label)
        if shortcut:
            ctrl = Gtk.ShortcutController(scope=1)
            ctrl.add_shortcut(
                Gtk.Shortcut(
                    trigger=Gtk.ShortcutTrigger.parse_string(shortcut),
                    action=Gtk.ShortcutAction.parse_string("activate"),
                )
            )
            self.add_controller(ctrl)


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
