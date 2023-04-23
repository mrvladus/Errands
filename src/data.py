import os
import json
from gi.repository import GLib
from .globals import VERSION, data

default_data = {
    "version": VERSION,
    "todos": {},
}

data_dir = GLib.get_user_data_dir() + "/list"


def ConvertData():
    # 44.1.x
    todos_v1 = data["gsettings"].get_value("todos").unpack()
    if todos_v1 != []:
        new_data = ReadData()
        for todo in todos_v1:
            new_data["todos"][todo] = {"sub": [], "color": ""}
        WriteData(new_data)
    # 44.2.x
    todos_v2 = data["gsettings"].get_value("todos-v2").unpack()
    if todos_v2 != []:
        new_data = ReadData()
        for todo in todos_v2:
            new_data["todos"][todo[0]] = {"sub": [i for i in todo[1:]], "color": ""}
        WriteData(new_data)


def InitData():
    if not os.path.exists(data_dir):
        os.mkdir(data_dir)
    if not os.path.exists(data_dir + "/data.json"):
        with open(data_dir + "/data.json", "w+") as f:
            json.dump(default_data, f)
        ConvertData()


def ReadData():
    with open(data_dir + "/data.json", "r") as f:
        data = json.load(f)
        return data


def WriteData(data: dict):
    with open(data_dir + "/data.json", "w") as f:
        json.dump(data, f)
