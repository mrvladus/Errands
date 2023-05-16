from gi.repository import Gtk, Adw
from .utils import UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Adw.Bin):
    __gtype_name__ = "SubTask"

    sub_task_completed_btn = Gtk.Template.Child()
    sub_task_text = Gtk.Template.Child()
    sub_task_edit_entry = Gtk.Template.Child()
    sub_task_cancel_edit_btn = Gtk.Template.Child()
    sub_task_menu = Gtk.Template.Child()
    sub_task_edit_btn = Gtk.Template.Child()
    sub_task_delete_btn = Gtk.Template.Child()

    # State
    edit_mode = False

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
        # Setup overlay menu
        self.ctrl = Gtk.EventControllerMotion()
        self.ctrl.connect("enter", self.show_menu)
        self.ctrl.connect("leave", self.hide_menu)
        self.add_controller(self.ctrl)

    def show_menu(self, *args):
        if not self.edit_mode:
            self.sub_task_menu.props.visible = True

    def hide_menu(self, *args):
        self.sub_task_menu.props.visible = False

    def update_sub_task(self, new_sub_task):
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.parent.task["text"]:
                for i, sub in enumerate(task["sub"]):
                    if sub["text"] == self.task["text"]:
                        task["sub"][i] = new_sub_task
                        UserData.set(new_data)
                        # Update parent data
                        self.parent.task["sub"] = task["sub"]
                        return

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        if btn.props.active:
            self.task = {"text": self.task["text"], "completed": True}
            self.update_sub_task(self.task)
            self.text = Markup.add_crossline(self.text)
            self.parent.update_statusbar()
        else:
            self.task = {"text": self.task["text"], "completed": False}
            self.update_sub_task(self.task)
            self.text = Markup.rm_crossline(self.text)
            self.parent.update_statusbar()
        self.sub_task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_sub_task_delete_btn_clicked(self, btn):
        # Remove sub-task data
        new_data = UserData.get()
        for task in new_data["tasks"]:
            if task["text"] == self.parent.task["text"]:
                for sub in task["sub"]:
                    if sub["text"] == self.task["text"]:
                        task["sub"].remove(sub)
                        UserData.set(new_data)
                        # Update parent data
                        self.parent.task["sub"] = task["sub"]
                        self.parent.update_statusbar()
                        break
                break
        # Remove sub-task widget
        self.parent.sub_tasks.remove(self)

    @Gtk.Template.Callback()
    def on_sub_task_edit_btn_clicked(self, btn):
        # Switch edit mode
        self.edit_mode = True
        # Hide overlay, label and checkbox
        self.sub_task_menu.props.visible = False
        self.sub_task_text.props.visible = False
        self.sub_task_completed_btn.props.visible = False
        # Show edit entry and button
        self.sub_task_edit_entry.props.visible = True
        self.sub_task_cancel_edit_btn.props.visible = True
        # Set entry text and select it
        self.sub_task_edit_entry.get_buffer().props.text = self.task["text"]
        self.sub_task_edit_entry.select_region(0, len(self.task["text"]))
        self.sub_task_edit_entry.grab_focus()

    @Gtk.Template.Callback()
    def on_sub_task_cancel_edit_btn_clicked(self, _):
        # Switch edit mode
        self.edit_mode = False
        # Show overlay, label and checkbox
        self.sub_task_menu.props.visible = True
        self.sub_task_text.props.visible = True
        self.sub_task_completed_btn.props.visible = True
        # Show edit entry and button
        self.sub_task_edit_entry.props.visible = False
        self.sub_task_cancel_edit_btn.props.visible = False

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
                # Set new data
                print(f"Change sub-task: '{old_text}' to '{new_text}'")
                self.task = {"text": new_text, "completed": False}
                for i, sub in enumerate(task["sub"]):
                    if sub["text"] == old_text:
                        task["sub"][i] = self.task
                        UserData.set(new_data)
                        # Update parent data
                        self.parent.task["sub"] = task["sub"]
                        self.parent.update_statusbar()
                        break
                break
        # Escape text and find URL's'
        self.text = Markup.escape(self.task["text"])
        self.text = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        self.sub_task_completed_btn.props.active = False
        # Set text
        self.sub_task_text.props.label = self.text
        # Show overlay, label and checkbox
        self.sub_task_menu.props.visible = True
        self.sub_task_text.props.visible = True
        self.sub_task_completed_btn.props.visible = True
        # Show edit entry and button
        self.sub_task_edit_entry.props.visible = False
        self.sub_task_cancel_edit_btn.props.visible = False
        # Switch edit mode
        self.edit_mode = False
