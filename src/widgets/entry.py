from gi.repository import Adw
from ..globals import data
from .todo import Todo


class Entry(Adw.PreferencesPage):
    def __init__(self):
        super().__init__()
        self.entry = Adw.EntryRow(title="Add new task")
        self.entry.connect("entry-activated", self.on_entry_activated)

        self.group = Adw.PreferencesGroup()
        self.group.add(self.entry)

        self.add(self.group)

    def on_entry_activated(self, entry):
        data["todo_list"].add(Todo(entry.props.text))
        entry.props.text = ""
