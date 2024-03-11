# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Callable
from threading import Thread
from gi.repository import Gtk  # type:ignore
from errands.lib.logging import Log


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


def catch_errors(function: Callable):
    """Catch errors and log them"""

    def wrapper(*args, **kwargs):
        try:
            function(*args, **kwargs)
        except Exception as e:
            Log.error(e)


from functools import wraps
import time


def timeit(func):
    @wraps(func)
    def timeit_wrapper(*args, **kwargs):
        start_time = time.perf_counter()
        result = func(*args, **kwargs)
        end_time = time.perf_counter()
        total_time = end_time - start_time
        print(f"Function '{func.__name__}' of {args}: Took {total_time:.4f} seconds")
        return result

    return timeit_wrapper
