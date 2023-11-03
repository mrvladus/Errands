# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.animation import scroll
from errands.utils.gsettings import GSettings
import errands.utils.tasks as TaskUtils
from errands.utils.data import UserData, UserDataDict, UserDataTask
from errands.utils.functions import get_children
from errands.widgets.task_details import TaskDetails
from errands.widgets.trash_item import TrashItem
from errands.widgets.trash_panel import TrashPanel
from gi.repository import Adw, Gtk, GObject, Gio, GLib, Gdk
from errands.widgets.task import Task
from errands.utils.markup import Markup
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.utils.tasks import task_to_ics


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/shortcuts_window.ui")
class ShortcutsWindow(Gtk.ShortcutsWindow):
    __gtype_name__ = "ShortcutsWindow"

    def __init__(self, window: Adw.ApplicationWindow):
        super().__init__()
        self.set_transient_for(window)
        self.present()
