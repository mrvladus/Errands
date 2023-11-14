# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
import os
import sqlite3

from typing import Any
from uuid import uuid4
from gi.repository import GLib
from errands.utils.logging import Log


class UserData:
    data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
    db_path: str = os.path.join(data_dir, "data.db")

    @classmethod
    def init(cls):
        # Create data dir if needed
        if not os.path.exists(cls.data_dir):
            os.mkdir(cls.data_dir)
        cls.connection = sqlite3.connect(cls.db_path, check_same_thread=False)
        cls.cursor = cls.connection.cursor()
        # Create lists table
        cls.cursor.execute(
            """CREATE TABLE IF NOT EXISTS lists (
            uid TEXT NOT NULL,
            name TEXT NOT NULL
            )"""
        )
        # Create tasks table
        cls.cursor.execute(
            """CREATE TABLE IF NOT EXISTS tasks (
            uid TEXT NOT NULL,
            list_uid TEXT NOT NULL,
            text TEXT NOT NULL,
            parent TEXT,
            completed INTEGER NOT NULL,
            deleted INTEGER NOT NULL,
            color TEXT,
            start_date TEXT,
            end_date TEXT,
            notes TEXT,
            tags TEXT,
            percent_complete INTEGER NOT NULL,
            priority INTEGER NOT NULL,
            synced INTEGER
            )"""
        )
        cls.connection.commit()

    @classmethod
    def add_list(cls, name: str):
        Log.info(f"Create '{name}' list")
        uid = str(uuid4())
        cls.cursor.execute(
            "INSERT INTO lists (uid, name) VALUES (?, ?)",
            (uid, name),
        )
        cls.connection.commit()
        return uid

    @classmethod
    def get_lists(cls) -> tuple[str, str]:
        cls.cursor.execute("SELECT uid, name FROM lists")
        return cls.cursor.fetchall()

    @classmethod
    def get_prop(cls, list_uid: str, uid: str, prop: str) -> Any:
        cls.cursor.execute(
            f"""SELECT {prop} FROM tasks 
            WHERE uid = '{uid}'
            AND list_uid = '{list_uid}'"""
        )
        return cls.cursor.fetchone()[0]

    @classmethod
    def update_prop(cls, list_uid: str, uid: str, prop: str, value) -> None:
        cls.cursor.execute(
            f"""UPDATE tasks SET {prop} = ? 
            WHERE uid = '{uid}'
            AND list_uid = '{list_uid}'""",
            (value,),
        )
        cls.connection.commit()

    @classmethod
    def run_sql(cls, sql: str):
        cls.cursor.execute(sql)
        cls.connection.commit()
        return cls.cursor.fetchall()

    @classmethod
    def count(cls):
        pass

    @classmethod
    def move_before(cls, list_uid: str, task_to_move_uid: str, before_uid: str):
        cls.cursor.execute("CREATE TABLE tmp AS SELECT * FROM tasks WHERE 0")
        ids = cls.get_tasks(list_uid)
        ids.insert(ids.index(before_uid), ids.pop(ids.index(task_to_move_uid)))
        for id in ids:
            cls.cursor.execute(
                f"INSERT INTO tmp SELECT * FROM tasks WHERE uid = '{id}'"
            )
        cls.cursor.execute("DROP TABLE tasks")
        cls.cursor.execute("ALTER TABLE tmp RENAME TO tasks")
        cls.connection.commit()

    @classmethod
    def remove_deleted(cls, list_uid: str):
        cls.cursor.execute(
            f"""DELETE FROM tasks 
            WHERE deleted = 1
            AND list_uid = '{list_uid}'"""
        )

    @classmethod
    def get_sub_tasks(cls, list_uid: str, parent_uid: str) -> list[str]:
        cls.cursor.execute(
            f"""SELECT uid FROM tasks 
            WHERE parent = '{parent_uid}'
            AND list_uid = '{list_uid}'"""
        )
        res = cls.cursor.fetchall()
        return [f[0] for f in res]

    @classmethod
    def get_toplevel_tasks(cls, list_uid: str) -> list[str]:
        cls.cursor.execute(
            f"""SELECT uid FROM tasks 
            WHERE parent IS NULL
            AND list_uid = '{list_uid}'"""
        )
        res = cls.cursor.fetchall()
        return [f[0] for f in res]

    @classmethod
    def get_tasks(cls, list_uid: str) -> list[str]:
        cls.cursor.execute(
            f"""SELECT uid FROM tasks 
            WHERE list_uid = '{list_uid}'"""
        )
        res = cls.cursor.fetchall()
        return [i[0] for i in res]

    @classmethod
    def add_task(
        cls,
        list_uid: str,
        text: str,
        uid: str = None,
        parent: str = None,
        completed: bool = False,
        deleted: bool = False,
        color: str = "",
        start_date: str = None,
        end_date: str = None,
        notes: str = "",
        percent_complete: int = 0,
        priority: int = 0,
        tags: str = "",
        synced: bool = False,
    ) -> str:
        if not uid:
            uid = str(uuid4())
        if not start_date:
            start_date = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
        if not end_date:
            end_date = format(
                datetime.datetime.now() + datetime.timedelta(days=1), "%Y%m%dT%H%M%S"
            )
        cls.cursor.execute(
            f"""INSERT INTO tasks 
            (uid, list_uid, text, parent, completed, deleted, color, notes, percent_complete, priority, start_date, end_date, tags, synced) 
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                uid,
                list_uid,
                text,
                parent,
                completed,
                deleted,
                color,
                notes,
                percent_complete,
                priority,
                start_date,
                end_date,
                tags,
                synced,
            ),
        )
        cls.connection.commit()
        return uid

    @classmethod
    def delete_task(cls, uid: str):
        pass


# class UserData:
#     """Class for accessing data file with user tasks"""

#     data_dir: str = os.path.join(GLib.get_user_data_dir(), "list")
#     default_data: UserDataDict = {"version": VERSION, "tasks": [], "deleted": []}
#     validated: bool = False

#     def _create_file(self):
#         """
#         Create data file if not exists
#         """

#         if not os.path.exists(os.path.join(self.data_dir, "data.json")):
#             with open(os.path.join(self.data_dir, "data.json"), "w+") as f:
#                 json.dump(self.default_data, f)
#                 Log.debug(
#                     f"Create data file at: {os.path.join(self.data_dir, 'data.json')}"
#                 )

#     @classmethod
#     def clean_orphans(self, data: UserDataDict) -> UserDataDict:
#         ids: list[str] = [t["id"] for t in data["tasks"]]
#         orphans: list[str] = [
#             t for t in data["tasks"] if t["parent"] not in ids and t["parent"] != ""
#         ]
#         for id in orphans:
#             data["tasks"].remove(id)
#         if GSettings.get("sync-provider") == 0:
#             data["deleted"].clear()
#         return data

#     @classmethod
#     def create_copy(self):
#         shutil.copy(
#             os.path.join(self.data_dir, "data.json"),
#             os.path.join(self.data_dir, "data.old.json"),
#         )
#         self.set(self.default_data)

#     # Load user data from json
#     @classmethod
#     def get(self) -> UserDataDict:
#         self._create_file(self)
#         try:
#             with open(os.path.join(self.data_dir, "data.json"), "r") as f:
#                 data: UserDataDict = json.load(f)
#                 if data["version"] != VERSION:
#                     converted_data: UserDataDict = self.convert(data)
#                     self.set(converted_data)
#                     return converted_data
#                 if not self.validate(data):
#                     raise
#                 return data
#         except:
#             Log.error(
#                 f"Data file is corrupted. Creating backup at {os.path.join(self.data_dir, 'data.old.json')}"
#             )
#             self.create_copy()

#     # Save user data to json
#     @classmethod
#     def set(self, data: UserDataDict) -> None:
#         with open(os.path.join(self.data_dir, "data.json"), "w") as f:
#             self.clean_orphans(data)
#             json.dump(data, f, indent=4, ensure_ascii=False)

#     # Validate data json
#     @classmethod
#     def validate(self, data: str | UserDataDict) -> bool:
#         if self.validated:
#             return True

#         Log.debug("Validating data file")

#         if type(data) == dict:
#             val_data = data
#         # Validate JSON
#         else:
#             try:
#                 val_data = json.loads(data)
#             except json.JSONDecodeError:
#                 Log.error("Data file is not JSON")
#                 return False
#         # Validate schema
#         for key in ["version", "tasks"]:
#             if not key in val_data:
#                 Log.error(f"Data file is not valid. Key doesn't exists: '{key}'")
#                 return False
#         # Validate tasks
#         if val_data["tasks"]:
#             for task in val_data["tasks"]:
#                 for key in [
#                     "id",
#                     "parent",
#                     "text",
#                     "color",
#                     "completed",
#                     "deleted",
#                     "synced_caldav",
#                     # "synced_todoist",
#                 ]:
#                     if not key in task:
#                         Log.error(
#                             f"Data file is not valid. Key doesn't exists: '{key}'"
#                         )
#                         return False
#         Log.debug("Data file is valid")
#         self.validated = True
#         return True

#     @classmethod
#     def convert(self, data: UserDataDict) -> UserDataDict:
#         """
#         Port tasks from older versions (for updates)
#         """

#         Log.debug("Converting data file")

#         ver: str = data["version"]

#         # Versions 44.6.x
#         if ver.startswith("44.6"):
#             new_tasks: list[dict] = []
#             for task in data["tasks"]:
#                 new_task = {
#                     "id": task["id"],
#                     "parent": "",
#                     "text": task["text"],
#                     "color": task["color"],
#                     "completed": task["completed"],
#                     "deleted": "history" in data and task["id"] in data["history"],
#                     "synced_caldav": False,
#                     # "synced_todoist": False,
#                 }
#                 new_tasks.append(new_task)
#                 if task["sub"] != []:
#                     for sub in task["sub"]:
#                         new_sub = {
#                             "id": sub["id"],
#                             "parent": task["id"],
#                             "text": sub["text"],
#                             "color": "",
#                             "completed": sub["completed"],
#                             "deleted": "history" in data
#                             and sub["id"] in data["history"],
#                             "synced_caldav": False,
#                             # "synced_todoist": False,
#                         }
#                         new_tasks.append(new_sub)
#             data["tasks"] = new_tasks
#             if "history" in data:
#                 del data["history"]
#             data["deleted"] = []

#         elif ver.startswith("44.7"):
#             data["deleted"] = []
#             for task in data["tasks"]:
#                 task["synced_caldav"] = False
#                 # task["synced_todoist"] = False

#         elif ver == "45.0" and os.path.exists(
#             os.path.join(self.data_dir, "data.old.json")
#         ):
#             pass

#         data["version"] = VERSION
#         return data
