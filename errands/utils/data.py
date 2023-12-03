# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
import os
import sqlite3

from typing import Any
from uuid import uuid4
from icalendar import Event, Calendar
from gi.repository import GLib, Secret
from errands.utils.logging import Log


class UserData:
    data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
    db_path: str = os.path.join(data_dir, "data.db")

    @classmethod
    def init(cls):
        # Create data dir if needed
        if not os.path.exists(cls.data_dir):
            os.mkdir(cls.data_dir)
        # Clear cache
        # os.remove(os.path.join(cls.data_dir, "*.ics"))
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
            color TEXT NOT NULL,
            completed INTEGER NOT NULL,
            deleted INTEGER NOT NULL,
            end_date TEXT NOT NULL,
            list_uid TEXT NOT NULL,
            notes TEXT NOT NULL,
            parent TEXT NOT NULL,
            percent_complete INTEGER NOT NULL,
            priority INTEGER NOT NULL,
            start_date TEXT NOT NULL,
            synced INTEGER NOT NULL,
            tags TEXT NOT NULL,
            text TEXT NOT NULL,
            uid TEXT NOT NULL
            )"""
        )
        cls.connection.commit()

    @classmethod
    def add_list(cls, name: str, uuid: str = None):
        Log.info(f"Create '{name}' list")
        uid = str(uuid4()) if not uuid else uuid
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
    def run_sql(cls, *cmds: list[str], fetch: bool = False) -> list[tuple] | None:
        for cmd in cmds:
            cls.cursor.execute(cmd)
        cls.connection.commit()
        return cls.cursor.fetchall() if fetch else None

    @classmethod
    def to_ics(cls, uid) -> str:
        cls.cursor.execute(f"SELECT * FROM tasks WHERE uid = '{uid}'")
        res = cls.cursor.fetchone()
        cal = Calendar()
        cal.add("version", "2.0")
        cal.add("prodid", "-//Errands")
        todo = Event()
        todo.add("uid", res[0])
        todo.add("summary", res[2])
        todo.add("dtstart", datetime.datetime.fromisoformat(res[7]))
        todo.add("dtend", datetime.datetime.fromisoformat(res[8]))
        todo.add("description", res[9])
        todo.add("percent-complete", res[12])
        todo.add("priority", res[13])
        cal.add_component(todo)
        return cal.to_ical().decode("utf-8")

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
            WHERE parent IS ''
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
    def get_tasks_as_dicts(cls, list_uid: str) -> list[dict]:
        cls.cursor.execute(
            f"""SELECT * FROM tasks 
            WHERE list_uid = '{list_uid}'"""
        )
        res = cls.cursor.fetchall()
        tasks = []
        for task in res:
            new_task = {
                "parent": task[6],
                "synced": task[10],
                "text": task[12],
                "uid": task[13],
            }
            tasks.append(new_task)
        return tasks

    @classmethod
    def add_task(
        cls,
        list_uid: str,
        text: str,
        uid: str = "",
        parent: str = "",
        completed: bool = False,
        deleted: bool = False,
        color: str = "",
        start_date: str = "",
        end_date: str = "",
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
