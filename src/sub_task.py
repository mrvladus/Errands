from gi.repository import Gtk, GLib
from .utils import UserData


@Gtk.Template(resource_path="/io/github/mrvladus/List/sub_task.ui")
class SubTask(Gtk.Box):
    __gtype_name__ = "SubTask"

    sub_task_text = Gtk.Template.Child()
    sub_task_completed_btn = Gtk.Template.Child()

    def __init__(self, text, parent):
        super().__init__()
        self.sub_task_text.props.label = text
        # self.text = text
        # self.parent = parent
        # # Escape markup symbols
        # if "<s>" in text:
        #     self.props.title = self.text
        #     self.sub_task_completed_btn.props.active = True
        # else:
        #     old_text = self.text
        #     # Detect if text was escaped
        #     if (
        #         "&amp;" in old_text
        #         or "&lt;" in old_text
        #         or "&gt;" in old_text
        #         or "&#39;" in old_text
        #     ):
        #         self.text = old_text
        #     # If not then escape it
        #     else:
        #         self.text = GLib.markup_escape_text(self.text)
        #     self.update_data(old_text, self.text)
        #     self.props.title = self.text

    @Gtk.Template.Callback()
    def on_completed_btn_toggled(self, btn):
        pass

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

    # @Gtk.Template.Callback()
    # def on_sub_task_complete_toggle(self, btn):
    #     old_text = self.props.title
    #     active = btn.props.active
    #     # Ignore at app launch
    #     if "<s>" in old_text and active:
    #         return
    #     # Ignore when sub task was just edited
    #     if not active and "<s>" not in old_text:
    #         return
    #     new_sub_task = f"<s>{old_text}</s>" if active else old_text[3:-4]
    #     self.props.title = new_sub_task
    #     # Save new sub task
    #     self.update_data(old_text, new_sub_task)
