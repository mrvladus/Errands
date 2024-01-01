# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
from typing import Callable
from errands.utils.functions import get_children
from gi.repository import Gtk, Adw, GObject, GLib


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


class DateTime(Gtk.Box):
    """
    Date and time picker widget.
    Datetime returned in yyyymmddThhmmss format.
    """

    datetime: str = ""
    changed = GObject.Signal()
    lock_signals: bool = True

    def __init__(self):
        super().__init__()
        self._build_ui()

    def _build_ui(self):
        self.set_orientation(Gtk.Orientation.VERTICAL)
        # Hour button
        self.hour = Gtk.SpinButton(
            adjustment=Gtk.Adjustment(lower=0, upper=23, step_increment=1)
        )
        self.hour.connect("value-changed", self._on_date_time_changed)
        # Minute button
        self.minutes = Gtk.SpinButton(
            adjustment=Gtk.Adjustment(lower=0, upper=59, step_increment=1)
        )
        self.minutes.connect("value-changed", self._on_date_time_changed)
        # Time box
        self.append(
            Box(
                children=[
                    self.hour,
                    Gtk.Label(label=":"),
                    self.minutes,
                ],
                css_classes=["toolbar"],
                halign="center",
            )
        )
        # Calendar
        self.calendar = Gtk.Calendar()
        self.calendar.connect("day-selected", self._on_date_time_changed)
        self.append(self.calendar)
        # Buttons box
        self.append(
            Box(
                children=[
                    Button(
                        label=_("Today"),
                        on_click=self._on_today_btn_clicked,
                        hexpand=True,
                    ),
                    Button(
                        label=_("Tomorrow"),
                        on_click=self._on_tomorrow_btn_clicked,
                        hexpand=True,
                    ),
                ],
                css_classes=["toolbar"],
                homogeneous=True,
            )
        )
        self.append(
            Box(
                children=[
                    Button(
                        label=_("Now"),
                        on_click=self._on_now_btn_clicked,
                        hexpand=True,
                    ),
                    Button(
                        label=_("Clear"),
                        on_click=self._on_clear_btn_clicked,
                        hexpand=True,
                    ),
                ],
                css_classes=["toolbar"],
                homogeneous=True,
            )
        )

    def _on_date_time_changed(self, *_args):
        # Get hour
        hour = str(self.hour.get_value_as_int())
        hour = f"0{hour}" if len(hour) == 1 else hour
        # Get min
        min = str(self.minutes.get_value_as_int())
        min = f"0{min}" if len(min) == 1 else min
        # Get date
        date = self.calendar.get_date().format("%Y%m%d")
        # Set date
        self.datetime = f"{date}T{hour}{min}00"
        if not self.lock_signals:
            self.emit("changed")

    def _on_now_btn_clicked(self, _btn):
        self.set_datetime(datetime.datetime.now().strftime("%Y%m%dT%H%M00"))
        self.emit("changed")

    def _on_today_btn_clicked(self, _btn):
        self.set_datetime(datetime.datetime.now().strftime("%Y%m%dT000000"))
        self.emit("changed")

    def _on_tomorrow_btn_clicked(self, _btn):
        self.set_datetime(
            (datetime.datetime.now() + datetime.timedelta(1)).strftime("%Y%m%dT000000")
        )
        self.emit("changed")

    def _on_clear_btn_clicked(self, _btn):
        self.datetime = ""
        self.emit("changed")

    def get_datetime(self) -> str:
        return self.datetime

    def get_human_datetime(self) -> str:
        if self.datetime:
            out: str = f"{self.datetime[9:11]}:{self.datetime[11:13]}, {self.calendar.get_date().format('%d %B, %Y')}"
        else:
            out: str = _("Not Set")
        return out

    def get_datetime_as_int(self) -> int:
        return int(f"{self.datetime[:8]}{self.datetime[9:]}") if self.datetime else 0

    def set_datetime(self, dt: str):
        self.lock_signals = True
        if dt:
            self.hour.set_value(int(dt[9:11]))
            self.minutes.set_value(int(dt[11:13]))
            self.calendar.select_day(
                GLib.DateTime.new_local(
                    int(dt[:4]), int(dt[4:6]), int(dt[6:8]), 0, 0, 0
                )
            )
        else:
            self.hour.set_value(0)
            self.minutes.set_value(0)
            self.calendar.select_day(GLib.DateTime.new_now_local())
        # Set datetime
        self.datetime = dt
        self.lock_signals = False
