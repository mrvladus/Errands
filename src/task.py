from gi.repository import Gtk
from .sub_task import SubTask


@Gtk.Template(resource_path="/io/github/mrvladus/List/task.ui")
class Task(Gtk.Box):
    __gtype_name__ = "Task"

    # Template items
    task_text = Gtk.Template.Child()
    expand_btn = Gtk.Template.Child()
    task_status = Gtk.Template.Child()
    sub_tasks_revealer = Gtk.Template.Child()
    sub_tasks = Gtk.Template.Child()

    # Data
    n_sub_tasks = 0

    def __init__(self, text, color, sub_tasks, parent):
        super().__init__()
        self.task_text.props.label = text
        # Set accent color
        if color != "":
            self.add_css_class(f"task_{color}")
        # Expand if sub-tasks exists
        if sub_tasks != []:
            self.expand(True)
        # Add sub tasks
        for task in sub_tasks:
            self.add_sub_task(task)
            self.n_sub_tasks += 1
        # Update statusbar
        self.update_statusbar()

    def expand(self, expand: bool):
        self.sub_tasks_revealer.set_reveal_child(expand)
        if expand:
            self.expand_btn.add_css_class("expanded")
        else:
            self.expand_btn.remove_css_class("expanded")

    def add_sub_task(self, text):
        self.sub_tasks.append(SubTask(text, self))

    def update_statusbar(self):
        if self.n_sub_tasks > 0:
            self.task_status.props.visible = True
            self.task_status.set_label(f"Completed sub-tasks: 0 / {self.n_sub_tasks}")
        else:
            self.task_status.props.visible = False

    # --- Template handlers --- #

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        pass

    @Gtk.Template.Callback()
    def on_expand_btn_clicked(self, _):
        if self.sub_tasks_revealer.get_child_revealed():
            self.expand(False)
        else:
            self.expand(True)

    @Gtk.Template.Callback()
    def on_sub_task_added(self, entry):
        self.add_sub_task(entry.get_buffer().props.text)
