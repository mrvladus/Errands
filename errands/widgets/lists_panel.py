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


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/lists_panel.ui")
class ListsPanel(Adw.Bin):
    __gtype_name__ = "ListsPanel"

    lists = Gtk.Template.Child()

    def __init__(self):
        super().__init__()

    def add_list(self):
        pass

    def load_lists(self):
        pass

    def delete_list(self):
        pass

    def rename_list(self):
        pass

    # @Gtk.Template.Callback()
    # def on_list_add_btn_clicked(self, btn):
    #     print("add")
