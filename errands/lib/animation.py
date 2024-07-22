# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk  # type:ignore


def property(
    obj: Gtk.Widget,
    prop: str,
    val_from,
    val_to,
    time_ms: int,
) -> None:
    """Animate widget property"""

    def callback(value, _) -> None:
        obj.set_property(prop, value)

    animation = Adw.TimedAnimation.new(
        obj,
        val_from,
        val_to,
        time_ms,
        Adw.CallbackAnimationTarget.new(callback, None),
    )
    animation.play()


def scroll(win: Gtk.ScrolledWindow, scroll_down: bool = True, widget=None) -> None:
    """Animate scrolling"""

    adj = win.get_vadjustment()

    def callback(value, _) -> None:
        adj.set_property("value", value)

    if not widget:
        # Scroll to the top or bottom
        scroll_to = adj.get_upper() if scroll_down else adj.get_lower()
    else:
        scroll_to = widget.get_allocation().height + adj.get_value()

    animation = Adw.TimedAnimation.new(
        win,
        adj.get_value(),
        scroll_to,
        250,
        Adw.CallbackAnimationTarget.new(callback, None),
    )
    animation.play()
