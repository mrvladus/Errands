# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk, GObject


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/lists_panel.ui")
class ListsPanel(Adw.Bin):
    __gtype_name__ = "ListsPanel"

    # Set props
    window = GObject.Property(type=Adw.ApplicationWindow)

    lists = Gtk.Template.Child()
    add_list_window = Gtk.Template.Child()

    def __init__(self):
        super().__init__()

    def add_list(self):
        self.add_list_window.present()

    def load_lists(self):
        pass

    def delete_list(self):
        pass

    def rename_list(self):
        pass

    @Gtk.Template.Callback()
    def on_list_add_btn_clicked(self, btn):
        self.add_list()
