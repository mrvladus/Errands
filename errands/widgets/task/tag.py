# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Gtk, Gdk  # type:ignore


class Tag(Gtk.Box):
    def __init__(self, title: str):
        super().__init__()
        self.title = title
        self.set_hexpand(False)
        self.set_valign(Gtk.Align.CENTER)
        self.add_css_class("tag")
        self.append(Gtk.Label(label=title))
        btn = Gtk.Button(
            icon_name="errands-close-symbolic", cursor=Gdk.Cursor(name="pointer")
        )
        btn.connect("clicked", lambda *_: self.get_parent().get_parent().remove(self))
        self.append(btn)
