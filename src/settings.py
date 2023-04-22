import os
import json
from gi.repository import GLib
from .globals import VERSION

default_data = {
    "version": VERSION,
    "todos": {},
}

data_dir: str = GLib.get_user_data_dir() + "/list"


def LoadSettings() -> dict:
    if not os.path.exists(data_dir):
        os.makedirs(data_dir, exist_ok=True)
    if not os.path.exists(data_dir + "/data.json"):
        with open(data_dir + "/data.json", "w+") as f:
            json.dump(default_data, f)
            return default_data
    else:
        with open(data_dir + "/data.json", "r") as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError:
                data = None
            return data


def WriteSettings(data: dict):
    with open(data_dir + "/data.json", "w+") as f:
        json.dump(data, f)
