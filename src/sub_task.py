from gi.repository import Gtk, GLib
from .utils import UserData, Markup


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Gtk.Box):
    __gtype_name__ = "SubTask"

    sub_task_text = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()

    def __init__(self, text, parent):
        super().__init__()
        self.parent = parent
        self.text = text
        # Escape text and find URL's'
        if not Markup.is_escaped(text):
            self.text = Markup.escape(self.text)
        self.text = Markup.find_url(self.text)
        self.sub_task_text.props.label = self.text
        # Check if text crosslined and toggle checkbox
        if Markup.is_crosslined(self.text):
            self.sub_task_completed_btn.props.active = True

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        if Markup.is_crosslined(self.text) and btn.props.active:
            self.parent.n_sub_tasks_completed += 1
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

    # def update_data(self, old_text: str, new_text: str):
    #     new_data = UserData.get()
    #     idx = new_data["todos"][self.parent.props.title]["sub"].index(old_text)
    #     new_data["todos"][self.parent.props.title]["sub"][idx] = new_text
    #     UserData.set(new_data)

    # @Gtk.Template.Callback()
    # def on_sub_task_delete(self, _):
    #     self.sub_task_popover.popdown()
    #     new_data = UserData.get()
    #     new_data["todos"][self.parent.get_title()]["sub"].remove(self.props.title)
    #     UserData.set(new_data)
    #     self.parent.remove(self)

    # @Gtk.Template.Callback()
    # def on_sub_task_edit(self, entry):
    #     old_text = self.props.title
    #     new_text = GLib.markup_escape_text(entry.get_buffer().props.text)
    #     if (
    #         new_text == old_text
    #         or new_text == ""
    #         or new_text in UserData.get()["todos"][self.parent.props.title]["sub"]
    #     ):
    #         return
    #     # Change sub-todo
    #     self.update_data(old_text, new_text)
    #     # Set new title
    #     self.props.title = new_text
    #     # Mark as uncompleted
    #     self.sub_task_completed_btn.props.active = False
    #     # Clear entry
    #     entry.get_buffer().props.text = ""
    #     # Hide popup
    #     self.sub_task_popover.popdown()
