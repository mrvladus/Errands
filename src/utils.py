import os
import json
from gi.repository import GLib

from .main import VERSION


class Markup:
    """Class for useful markup functions"""

    @classmethod
    def is_escaped(self, text: str) -> bool:
        if (
            "&amp;" in text
            or "&lt;" in text
            or "&gt;" in text
            or "&#39;" in text
            or "&apos;" in text
        ):
            return True
        else:
            return False

    @classmethod
    def escape(self, text: str):
        return GLib.markup_escape_text(text)

    @classmethod
    def is_crosslined(self, text: str) -> bool:
        if text.startswith("<s>") and text.endswith("</s>"):
            return True
        else:
            return False

    @classmethod
    def add_crossline(self, text: str):
        return f"<s>{text}</s>"

    @classmethod
    def rm_crossline(self, text: str):
        return text.replace("<s>", "").replace("</s>", "")

    @classmethod
    def find_url(self, text: str):
        """Convert urls to markup. Make sure to escape text before calling."""
        arr = text.split(" ")
        new_str = []
        for i in arr:
            if i.startswith("http://") or i.startswith("https://"):
                new_str.append(f'<a href="{i}">{i}</a>')
            else:
                new_str.append(i)
        return " ".join(new_str)


class UserData:
    """Class for accessing data file with user tasks"""

    data_dir = GLib.get_user_data_dir() + "/list"
    default_data = {
        "version": VERSION,
        "tasks": [],
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
    @classmethod
    def convert(self):
        pass
