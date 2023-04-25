import os
import json
from gi.repository import GLib
from .globals import VERSION, data


class UserData:
    data_dir = GLib.get_user_data_dir() + "/list"
    default_data = {
        "version": VERSION,
        "todos": {},
    }

    # Create data dir and data.json file
    @classmethod
    def init(self):
        if not os.path.exists(self.data_dir):
            os.mkdir(self.data_dir)
        if not os.path.exists(self.data_dir + "/data.json"):
            with open(self.data_dir + "/data.json", "w+") as f:
                json.dump(self.default_data, f)
            self.convert()

    # Load user data from json
    @classmethod
    def get(self):
        if not os.path.exists(self.data_dir + "/data.json"):
            self.init()
        with open(self.data_dir + "/data.json", "r") as f:
            data = json.load(f)
            return data

    # Save user data to json
    @classmethod
    def set(self, data: dict):
        with open(self.data_dir + "/data.json", "w") as f:
            json.dump(data, f)

    # Port todos from older versions (for updates)
    def convert(self):
        # 44.1.x
        todos_v1 = data["gsettings"].get_value("todos").unpack()
        if todos_v1 != []:
            new_data = self.get()
            for todo in todos_v1:
                new_data["todos"][todo] = {
                    "sub": [],
                    "color": "",
                }
            self.set(new_data)
        # 44.2.x
        todos_v2 = data["gsettings"].get_value("todos-v2").unpack()
        if todos_v2 != []:
            new_data = self.get()
            for todo in todos_v2:
                new_data["todos"][todo[0]] = {
                    "sub": [i for i in todo[1:]],
                    "color": "",
                }
            self.set(new_data)
