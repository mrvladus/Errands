# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime

from gi.repository import Adw, GLib, Gtk  # type:ignore

from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.buttons import ErrandsButton, ErrandsSpinButton


class DateTimePicker(Gtk.Box):
    # STATE
    __datetime: str = ""
    lock_signals: bool = True

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.__build_ui()

    # ------ PRIVATE METHODS ------ #

    def __build_ui(self) -> None:
        self.set_orientation(Gtk.Orientation.VERTICAL)
        self.set_spacing(12)

        # Date and Time label
        self.label = Gtk.Label(halign=Gtk.Align.CENTER, css_classes=["title-2"])
        self.append(self.label)

        # Separator
        self.append(
            Gtk.Separator(margin_end=6, margin_start=6, css_classes=["dim-label"])
        )

        # Time
        self.hours: ErrandsSpinButton = ErrandsSpinButton(
            orientation=Gtk.Orientation.VERTICAL,
            numeric=True,
            adjustment=Gtk.Adjustment(lower=0, upper=23, step_increment=1),
            on_value_changed=self._on_date_time_changed,
        )
        self.minutes: ErrandsSpinButton = ErrandsSpinButton(
            orientation=Gtk.Orientation.VERTICAL,
            numeric=True,
            adjustment=Gtk.Adjustment(lower=0, upper=59, step_increment=1),
            on_value_changed=self._on_date_time_changed,
        )

        self.append(
            ErrandsBox(
                halign=Gtk.Align.CENTER,
                spacing=6,
                children=[
                    # Time
                    ErrandsBox(
                        halign=Gtk.Align.CENTER,
                        spacing=6,
                        children=[
                            self.hours,
                            Gtk.Label(label=":", css_classes=["heading"]),
                            self.minutes,
                        ],
                    ),
                    # Presets
                    ErrandsBox(
                        orientation=Gtk.Orientation.VERTICAL,
                        spacing=6,
                        children=[
                            # Time presets
                            ErrandsBox(
                                orientation=Gtk.Orientation.VERTICAL,
                                spacing=6,
                                children=[
                                    ErrandsBox(
                                        spacing=6,
                                        homogeneous=True,
                                        children=[
                                            ErrandsButton(
                                                on_click=self._on_time_preset_clicked,
                                                css_classes=["flat"],
                                                child=Adw.ButtonContent(
                                                    label="09:00",
                                                    icon_name="errands-daytime-morning-symbolic",
                                                ),
                                            ),
                                            ErrandsButton(
                                                on_click=self._on_time_preset_clicked,
                                                css_classes=["flat"],
                                                child=Adw.ButtonContent(
                                                    label="13:00",
                                                    icon_name="errands-theme-light-symbolic",
                                                ),
                                            ),
                                        ],
                                    ),
                                    ErrandsBox(
                                        spacing=6,
                                        homogeneous=True,
                                        children=[
                                            ErrandsButton(
                                                on_click=self._on_time_preset_clicked,
                                                css_classes=["flat"],
                                                child=Adw.ButtonContent(
                                                    label="17:00",
                                                    icon_name="errands-daytime-sunset-symbolic",
                                                ),
                                            ),
                                            ErrandsButton(
                                                on_click=self._on_time_preset_clicked,
                                                css_classes=["flat"],
                                                child=Adw.ButtonContent(
                                                    label="20:00",
                                                    icon_name="errands-theme-dark-symbolic",
                                                ),
                                            ),
                                        ],
                                    ),
                                ],
                            ),
                            # Separator
                            Gtk.Separator(
                                margin_end=6, margin_start=6, css_classes=["dim-label"]
                            ),
                            # Day presets
                            ErrandsBox(
                                orientation=Gtk.Orientation.VERTICAL,
                                spacing=6,
                                children=[
                                    ErrandsBox(
                                        spacing=6,
                                        homogeneous=True,
                                        children=[
                                            ErrandsButton(
                                                on_click=self._on_today_clicked,
                                                css_classes=["flat"],
                                                label=_("Today"),
                                            ),
                                            ErrandsButton(
                                                on_click=self._on_tomorrow_clicked,
                                                css_classes=["flat"],
                                                label=_("Tomorrow"),
                                            ),
                                        ],
                                    ),
                                    ErrandsBox(
                                        spacing=6,
                                        homogeneous=True,
                                        children=[
                                            ErrandsButton(
                                                on_click=self._on_now_clicked,
                                                css_classes=["flat"],
                                                label=_("Now"),
                                            ),
                                            ErrandsButton(
                                                on_click=self._on_clear_clicked,
                                                css_classes=["flat"],
                                                child=Adw.ButtonContent(
                                                    label=_("Clear"),
                                                    icon_name="errands-delete-all-symbolic",
                                                ),
                                            ),
                                        ],
                                    ),
                                ],
                            ),
                        ],
                    ),
                ],
            )
        )

        # Separator
        self.append(
            Gtk.Separator(margin_end=6, margin_start=6, css_classes=["dim-label"])
        )

        # Calendar
        self.calendar = Gtk.Calendar()
        self.calendar.connect("day-selected", self._on_date_time_changed)
        self.append(self.calendar)

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
        self.label.set_label(self.human_datetime if dt else _("Set Date"))

    @property
    def datetime_as_int(self) -> int:
        return int(f"{self.datetime[:8]}{self.datetime[9:]}") if self.datetime else 0

    @property
    def human_datetime(self) -> str:
        if self.datetime:
            out: str = f"{self.calendar.get_date().format('%d %b')} {self.datetime[9:11]}:{self.datetime[11:13]}"
        else:
            out: str = _("Date")
        return out

    # ------ SIGNAL HANDLERS ------ #

    def _on_day_preset_clicked(self, btn: Gtk.Button) -> None:
        pass

    def _on_clear_clicked(self, btn: Gtk.Button):
        self.datetime = ""

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

    def _on_now_clicked(self, btn: Gtk.Button):
        self.datetime = datetime.datetime.now().strftime("%Y%m%dT%H%M00")

    def _on_time_preset_clicked(self, btn: Gtk.Button):
        hour, min = btn.get_child().props.label.split(":")
        self.hours.set_value(int(hour))
        self.minutes.set_value(int(min))

    def _on_today_clicked(self, btn: Gtk.Button):
        self.datetime = datetime.datetime.now().strftime("%Y%m%dT000000")

    def _on_tomorrow_clicked(self, btn: Gtk.Button):
        self.datetime = (datetime.datetime.now() + datetime.timedelta(1)).strftime(
            "%Y%m%dT000000"
        )
