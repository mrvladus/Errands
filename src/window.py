# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from gi.repository import Gio, Adw, Gtk, GLib
from __main__ import VERSION
from .utils import Animate, GSettings, TaskUtils, UserData
from .task import Task
from .preferences import PreferencesWindow


@Gtk.Template(resource_path="/io/github/mrvladus/List/window.ui")
class Window(Adw.ApplicationWindow):
    __gtype_name__ = "Window"

    undo_btn = Gtk.Template.Child()
    delete_completed_tasks_btn = Gtk.Template.Child()
    separator = Gtk.Template.Child()
    tasks_list = Gtk.Template.Child()
    status = Gtk.Template.Child()
    scroll_up_btn_rev = Gtk.Template.Child()
    scrolled_window = Gtk.Template.Child()
    drop_motion_ctrl = Gtk.Template.Child()
    about_window = Gtk.Template.Child()

    # State
    scrolling = False

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        # Remember window size
        GSettings.bind("width", self, "default_width")
        GSettings.bind("height", self, "default_height")
        GSettings.bind("maximized", self, "maximized")
        # Setup theme
        Adw.StyleManager.get_default().set_color_scheme(GSettings.get("theme"))
        self.get_settings().props.gtk_icon_theme_name = "Adwaita"
        # Create actions for main menu
        self.create_action(
            "preferences",
            lambda *_: PreferencesWindow(self).show(),
            ["<primary>comma"],
        )
        self.create_action("about", self.on_about_action)
        self.create_action(
            "quit",
            lambda *_: self.props.application.quit(),
            ["<primary>q"],
        )
        self.load_tasks()
        self.update_status()
        self.update_undo()

    def create_action(self, name: str, callback: callable, shortcuts=None) -> None:
        action = Gio.SimpleAction.new(name, None)
        action.connect("activate", callback)
        if shortcuts:
            self.props.application.set_accels_for_action(f"app.{name}", shortcuts)
        self.props.application.add_action(action)

    def load_tasks(self) -> None:
        # Clear history
        if GSettings.get("clear-history-on-startup"):
            print("Clearing history...")
            data: dict = UserData.get()
            data["tasks"] = [t for t in data["tasks"] if not t["id"] in data["history"]]
            for task in data["tasks"]:
                task["sub"] = [s for s in task["sub"] if not s["id"] in data["history"]]
            data["history"] = []
            UserData.set(data)
        # Load tasks
        data: dict = UserData.get()
        print("Loading tasks...")
        for task in data["tasks"]:
            new_task = Task(task, self)
            self.tasks_list.append(new_task)
            if task["id"] not in data["history"]:
                new_task.toggle_visibility()

    def on_about_action(self, *args) -> None:
        """Show about window"""
        self.about_window.props.version = VERSION
        self.about_window.show()

    def update_status(self) -> None:
        """Update progress bar on the top"""
        n_total = 0
        n_completed = 0
        data: dict = UserData.get()
        for task in data["tasks"]:
            if task["id"] not in data["history"]:
                n_total += 1
                if task["completed"]:
                    n_completed += 1
        # Update progress bar animation
        Animate.property(
            self.status,
            "fraction",
            self.status.props.fraction,
            n_completed / n_total if n_total > 0 else 0,
            250,
        )
        # Show delete completed button
        self.delete_completed_tasks_btn.set_sensitive(n_completed > 0)

    def update_undo(self) -> None:
        """Change undo button sensitivity"""
        data: dict = UserData.get()
        # Remove old tasks from history
        if len(data["history"]) > GSettings.get("history-size"):
            to_del_id = data["history"].pop(0)
            for task in data["tasks"]:
                if task["id"] == to_del_id:
                    data["tasks"].remove(task)
                    break
                else:
                    for sub in task["sub"]:
                        if sub["id"] == to_del_id:
                            task["sub"].remove(sub)
                            break
            UserData.set(data)
        # Set sensitivity of undo button
        self.undo_btn.props.sensitive = len(data["history"]) > 0
        self.update_status()

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_dnd_scroll(self, _motion, _x, y) -> bool:
        """Autoscroll while dragging task"""

        def auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.drop_motion_ctrl.contains_pointer():
                return False
            adj = self.scrolled_window.get_vadjustment()
            if scroll_up:
                adj.set_value(adj.get_value() - 2)
                return True
            else:
                adj.set_value(adj.get_value() + 2)
                return True

        MARGIN = 50
        height = self.scrolled_window.get_allocation().height
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, auto_scroll, True)
        elif y > height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, auto_scroll, False)
        else:
            self.scrolling = False

    @Gtk.Template.Callback()
    def on_scroll(self, adj):
        """Show scroll up button"""
        self.scroll_up_btn_rev.set_reveal_child(adj.get_value() > 0)
        if adj.get_value() > 0:
            self.separator.add_css_class("separator")
        else:
            self.separator.remove_css_class("separator")

    @Gtk.Template.Callback()
    def on_scroll_up_btn_clicked(self, _):
        """Scroll up"""
        Animate.scroll(self.scrolled_window, False)

    @Gtk.Template.Callback()
    def on_task_added(self, entry: Gtk.Entry) -> None:
        """Add new task"""
        text: str = entry.props.text
        # Check for empty string or task exists
        if text == "":
            return
        # Add new task
        new_data: dict = UserData.get()
        new_task: dict = TaskUtils.new_task(text)
        new_data["tasks"].append(new_task)
        UserData.set(new_data)
        task = Task(new_task, self)
        self.tasks_list.append(task)
        task.toggle_visibility()
        # Clear entry
        entry.props.text = ""
        # Scroll to the end
        Animate.scroll(self.scrolled_window, True)
        self.update_status()

    @Gtk.Template.Callback()
    def on_delete_completed_tasks_btn_clicked(self, _) -> None:
        """Hide completed tasks"""
        history: list = UserData.get()["history"]
        tasks = self.tasks_list.observe_children()
        for i in range(tasks.get_n_items()):
            task = tasks.get_item(i)
            if task.task["completed"] and task.task["id"] not in history:
                task.delete()
        self.update_status()

    @Gtk.Template.Callback()
    def on_undo_clicked(self, _) -> None:
        data: dict = UserData.get()
        if len(data["history"]) == 0:
            return
        last_task_id: str = data["history"][-1]
        for task in data["tasks"]:
            if task["id"] == last_task_id:
                childrens = self.tasks_list.observe_children()
                for i in range(childrens.get_n_items()):
                    # Get task
                    child = childrens.get_item(i)
                    if child.task["id"] == last_task_id:
                        child.toggle_visibility()
                        data["history"].pop()
                        UserData.set(data)
                        self.update_status()
                        self.update_undo()
                        return
            else:
                tasks = self.tasks_list.observe_children()
                for i in range(tasks.get_n_items()):
                    task = tasks.get_item(i)
                    sub_tasks = task.sub_tasks.observe_children()
                    for i in range(sub_tasks.get_n_items()):
                        sub = sub_tasks.get_item(i)
                        if sub.task["id"] == last_task_id:
                            sub.toggle_visibility()
                            data["history"].pop()
                            UserData.set(data)
                            task.update_statusbar()
                            self.update_undo()
                            return
