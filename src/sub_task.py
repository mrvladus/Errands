from gi.repository import Gtk, GLib
from .utils import UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Gtk.Box):
    __gtype_name__ = "SubTask"

    sub_task_popover = Gtk.Template.Child()
    sub_task_text = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()

    def __init__(self, text, parent):
        super().__init__()
        self.parent = parent
        self.text = text
        # Escape text and find URL's'
        if not Markup.is_escaped(text):
            self.text = Markup.escape(self.text)
        # self.text = Markup.find_url(self.text)
        self.sub_task_text.props.label = Markup.find_url(self.text)
        # Check if text crosslined and toggle checkbox
        if Markup.is_crosslined(self.text):
            self.sub_task_completed_btn.props.active = True

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        # TODO: use label text instead self.text
        if Markup.is_crosslined(self.text) and btn.props.active:
            self.parent.n_sub_tasks_completed += 1
            return
        if not Markup.is_crosslined(self.text) and not btn.props.active:
            return
        if btn.props.active:
            self.text = Markup.add_crossline(self.text)
            self.parent.n_sub_tasks_completed += 1
            self.parent.update_statusbar()
        else:
            self.text = Markup.rm_crossline(self.text)
            self.parent.n_sub_tasks_completed -= 1
            self.parent.update_statusbar()
        self.sub_task_text.props.label = self.text

    @Gtk.Template.Callback()
    def on_sub_task_delete(self, btn):
        self.sub_task_popover.popdown()
        new_data = UserData.get()
        new_data["todos"][self.parent.text]["sub"].remove(self.text)
        UserData.set(new_data)
        self.parent.sub_tasks.remove(self)

    @Gtk.Template.Callback()
    def on_sub_task_edit(self, entry):
        old_text = self.text
        new_text = entry.get_buffer().props.text
        # Escape text and find URL's'
        new_text = Markup.escape(new_text)
        # Return if text the same or empty
        if (
            new_text == old_text
            or new_text == ""
            or new_text in UserData.get()["todos"][self.parent.text]["sub"]
        ):
            return
        # Change sub-task text
        new_data = UserData.get()
        for idx, task in enumerate(new_data["todos"][self.parent.text]["sub"]):
            if task == old_text:
                new_data["todos"][self.parent.text]["sub"][idx] = new_text
                UserData.set(new_data)
                break
        # Set new text
        self.text = Markup.find_url(new_text)
        self.sub_task_text.props.label = self.text
        # Mark as uncompleted
        self.sub_task_completed_btn.props.active = False
        # Clear entry
        entry.get_buffer().props.text = ""
        # Hide popup
        self.sub_task_popover.popdown()
