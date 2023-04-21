from gi.repository import Adw
from ..globals import data
from .todo import Todo


class TodoList(Adw.PreferencesPage):
    def __init__(self):
        super().__init__()
        data["todo_list"] = self
        self.load_todos()

    def load_todos(self):
        self.add(Todo("todo1", ["sub1", "sub2"]))
