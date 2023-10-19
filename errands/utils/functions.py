# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Callable
from threading import Thread
from gi.repository import Gtk


def get_children(obj: Gtk.Widget) -> list[Gtk.Widget]:
    """
    Get list of widget's children
    """

    children: list[Gtk.Widget] = []
    child: Gtk.Widget = obj.get_first_child()
    while child:
        children.append(child)
        child = child.get_next_sibling()
    return children


def threaded(function: Callable):
    """
    Decorator to run function in thread.
    Use GLib.idle_add(func) as callback at the end of threaded function
    if you need to change UI from thread.
    It's needed to be called to make changes in UI thread safe.
    """

    def wrapper(*args, **kwargs):
        Thread(target=function, args=args, kwargs=kwargs, daemon=True).start()

    return wrapper
