# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import random
import time
from datetime import datetime
from functools import wraps
from threading import Thread
from typing import Callable

from gi.repository import GLib, Gtk  # type:ignore


def get_human_datetime(date_time: str) -> str:
    if date_time:
        out: str = datetime.fromisoformat(date_time).strftime("%d %B %H:%M")
        if "00:00" in out:
            out = out.removesuffix("00:00")
    else:
        out: str = _("Date")
    return out


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
    Use GLib.idle_add(func) if you need to change UI from thread.
    It's needed to be called to make changes in UI thread safe.
    """

    def wrapper(*args, **kwargs):
        Thread(target=function, args=args, kwargs=kwargs, daemon=True).start()

    return wrapper


def idle_add(func: Callable):
    """Call function with GLib.idle_add() without blocking UI"""

    @wraps(func)
    def wrapper(*args, **kwargs):
        GLib.idle_add(func, *args)

    return wrapper


def timeit(func):
    @wraps(func)
    def timeit_wrapper(*args, **kwargs):
        global PROFILING_LOG
        start_time = time.perf_counter()
        result = func(*args, **kwargs)
        end_time = time.perf_counter()
        total_time = end_time - start_time
        print(f"{func.__name__} | {args} | {total_time:.4f}")
        return result

    return timeit_wrapper


def rgb_to_hex(r: str, g: str, b: str) -> str:
    return "#{:02x}{:02x}{:02x}".format(int(r), int(g), int(b))


def random_hex_color() -> str:
    hex_chars: str = "0123456789ABCDEF"
    return "#" + "".join(random.choice(hex_chars) for _ in range(6))
