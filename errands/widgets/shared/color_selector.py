# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GObject, Gtk  # type:ignore

from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.widgets.shared.components.buttons import ErrandsCheckButton


class ErrandsColorSelector(Gtk.Box):
    color_selected: GObject.Signal = GObject.Signal(
        name="color-selected",
        arg_types=(ErrandsCheckButton, str),
    )

    def __init__(self):
        super().__init__()
        self.__build_ui()

    @property
    def buttons(self) -> list[ErrandsCheckButton]:
        return get_children(self)

    def __build_ui(self) -> None:
        none_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="none",
            on_toggle=lambda *_: self.emit("color-selected", none_color_btn, "none"),
            css_classes=["accent-color-btn", "accent-color-btn-none"],
            tooltip_text=_("None"),
        )
        self.append(none_color_btn)

        blue_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="blue",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit("color-selected", blue_color_btn, "blue"),
            css_classes=["accent-color-btn", "accent-color-btn-blue"],
            tooltip_text=_("Blue"),
        )
        self.append(blue_color_btn)

        green_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="green",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit("color-selected", green_color_btn, "green"),
            css_classes=["accent-color-btn", "accent-color-btn-green"],
            tooltip_text=_("Green"),
        )
        self.append(green_color_btn)

        yellow_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="yellow",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit(
                "color-selected", yellow_color_btn, "yellow"
            ),
            css_classes=["accent-color-btn", "accent-color-btn-yellow"],
            tooltip_text=_("Yellow"),
        )
        self.append(yellow_color_btn)

        orange_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="orange",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit(
                "color-selected", orange_color_btn, "orange"
            ),
            css_classes=["accent-color-btn", "accent-color-btn-orange"],
            tooltip_text=_("Orange"),
        )
        self.append(orange_color_btn)

        red_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="red",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit("color-selected", red_color_btn, "red"),
            css_classes=["accent-color-btn", "accent-color-btn-red"],
            tooltip_text=_("Red"),
        )
        self.append(red_color_btn)

        purple_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="purple",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit(
                "color-selected", purple_color_btn, "purple"
            ),
            css_classes=["accent-color-btn", "accent-color-btn-purple"],
            tooltip_text=_("Purple"),
        )
        self.append(purple_color_btn)

        brown_color_btn: ErrandsCheckButton = ErrandsCheckButton(
            name="brown",
            group=none_color_btn,
            on_toggle=lambda *_: self.emit("color-selected", brown_color_btn, "brown"),
            css_classes=["accent-color-btn", "accent-color-btn-brown"],
            tooltip_text=_("Brown"),
        )
        self.append(brown_color_btn)

    def select_color(self, color: str):
        if color == "":
            color = "none"
        Log.debug(f"Color Selector: set color to '{color}'")
        for btn in self.buttons:
            if btn.get_name() == color:
                btn.set_active(True)
                break
