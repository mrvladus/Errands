# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
import os
from gi.repository import Gtk, GLib  # type:ignore


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class DateTimePicker(Gtk.Box):
    __gtype_name__ = "DateTimePicker"

    # CHILDREN
    label: Gtk.Label = Gtk.Template.Child()
    hours: Gtk.SpinButton = Gtk.Template.Child()
    minutes: Gtk.SpinButton = Gtk.Template.Child()
    calendar: Gtk.Calendar = Gtk.Template.Child()

    # STATE
    __datetime: str = ""
    lock_signals: bool = True

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    # ------ PROPERTIES ------ #

    @property
    def datetime(self) -> str:
        return self.__datetime

    @datetime.setter
    def datetime(self, dt: str) -> str:
        self.lock_signals = True
        if dt:
            dt = datetime.datetime.fromisoformat(dt).strftime("%Y%m%dT%H%M00")
            self.hours.set_value(int(dt[9:11]))
            self.minutes.set_value(int(dt[11:13]))
            self.calendar.select_day(
                GLib.DateTime.new_local(
                    int(dt[:4]), int(dt[4:6]), int(dt[6:8]), 0, 0, 0
                )
            )
        else:
            self.hours.set_value(0)
            self.minutes.set_value(0)
            self.calendar.select_day(GLib.DateTime.new_now_local())

        # Set datetime
        self.__datetime = dt
        self.lock_signals = False
        self.label.set_label(self.human_datetime if dt else _("Set Date"))  # noqa: F821

    @property
    def datetime_as_int(self) -> int:
        return int(f"{self.datetime[:8]}{self.datetime[9:]}") if self.datetime else 0

    @property
    def human_datetime(self) -> str:
        if self.datetime:
            out: str = f"{self.calendar.get_date().format('%d %b')} {self.datetime[9:11]}:{self.datetime[11:13]}"
        else:
            out: str = _("Date")  # noqa: F821
        return out

    # ------ TEMPLATE HANDLERS ------ #

    @Gtk.Template.Callback()
    def _on_clear_clicked(self, btn: Gtk.Button):
        self.datetime = ""

    @Gtk.Template.Callback()
    def _on_date_time_changed(self, *_args):
        # Get hour
        hour: str = str(self.hours.get_value_as_int())
        hour: str = f"0{hour}" if len(hour) == 1 else hour
        # Get min
        min: str = str(self.minutes.get_value_as_int())
        min: str = f"0{min}" if len(min) == 1 else min
        # Get date
        date: str = self.calendar.get_date().format("%Y%m%d")
        # Set date
        self.datetime = f"{date}T{hour}{min}00"

    @Gtk.Template.Callback()
    def _on_now_clicked(self, btn: Gtk.Button):
        self.datetime = datetime.datetime.now().strftime("%Y%m%dT%H%M00")

    @Gtk.Template.Callback()
    def _on_time_preset_clicked(self, btn: Gtk.Button):
        hour, min = btn.get_child().props.label.split(":")
        self.hours.set_value(int(hour))
        self.minutes.set_value(int(min))

    @Gtk.Template.Callback()
    def _on_today_clicked(self, btn: Gtk.Button):
        self.datetime = datetime.datetime.now().strftime("%Y%m%dT000000")

    @Gtk.Template.Callback()
    def _on_tomorrow_clicked(self, btn: Gtk.Button):
        self.datetime = (datetime.datetime.now() + datetime.timedelta(1)).strftime(
            "%Y%m%dT000000"
        )
