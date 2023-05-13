from gi.repository import Gtk
from .utils import UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Gtk.Box):
    __gtype_name__ = "SubTask"

    sub_task_popover = Gtk.Template.Child()
    sub_task_text = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()

    def __init__(self, task: dict, parent):
        super().__init__()
        print("Add sub-task: ", task)

        self.parent = parent
        self.task = task
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if sub-task completed and toggle checkbox
        if self.task["completed"]:
            self.sub_task_completed_btn.props.active = True
        # Set text
        self.sub_task_text.props.label = self.text

    def update_sub_task(self, new_sub_task):
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.parent.task["text"]:
                print("found: ", self.parent.task["text"])
                for i, sub in enumerate(task["sub"]):
                    if sub["text"] == self.task["text"]:
                        task["sub"][i] = new_sub_task
                        UserData.set(new_data)
                        return

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        if btn.props.active:
            self.task = {"text": self.task["text"], "completed": True}
            self.update_sub_task(self.task)
            self.text = Markup.add_crossline(self.text)
            self.parent.n_sub_tasks_completed += 1
            self.parent.update_statusbar()
        else:
            self.task = {"text": self.task["text"], "completed": False}
            self.update_sub_task(self.task)
            self.text = Markup.rm_crossline(self.text)
            self.parent.n_sub_tasks_completed -= 1
            self.parent.update_statusbar()
        self.sub_task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_sub_task_delete(self, btn):
        self.sub_task_popover.popdown()
        # Remove sub-task data
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.parent.task["text"]:
                for sub in task["sub"]:
                    if sub["text"] == self.task["text"]:
                        task["sub"].remove(sub)
                        break
                break
        UserData.set(new_data)
        # Remove sub-task widget
        self.parent.sub_tasks.remove(self)

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry):
        old_text = self.task["text"]
        new_text = entry.get_buffer().props.text
        # Return if text the same or empty
        if new_text == old_text or new_text == "":
            return
        # Return if sub-task exists
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.parent.task["text"]:
                # Return if sub-task exists
                for sub in task["sub"]:
                    if sub["text"] == new_text:
                        return
        # Set new text
        self.task = {"text": new_text, "completed": False}
        self.update_sub_task(self.task)
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        self.sub_task_completed_btn.props.active = False
        # Set text
        self.sub_task_text.props.label = self.text
        # Clear entry
        entry.get_buffer().props.text = ""
        # Hide popup
        self.sub_task_popover.popdown()
