# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import datetime
import os
import json
import shutil
import uuid
from gi.repository import GLib, Gio, Adw, Gtk
from __main__ import VERSION, APP_ID


class Animate:
    """Class for creating UI animations using Adw.Animation"""

    @staticmethod
    def property(obj: Gtk.Widget, prop: str, val_from, val_to, time_ms: int) -> None:
        """Animate widget property"""

        def callback(value, _) -> None:
            obj.set_property(prop, value)

        animation = Adw.TimedAnimation.new(
            obj,
            val_from,
            val_to,
            time_ms,
            Adw.CallbackAnimationTarget.new(callback, None),
        )
        animation.play()

    @staticmethod
    def scroll(win: Gtk.ScrolledWindow, scroll_down: bool = True, widget=None) -> None:
        """Animate scrolling"""

        adj = win.get_vadjustment()

        def callback(value, _) -> None:
            adj.set_property("value", value)

        if not widget:
            # Scroll to the top or bottom
            scroll_to = adj.get_upper() if scroll_down else adj.get_lower()
        else:
            scroll_to = widget.get_allocation().height + adj.get_value()

        animation = Adw.TimedAnimation.new(
            win,
            adj.get_value(),
            scroll_to,
            250,
            Adw.CallbackAnimationTarget.new(callback, None),
        )
        animation.play()


class GSettings:
    """Class for accessing gsettings"""

    gsettings = None

    @classmethod
    def bind(self, setting, obj, prop) -> None:
        self.gsettings.bind(setting, obj, prop, 0)

    @classmethod
    def get(self, setting: str):
        return self.gsettings.get_value(setting).unpack()

    @classmethod
    def set(self, setting: str, gvariant: str, value) -> None:
        self.gsettings.set_value(setting, GLib.Variant(gvariant, value))

    @classmethod
    def init(self) -> None:
        Log.debug("Initialize GSettings")
        self.gsettings = Gio.Settings.new(APP_ID)


class Log:
    """Logging class"""

    data_dir: str = GLib.get_user_data_dir() + "/list"
    log_file: str = data_dir + "/log.txt"
    log_old_file: str = data_dir + "/log.old.txt"

    @classmethod
    def init(self):
        if os.path.exists(self.log_file):
            os.rename(self.log_file, self.log_old_file)
        self.log(self, f"Log for Errands {VERSION}")

    @classmethod
    def debug(self, msg: str) -> None:
        print(f"\033[33;1m[DEBUG]\033[0m {msg}")
        self.log(self, f"[DEBUG] {msg}")

    @classmethod
    def error(self, msg: str) -> None:
        print(f"\033[31;1m[ERROR]\033[0m {msg}")
        self.log(self, f"[ERROR] {msg}")

    @classmethod
    def info(self, msg: str) -> None:
        print(f"\033[32;1m[INFO]\033[0m {msg}")
        self.log(self, f"[INFO] {msg}")

    def log(self, msg: str) -> None:
        try:
            with open(self.log_file, "a") as f:
                f.write(msg + "\n")
        except OSError:
            self.error("Can't write to the log file")


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
    def escape(self, text: str) -> str:
        return GLib.markup_escape_text(text)

    @classmethod
    def unescape(self, text: str) -> str:
        new_text = text
        new_text = new_text.replace("&amp;", "&")
        new_text = new_text.replace("&lt;", "<")
        new_text = new_text.replace("&gt;", ">")
        new_text = new_text.replace("&#39;", "'")
        new_text = new_text.replace("&apos;", "'")
        return new_text

    @classmethod
    def is_crosslined(self, text: str) -> bool:
        if text.startswith("<s>") and text.endswith("</s>"):
            return True
        else:
            return False

    @classmethod
    def add_crossline(self, text: str) -> str:
        return f"<s>{text}</s>"

    @classmethod
    def rm_crossline(self, text: str) -> str:
        return text.replace("<s>", "").replace("</s>", "")

    @classmethod
    def find_url(self, text: str) -> str:
        """Convert urls to markup. Make sure to escape text before calling."""
        arr: list = text.split(" ")
        new_str = []
        for i in arr:
            if i.startswith("http://") or i.startswith("https://"):
                new_str.append(f'<a href="{i}">{i}</a>')
            else:
                new_str.append(i)
        return " ".join(new_str)

    @classmethod
    def remove_url(self, text: str) -> str:
        if "<a href=" in text:
            return text.split('"')[1]
        else:
            return text


class TaskUtils:
    """Task related functions"""

    @classmethod
    def generate_id(self) -> str:
        """Generate unique immutable id for task"""
        return str(uuid.uuid4())

    @classmethod
    def new_task(self, text: str) -> dict:
        return {
            "id": self.generate_id(),
            "text": text,
            "sub": [],
            "color": "",
            "completed": False,
        }

    @classmethod
    def new_sub_task(self, text: str) -> dict:
        return {
            "id": self.generate_id(),
            "text": text,
            "completed": False,
        }


class UserData:
    """Class for accessing data file with user tasks"""

    data_dir: str = GLib.get_user_data_dir() + "/list"
    default_data = {
        "version": VERSION,
        "tasks": [],
        "history": [],
    }

    @classmethod
    def init(self) -> None:
        Log.debug("Initialize user data")
        # Create data dir
        if not os.path.exists(self.data_dir):
            os.mkdir(self.data_dir)
        # Create data file if not exists
        if not os.path.exists(self.data_dir + "/data.json"):
            with open(self.data_dir + "/data.json", "w+") as f:
                json.dump(self.default_data, f)
        # Create new file if old is corrupted
        try:
            with open(self.data_dir + "/data.json", "r") as f:
                data: dict = json.load(f)
        except json.JSONDecodeError:
            Log.error("Data file is corrupted. Creating new.")
            shutil.copy(self.data_dir + "/data.json", self.data_dir + "/data_old.json")
            Log.error("Old file is saved at: ", self.data_dir + "/data_old.json")
            with open(self.data_dir + "/data.json", "w+") as f:
                json.dump(self.default_data, f)
        self.convert()

    # Backup user data
    @classmethod
    def backup(self) -> None:
        Log.debug(
            "Create backup file at: " + GLib.get_home_dir() + "/list_backup_data.json"
        )
        data = self.get()
        with open(GLib.get_home_dir() + "/list_backup_data.json", "w+") as f:
            json.dump(data, f, indent=4)

    # Load user data from json
    @classmethod
    def get(self) -> dict:
        if not os.path.exists(self.data_dir + "/data.json"):
            self.init()
        with open(self.data_dir + "/data.json", "r") as f:
            data: dict = json.load(f)
            return data

    # Save user data to json
    @classmethod
    def set(self, data: dict) -> None:
        with open(self.data_dir + "/data.json", "w") as f:
            json.dump(data, f, indent=4)

    # Port tasks from older versions (for updates)
    @classmethod
    def convert(self) -> None:
        data: dict = self.get()
        ver: str = data["version"]
        # Bugfix for 44.5
        if ver == "":
            data["version"] = "44.5"
            self.set(data)
            ver = "44.5"
        # Versions 44.3.x and 44.4.x
        if ver.startswith("44.4") or ver.startswith("44.3"):
            new_data: dict = self.default_data
            old_tasks: list = data["todos"]
            for task in old_tasks:
                new_sub_tasks = []
                for sub in old_tasks[task]["sub"]:
                    new_text: str = Markup.unescape(sub)
                    new_text = Markup.remove_url(new_text)
                    new_sub_tasks.append(
                        {
                            "id": TaskUtils.generate_id(),
                            "text": Markup.rm_crossline(new_text),
                            "completed": True if Markup.is_crosslined(sub) else False,
                        }
                    )
                new_data["tasks"].append(
                    {
                        "id": TaskUtils.generate_id(),
                        "text": Markup.rm_crossline(task),
                        "sub": new_sub_tasks,
                        "color": old_tasks[task]["color"],
                        "completed": True if Markup.is_crosslined(task) else False,
                    }
                )
            UserData.set(new_data)
            return
        # Versions 44.5.x
        elif ver.startswith("44.5"):
            new_data: dict = self.get()
            new_data["version"] = VERSION
            new_data["history"] = []
            for task in new_data["tasks"]:
                task["id"] = TaskUtils.generate_id()
                for sub in task["sub"]:
                    sub["id"] = TaskUtils.generate_id()
            self.set(new_data)
