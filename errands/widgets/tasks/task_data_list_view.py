# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from gi.repository import Adw, Gio, GObject, Gtk


class SingletonMeta(type(GObject.GObject)):
    _instances = {}

    def __call__(cls, task_data, *args, **kwargs):
        if task_data.uid not in cls._instances:
            cls._instances[task_data.uid] = super(SingletonMeta, cls).__call__(
                task_data, *args, **kwargs
            )
        return cls._instances[task_data.uid]


class TaskDataListView(GObject.GObject, metaclass=SingletonMeta):
    def __init__(self, source):
        super().__init__()
        self._source = source

    def __getattr__(self, name):
        return getattr(self._source, name)

    def __setattr__(self, name, value):
        if name == "_source":
            self.__dict__[name] = value
        else:
            setattr(self._source, name, value)
