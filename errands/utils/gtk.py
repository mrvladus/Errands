# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GLib, Gio, Gtk


class GtkUtils:
    """UI building methods"""

    @staticmethod
    def add_css(widget: Gtk.Widget, classes: list[str]):
        """Add multiple css classes at once"""
        for cls in classes:
            widget.add_css_class(cls)
