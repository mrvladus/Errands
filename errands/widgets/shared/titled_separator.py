# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Gtk, GObject  # type:ignore


class TitledSeparator(Gtk.Box):
    title = GObject.Property(type=str, default="")

    def __init__(
        self, title: str = "", margins: tuple[int, int, int, int] = (0, 0, 0, 0)
    ):
        super().__init__()
        self.__build_ui()
        self.label.set_label(title)
        self.set_margin_start(margins[0])
        self.set_margin_end(margins[1])
        self.set_margin_top(margins[2])
        self.set_margin_bottom(margins[3])

    def __build_ui(self) -> None:
        self.add_css_class("dim-label")
        self.set_spacing(12)

        # Label
        self.label = Gtk.Label(css_classes=["caption"])
        self.label.bind_property(
            "label",
            self,
            "title",
            GObject.BindingFlags.SYNC_CREATE,
            GObject.BindingFlags.BIDIRECTIONAL,
        )

        self.append(Gtk.Separator(hexpand=True, valign=Gtk.Align.CENTER))
        self.append(self.label)
        self.append(Gtk.Separator(hexpand=True, valign=Gtk.Align.CENTER))
