from gi.repository import Adw, Gtk
from ..globals import data
from ..data import UserData


class TodoMenu(Gtk.MenuButton):
    def __init__(self, parent):
        super().__init__()
        self.props.icon_name = "view-more-symbolic"
        self.props.css_classes = ["flat"]
        self.props.valign = Gtk.Align.CENTER
        self.props.tooltip_text = "Task menu"
        self.parent = parent
        # Delete button
        del_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            css_classes=["destructive-action"],
            tooltip_text="Delete task",
        )
        del_btn.connect("clicked", self.on_delete, self.parent)
        # Edit entry
        edit_entry = Gtk.Entry(placeholder_text="Edit task")
        edit_entry.connect("activate", self.on_edit, self.parent)
        # 1st menu row
        row1 = Gtk.Box(
            orientation="horizontal",
            css_classes=["toolbar"],
            halign=Gtk.Align.CENTER,
        )
        row1.append(del_btn)
        row1.append(edit_entry)
        # 2nd menu row
        row2 = Gtk.Box(
            orientation="horizontal",
            css_classes=["toolbar"],
            halign=Gtk.Align.CENTER,
        )
        row2.append(self.accent_btn(None, "window-close-symbolic"))
        row2.append(self.accent_btn("red"))
        row2.append(self.accent_btn("orange"))
        row2.append(self.accent_btn("green"))
        row2.append(self.accent_btn("blue"))
        row2.append(self.accent_btn("purple"))
        # Menu box
        box = Gtk.Box(orientation="vertical", spacing=10)
        box.append(row1)
        box.append(row2)
        self.props.popover = Gtk.Popover(child=box)

    def accent_btn(self, color, icon=None):
        btn = Gtk.Button(
            css_classes=["circular", f"btn_{color}"] if color else ["circular"],
            icon_name=icon if icon else "",
        )
        btn.connect("clicked", self.on_accent_clicked, color)
        return btn

    def on_accent_clicked(self, _, color):
        self.props.popover.popdown()
        self.parent.row.props.css_classes = (
            ["expander", f"row_{color}"] if color else ["expander"]
        )
        new_data = UserData.get()
        new_data["todos"][self.parent.text]["color"] = color if color else ""
        UserData.set(new_data)

    def on_delete(self, btn, parent):
        self.props.popover.popdown()
        new_data = UserData.get()
        new_data["todos"].pop(parent.row.props.title)
        UserData.set(new_data)
        data["todo_list"].remove(parent)

    def on_edit(self, entry, parent):
        # Get old and new text
        old_text = parent.row.props.title
        new_text = entry.get_buffer().props.text
        new_data = UserData.get()
        if old_text == new_text or new_text in new_data["todos"] or new_text == "":
            return
        # Create new dict and change todo text
        tmp = UserData.default_data
        for key in new_data["todos"]:
            if key == old_text:
                tmp["todos"][new_text] = new_data["todos"][old_text]
            else:
                tmp["todos"][key] = new_data["todos"][key]
        UserData.set(tmp)
        # Set new title
        parent.row.props.title = new_text
        # Clear entry
        entry.get_buffer().props.text = ""
        # Hide popup
        self.props.popover.popdown()


class SubTodo(Adw.ActionRow):
    def __init__(self, text, parent):
        super().__init__(title=text)
        self.parent = parent
        # Delete sub-todo button
        del_btn = Gtk.Button(
            icon_name="user-trash-symbolic",
            css_classes=["destructive-action"],
            tooltip_text="Delete sub-task",
        )
        del_btn.connect("clicked", self.on_delete)
        # Edit sub-todo
        edit_entry = Gtk.Entry(placeholder_text="Edit task")
        edit_entry.connect("activate", self.on_edit)
        # Box
        box = Gtk.Box(css_classes=["toolbar"])
        box.append(del_btn)
        box.append(edit_entry)
        # Popover
        self.popover = Gtk.Popover(child=box)
        self.add_prefix(
            Gtk.MenuButton(
                icon_name="view-more-symbolic",
                valign="center",
                css_classes=["flat"],
                popover=self.popover,
                tooltip_text="Sub-task menu",
            )
        )

    def on_delete(self, _):
        self.popover.popdown()
        new_data = UserData.get()
        new_data["todos"][self.parent.get_title()]["sub"].remove(self.props.title)
        UserData.set(new_data)
        self.parent.remove(self)

    def on_edit(self, entry):
        old_text = self.props.title
        new_text = entry.get_buffer().props.text
        new_data = UserData.get()
        if (
            new_text == old_text
            or new_text == ""
            or new_text in new_data["todos"][self.parent.props.title]["sub"]
        ):
            return
        # Change sub-todo
        idx = new_data["todos"][self.parent.props.title]["sub"].index(old_text)
        new_data["todos"][self.parent.props.title]["sub"][idx] = new_text
        UserData.set(new_data)
        # Set new title
        self.props.title = new_text
        self.popover.popdown()


class Todo(Adw.PreferencesGroup):
    def __init__(self, text, color, subtodos=[]):
        super().__init__()
        self.text = text
        # Expander row
        self.row = Adw.ExpanderRow(
            title=self.text,
            expanded=True if subtodos != [] else False,
            css_classes=["expander", f"row_{color}"] if color else ["expander"],
        )
        self.add(self.row)
        # Sub entry
        sub_entry = Adw.EntryRow(title="Add new sub task")
        sub_entry.connect("entry-activated", self.on_sub_entry_activated)
        self.row.add_row(sub_entry)
        # Menu
        self.row.add_prefix(TodoMenu(self))
        # Sub-todos
        for todo in subtodos:
            self.row.add_row(SubTodo(todo, self.row))

    def on_sub_entry_activated(self, entry):
        # Check for empty string
        if entry.props.text == "":
            return
        new_data = UserData.get()
        # Check if todo exists
        if entry.props.text in new_data["todos"][self.text]["sub"]:
            return
        new_data["todos"][self.text]["sub"].append(entry.props.text)
        UserData.set(new_data)
        self.row.add_row(SubTodo(entry.props.text, self.row))
        entry.props.text = ""
