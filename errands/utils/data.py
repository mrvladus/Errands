# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import json
import os
import shutil
import sqlite3
from typing import Any
from uuid import uuid4
from errands.lib.gsettings import GSettings
from gi.repository import GLib
from errands.lib.logging import Log


class UserData:
    data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
    db_path: str = os.path.join(data_dir, "data.db")

    @classmethod
    def init(cls):
        if not os.path.exists(cls.data_dir):
            os.mkdir(cls.data_dir)
        cls.connection = sqlite3.connect(
            cls.db_path, check_same_thread=False, isolation_level=None
        )
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
        cls._convert(cls)

    @classmethod
    def add_list(cls, name: str, uuid: str = None, synced: bool = False) -> str:
        uid = str(uuid4()) if not uuid else uuid
        Log.debug(f"Data: Create '{uid}' list")
        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                "INSERT INTO lists (deleted, name, synced, uid) VALUES (?, ?, ?, ?)",
                (False, name, synced, uid),
            )
            return uid

    @classmethod
    def get_tasks(cls) -> list[str]:
        # Log.debug(f"Data: Get tasks uids")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("SELECT uid FROM tasks")
            return [i[0] for i in cur.fetchall()]

    @classmethod
    def clean_deleted(cls):
        Log.debug("Data: Clean deleted")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("DELETE FROM lists WHERE deleted = 1")
            cur.execute("DELETE FROM tasks WHERE deleted = 1")

    @classmethod
    def get_lists_as_dicts(cls) -> list[dict]:
        # Log.debug("Data: Get lists as dicts")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("SELECT * FROM lists")
            res: list[tuple] = cur.fetchall()
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
    def get_prop(cls, list_uid: str, uid: str, prop: str) -> Any:
        # Log.debug(f"Data: Get '{prop}' for '{uid}'")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                f"SELECT {prop} FROM tasks WHERE uid = ? AND list_uid = ?",
                (uid, list_uid),
            )
            return cur.fetchone()[0]

    @classmethod
    def update_props(
        cls, list_uid: str, uid: str, props: list[str], values: list[Any]
    ) -> None:
        # Log.debug(f"Data: Update props for '{uid}'")

        with cls.connection:
            cur = cls.connection.cursor()
            for i, prop in enumerate(props):
                cur.execute(
                    f"""UPDATE tasks SET {prop} = ? 
                    WHERE uid = '{uid}'
                    AND list_uid = '{list_uid}'""",
                    (values[i],),
                )

    @classmethod
    def run_sql(cls, *cmds: list, fetch: bool = False) -> list[tuple] | None:
        try:
            with cls.connection:
                cur = cls.connection.cursor()
                for cmd in cmds:
                    if isinstance(cmd, tuple):
                        cur.execute(cmd[0], cmd[1])
                    else:
                        cur.execute(cmd)
                return cur.fetchall() if fetch else None
        except Exception as e:
            Log.error(f"Data: {e}")

    @classmethod
    def get_tasks_uids(cls, list_uid: str, parent: str = None) -> list[str]:
        # Log.debug(f"Data: Get tasks uids {f'for {parent}' if parent else ''}")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                f"""SELECT uid FROM tasks
                WHERE list_uid = ?
                AND deleted = 0
                {f"AND parent = '{parent}'" if parent != None else ''}""",
                (list_uid,),
            )
            return [i[0] for i in cur.fetchall()]

    @classmethod
    def get_tasks_uids_tree(cls, list_uid: str, parent: str) -> list[str]:
        # Log.debug(f"Data: Get tasks uids tree for '{parent}'")

        uids: list[str] = []

        def _add(sub_uids: list[str]) -> None:
            for uid in sub_uids:
                uids.append(uid)
                if len(cls.get_tasks_uids(list_uid, uid)) > 0:
                    _add(cls.get_tasks_uids(list_uid, uid))

        _add(cls.get_tasks_uids(list_uid, parent))

        return uids

    @classmethod
    def get_tasks_as_dicts(cls, list_uid: str, parent: str = None) -> list[dict]:
        # Log.debug(f"Data: Get tasks as dicts")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                f"""SELECT * FROM tasks WHERE list_uid = ?
                {f"AND parent = '{parent}'" if parent else ""}""",
                (list_uid,),
            )
            tasks = []
            for task in cur.fetchall():
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

        Log.debug(f"Data: Add task {uid}")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute(
                """INSERT INTO tasks 
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

            return uid

    def _convert(cls):
        old_path = os.path.join(GLib.get_user_data_dir(), "list")
        old_data_file = os.path.join(old_path, "data.json")
        if not os.path.exists(old_data_file):
            return
        Log.debug("Data: convert data file")
        # Get tasks
        try:
            with open(old_data_file, "r") as f:
                data: dict = json.loads(f.read())
        except:
            Log.error("Data: can't read data file")
            return
        # Remove old data folder
        shutil.rmtree(old_path, True)
        # If sync is enabled
        if GSettings.get("sync-provider") != 0:
            uid = cls.add_list(GSettings.get("sync-cal-name"), synced=True)
            GSettings.set("sync-cal-name", "s", "")
        # If sync is disabled
        else:
            uid = cls.add_list("Errands")
        # Add tasks
        for task in data["tasks"]:
            cls.add_task(
                color=task["color"],
                completed=task["completed"],
                deleted=task["id"] in data["deleted"],
                list_uid=uid,
                parent=task["parent"],
                synced=task["synced_caldav"],
                text=task["text"],
                trash=task["deleted"],
                uid=task["id"],
            )
