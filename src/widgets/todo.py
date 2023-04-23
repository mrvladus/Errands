from gi.repository import Adw, Gtk
from ..globals import data
from ..data import ReadData, WriteData


class SubTodo(Adw.ActionRow):
    def __init__(self, text, parent):
        super().__init__(title=text)
        self.parent_todo = parent
        # Delete sub-todo button
        del_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            css_classes=["flat"],
            valign="center",
        )
        del_btn.connect("clicked", self.on_delete)
        self.add_prefix(del_btn)

    def on_delete(self, _):
        new_data = ReadData()
        new_data["todos"][self.parent_todo.get_title()]["sub"].remove(self.props.title)
        WriteData(new_data)
        self.parent_todo.remove(self)


class TodoMenu(Gtk.MenuButton):
    def __init__(self, parent):
        super().__init__()
        self.props.icon_name = "view-more-symbolic"
        self.props.css_classes = ["flat"]
        self.props.valign = Gtk.Align.CENTER
        # Delete button
        del_btn = Gtk.Button(icon_name="user-trash-symbolic")
        del_btn.connect("clicked", self.on_delete, parent)
        # Box
        box = Gtk.Box(orientation="horizontal", css_classes=["toolbar"])
        box.append(del_btn)
        self.props.popover = Gtk.Popover(child=box)

    def on_delete(self, btn, parent):
        self.props.popover.popdown()
        new_data = ReadData()
        new_data["todos"].pop(parent.text)
        WriteData(new_data)
        data["todo_list"].remove(parent)


class Todo(Adw.PreferencesGroup):
    def __init__(self, text, subtodos=[]):
        super().__init__()
        self.text = text
        # Expander row
        self.row = Adw.ExpanderRow(
            title=self.text, expanded=True if subtodos != [] else False
        )
        # Sub entry
        sub_entry = Adw.EntryRow(title="Add new sub task")
        sub_entry.connect("entry-activated", self.on_sub_entry_activated)
        self.row.add_row(sub_entry)
        # Menu
        self.row.add_prefix(TodoMenu(self))
        # Sub-todos
        for todo in subtodos:
            self.row.add_row(SubTodo(todo, self.row))

        self.add(self.row)

    def on_sub_entry_activated(self, entry):
        # Check for empty string
        if entry.props.text == "":
            return
        new_data = ReadData()
        # Check if todo exists
        if entry.props.text in new_data["todos"][self.text]:
            return
        new_data["todos"][self.text]["sub"].append(entry.props.text)
        WriteData(new_data)
        self.row.add_row(SubTodo(entry.props.text, self.row))
        entry.props.text = ""
