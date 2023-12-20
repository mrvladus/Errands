# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
import os
import sqlite3
from typing import Any
from uuid import uuid4
from icalendar import Event, Calendar
from gi.repository import GLib
from errands.utils.logging import Log


class UserData:
    data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
    db_path: str = os.path.join(data_dir, "data.db")

    @classmethod
    def init(cls):
        if not os.path.exists(cls.data_dir):
            os.mkdir(cls.data_dir)
        cls.connection = sqlite3.connect(cls.db_path, check_same_thread=False)
        cls.run_sql(
            """CREATE TABLE IF NOT EXISTS lists (
            deleted INTEGER NOT NULL,
            name TEXT NOT NULL,
            synced INTEGER NOT NULL,
            uid TEXT NOT NULL
            )""",
            """CREATE TABLE IF NOT EXISTS tasks (
            color TEXT NOT NULL,
            completed INTEGER NOT NULL,
            deleted INTEGER NOT NULL,
            end_date TEXT NOT NULL,
            expanded INTEGER NOT NULL,
            list_uid TEXT NOT NULL,
            notes TEXT NOT NULL,
            parent TEXT NOT NULL,
            percent_complete INTEGER NOT NULL,
            priority INTEGER NOT NULL,
            start_date TEXT NOT NULL,
            synced INTEGER NOT NULL,
            tags TEXT NOT NULL,
            text TEXT NOT NULL,
            trash INTEGER NOT NULL,
            uid TEXT NOT NULL
            )""",
        )

    @classmethod
    def add_list(cls, name: str, uuid: str = None, synced: bool = False) -> str:
        uid = str(uuid4()) if not uuid else uuid
        Log.info(f"Data: Create '{uid}' list")
        cls.run_sql(
            f"""INSERT INTO lists (deleted, name, synced, uid) 
            VALUES (0, '{name}', {synced}, '{uid}')"""
        )
        return uid

    @classmethod
    def get_lists_as_dicts(cls) -> dict:
        res = cls.run_sql("SELECT * FROM lists", fetch=True)
        lists = []
        for i in res:
            data = {
                "deleted": bool(i[0]),
                "name": i[1],
                "synced": bool(i[2]),
                "uid": i[3],
            }
            lists.append(data)
        return lists

    @classmethod
    def get_prop(cls, uid: str, prop: str) -> Any:
        return cls.run_sql(
            f"""SELECT {prop} FROM tasks 
                WHERE uid = '{uid}'""",
            fetch=True,
        )[0][0]

    @classmethod
    def update_props(
        cls, list_uid: str, uid: str, props: list[str], values: list
    ) -> None:
        with cls.connection:
            cur = cls.connection.cursor()
            for i, prop in enumerate(props):
                cur.execute(
                    f"""UPDATE tasks SET {prop} = ? 
                    WHERE uid = '{uid}'
                    AND list_uid = '{list_uid}'""",
                    (values[i],),
                )
            cls.connection.commit()

    @classmethod
    def run_sql(cls, *cmds: list[str], fetch: bool = False) -> list[tuple] | None:
        with cls.connection:
            cur = cls.connection.cursor()
            for cmd in cmds:
                cur.execute(cmd)
            cls.connection.commit()
            return cur.fetchall() if fetch else None

    @classmethod
    def get_tasks_uids(cls, list_uid: str) -> list[str]:
        res = cls.run_sql(
            f"""SELECT uid FROM tasks 
                WHERE list_uid = '{list_uid}'
                AND deleted = 0""",
            fetch=True,
        )
        return [i[0] for i in res]

    @classmethod
    def get_sub_tasks_uids(cls, list_uid: str, parent: str) -> list[str]:
        res = cls.run_sql(
            f"""SELECT uid FROM tasks 
                WHERE list_uid = '{list_uid}'
                AND parent = '{parent}'
                AND deleted = 0""",
            fetch=True,
        )
        return [i[0] for i in res]

    @classmethod
    def get_tasks_as_dicts(cls, list_uid: str) -> list[dict]:
        res = cls.run_sql(
            f"""SELECT * FROM tasks WHERE list_uid = '{list_uid}'""",
            fetch=True,
        )
        tasks = []
        for task in res:
            new_task = {
                "color": task[0],
                "completed": bool(task[1]),
                "deleted": bool(task[2]),
                "end_date": task[3],
                "expanded": bool(task[4]),
                "list_uid": task[5],
                "notes": task[6],
                "parent": task[7],
                "percent_complete": int(task[8]),
                "priority": int(task[9]),
                "start_date": task[10],
                "synced": bool(task[11]),
                "tags": task[12],
                "text": task[13],
                "trash": bool(task[14]),
                "uid": task[15],
            }
            tasks.append(new_task)
        return tasks

    @classmethod
    def add_task(
        cls,
        color: str = "",
        completed: bool = False,
        deleted: bool = False,
        end_date: str = "",
        expanded: bool = False,
        list_uid: str = "",
        notes: str = "",
        parent: str = "",
        percent_complete: int = 0,
        priority: int = 0,
        start_date: str = "",
        synced: bool = False,
        tags: str = "",
        text: str = "",
        trash: bool = False,
        uid: str = "",
    ) -> str:
        if not uid:
            uid = str(uuid4())
        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                f"""INSERT INTO tasks 
                (uid, list_uid, text, parent, completed, deleted, color, notes, percent_complete, priority, start_date, end_date, tags, synced, expanded, trash) 
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
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
                    expanded,
                    trash,
                ),
            )
            cls.connection.commit()
            return uid


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
