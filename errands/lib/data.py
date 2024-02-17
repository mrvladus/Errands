# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import json
import os
import shutil
import sqlite3
from typing import Any, TypedDict
from uuid import uuid4
from errands.lib.gsettings import GSettings
from gi.repository import GLib  # type:ignore
from errands.lib.logging import Log


class TaskListData(TypedDict):
    deleted: bool
    name: str
    synced: bool
    uid: str


class TaskData(TypedDict):
    color: str
    completed: bool
    deleted: bool
    end_date: str
    expanded: bool
    list_uid: str
    notes: str
    parent: str
    percent_complete: int
    priority: int
    start_date: str
    synced: bool
    tags: str
    text: str
    trash: bool
    uid: str


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
        uid: str = str(uuid4()) if not uuid else uuid
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
    def get_lists_as_dicts(cls) -> list[TaskListData]:
        # Log.debug("Data: Get lists as dicts")

        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("SELECT * FROM lists")
            res: list[tuple] = cur.fetchall()
            lists: list[TaskListData] = []
            for i in res:
                lists.append(
                    TaskListData(
                        deleted=bool(i[0]),
                        name=i[1],
                        synced=bool(i[2]),
                        uid=i[3],
                    )
                )

            return lists

    @classmethod
    def move_task_after(cls, list_uid: str, task_uid: str, after_uid: str) -> None:
        tasks: list[TaskData] = UserData.get_tasks_as_dicts()

        # Find tasks to move
        task_to_move: TaskData = None
        task_to_move_before: TaskData = None
        for task in tasks:
            if task["list_uid"] == list_uid and task["uid"] == task_uid:
                task_to_move = task
            elif task["list_uid"] == list_uid and task["uid"] == after_uid:
                task_to_move_before = task

        # Move task up
        task_to_move = tasks.pop(tasks.index(task_to_move))
        tasks.insert(tasks.index(task_to_move_before) + 1, task_to_move)

        # Run SQL
        values: list[tuple[str, str]] = ((t["list_uid"], t["uid"]) for t in tasks)
        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("CREATE TABLE IF NOT EXISTS tmp AS SELECT * FROM tasks WHERE 0")
            cur.executemany(
                f"""INSERT INTO tmp SELECT * FROM tasks
                    WHERE list_uid = ? AND uid = ?""",
                values,
            )
            cur.execute("DROP TABLE tasks")
            cur.execute("ALTER TABLE tmp RENAME TO tasks")

    @classmethod
    def move_task_before(cls, list_uid: str, task_uid: str, before_uid: str) -> None:
        tasks: list[TaskData] = UserData.get_tasks_as_dicts()

        # Find tasks to move
        task_to_move_down: TaskData = None
        task_to_move_up: TaskData = None
        for task in tasks:
            if task["list_uid"] == list_uid and task["uid"] == before_uid:
                task_to_move_down = task
            elif task["list_uid"] == list_uid and task["uid"] == task_uid:
                task_to_move_up = task

        # Move task up
        task_to_move_up = tasks.pop(tasks.index(task_to_move_up))
        tasks.insert(tasks.index(task_to_move_down), task_to_move_up)

        # Run SQL
        values: list[tuple[str, str]] = ((t["list_uid"], t["uid"]) for t in tasks)
        with cls.connection:
            cur = cls.connection.cursor()
            cur.execute("CREATE TABLE IF NOT EXISTS tmp AS SELECT * FROM tasks WHERE 0")
            cur.executemany(
                f"""INSERT INTO tmp SELECT * FROM tasks
                    WHERE list_uid = ? AND uid = ?""",
                values,
            )
            cur.execute("DROP TABLE tasks")
            cur.execute("ALTER TABLE tmp RENAME TO tasks")

    @classmethod
    def execute(
        cls, cmd: str, values: tuple = (), fetch: bool = False
    ) -> list[tuple] | None:
        try:
            with cls.connection:
                cur = cls.connection.cursor()
                cur.execute(cmd, values)
                if fetch:
                    return cur.fetchall()
        except Exception as e:
            Log.error(f"Data: {e}")

    @classmethod
    def move_task_to_list(
        cls,
        task_uid: str,
        old_list_uid: str,
        new_list_uid: str,
        parent: str,
        synced: bool,
    ) -> None:
        sub_tasks_uids: list[str] = cls.get_tasks_uids_tree(old_list_uid, task_uid)
        tasks: list[TaskData] = cls.get_tasks_as_dicts(old_list_uid)
        task_dict: TaskData = [i for i in tasks if i["uid"] == task_uid][0]
        sub_tasks_dicts: list[TaskData] = [
            i for i in tasks if i["uid"] in sub_tasks_uids
        ]
        # Move task
        if old_list_uid == new_list_uid:
            cls.run_sql(
                f"DELETE FROM tasks WHERE list_uid = '{old_list_uid}' AND uid = '{task_uid}'"
            )
        else:
            cls.update_props(old_list_uid, task_uid, ["deleted"], [True])
        task_dict["list_uid"] = new_list_uid
        task_dict["parent"] = parent
        task_dict["synced"] = synced
        cls.add_task(**task_dict)
        # Move sub-tasks
        for sub_dict in sub_tasks_dicts:
            if old_list_uid == new_list_uid:
                cls.run_sql(
                    f"""DELETE FROM tasks
                    WHERE list_uid = '{old_list_uid}'
                    AND uid = '{sub_dict['uid']}'"""
                )
            else:
                cls.update_props(old_list_uid, sub_dict["uid"], ["deleted"], [True])
            sub_dict["list_uid"] = new_list_uid
            sub_dict["synced"] = synced
            cls.add_task(**sub_dict)

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
        """
        Get all sub-task uids recursively
        """
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
    def get_tasks_as_dicts(
        cls, list_uid: str = None, parent: str = None
    ) -> list[TaskData]:
        # Log.debug(f"Data: Get tasks as dicts")

        with cls.connection:
            cur = cls.connection.cursor()
            if not list_uid and not parent:
                cur.execute("SELECT * FROM tasks")
            else:
                cur.execute(
                    f"""SELECT * FROM tasks WHERE list_uid = ?
                    {f"AND parent = '{parent}'" if parent else ""}""",
                    (list_uid,),
                )
            tasks: list[TaskData] = []
            for task in cur.fetchall():
                tasks.append(
                    TaskData(
                        color=task[0],
                        completed=bool(task[1]),
                        deleted=bool(task[2]),
                        end_date=task[3],
                        expanded=bool(task[4]),
                        list_uid=task[5],
                        notes=task[6],
                        parent=task[7],
                        percent_complete=int(task[8]),
                        priority=int(task[9]),
                        start_date=task[10],
                        synced=bool(task[11]),
                        tags=task[12],
                        text=task[13],
                        trash=bool(task[14]),
                        uid=task[15],
                    )
                )

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
        insert_at_the_top: bool = False,
    ) -> str:
        if not uid:
            uid: str = str(uuid4())

        Log.debug(f"Data: Add task {uid}")

        with cls.connection:
            cur = cls.connection.cursor()
            if not insert_at_the_top:
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
            else:
                cur.execute(
                    "CREATE TABLE IF NOT EXISTS tmp AS SELECT * FROM tasks WHERE 0"
                )
                cur.execute(
                    """INSERT INTO tmp 
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
                cur.execute("INSERT INTO tmp SELECT * FROM tasks")
                cur.execute("DROP TABLE tasks")
                cur.execute("ALTER TABLE tmp RENAME TO tasks")

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
