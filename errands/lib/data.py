# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import datetime
import json
import os
from queue import Empty, Queue
import shutil
import sqlite3
from dataclasses import asdict, dataclass, field
from threading import Thread
from typing import Any, Iterable
from uuid import uuid4

from gi.repository import GLib  # type:ignore

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log


@dataclass
class ErrandsData:
    tags: list[TagsData]
    lists: list[TaskListData]
    tasks: list[TaskData]


@dataclass
class TagsData:
    text: str


@dataclass
class TaskListData:
    color: str = ""
    deleted: bool = False
    name: str = ""
    show_completed: bool = True
    synced: bool = False
    uid: str = ""


@dataclass
class TaskData:
    color: str = ""
    completed: bool = False
    changed_at: str = ""
    created_at: str = ""
    deleted: bool = False
    due_date: str = ""
    expanded: bool = False
    list_uid: str = ""
    notes: str = ""
    parent: str = ""
    percent_complete: int = 0
    priority: int = 0
    rrule: str = ""
    start_date: str = ""
    synced: bool = False
    tags: list[str] = field(default_factory=lambda: [])
    text: str = ""
    toolbar_shown: bool = False
    trash: bool = False
    uid: str = ""

    def __post_init__(self):
        """Set default values that need to be calculated"""

        now: str = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
        if not self.created_at:
            self.created_at = now
        if not self.changed_at:
            self.changed_at = now


class UserDataJSON:
    def __init__(self) -> None:
        self.__data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
        self.__data_file_path: str = os.path.join(self.__data_dir, "data.json")

        self.__tags_data: list[TagsData] = []
        self.__task_lists_data: list[TaskListData] = []
        self.__tasks_data: list[TaskData] = []

    # ------ PROPERTIES ------ #

    @property
    def data(self) -> ErrandsData:
        return ErrandsData(
            tags=self.__tags_data, lists=self.__task_lists_data, tasks=self.__tasks_data
        )

    @data.setter
    def data(self, new_data: ErrandsData):
        self.__tags_data = new_data.tags
        self.__task_lists_data = new_data.lists
        self.__tasks_data = new_data.tasks
        self.__write_data()

    @property
    def tags(self) -> list[TagsData]:
        return self.__tags_data

    @tags.setter
    def tags(self, new_data: list[TagsData]):
        self.__tags_data = new_data
        self.__write_data()

    @property
    def task_lists(self) -> list[TaskListData]:
        return self.__task_lists_data

    @task_lists.setter
    def task_lists(self, lists_data: list[TaskListData]):
        self.__task_lists_data = lists_data
        self.__write_data()

    @property
    def tasks(self) -> list[TaskData]:
        return self.__tasks_data

    @tasks.setter
    def tasks(self, tasks_data: list[TaskData]):
        self.__tasks_data = tasks_data
        self.__write_data()

    # ------ PUBLIC METHODS ------ #

    def add_list(
        self, name: str, uuid: str = None, synced: bool = False
    ) -> TaskListData:
        uid: str = str(uuid4()) if not uuid else uuid

        Log.debug(f"Data: Create list '{uid}'")

        new_list = TaskListData(deleted=False, name=name, uid=uid, synced=synced)
        data: list[TaskListData] = self.task_lists
        data.append(new_list)
        self.task_lists = data

        return new_list

    def add_task(self, **kwargs) -> TaskData:
        data: list[TaskData] = self.tasks
        new_task = TaskData(**kwargs)
        if not new_task.uid:
            new_task.uid = str(uuid4())
        Log.debug(f"Data: Add task '{new_task.uid}'")
        if not GSettings.get("task-list-new-task-position-top"):
            data.append(new_task)
        else:
            data.insert(0, new_task)
        self.tasks = data

        return new_task

    def clean_deleted(self) -> None:
        Log.debug("Data: Clean deleted")

        data: ErrandsData = self.data
        data.lists = [lst for lst in data.lists if not lst.deleted]
        data.tasks = [t for t in data.tasks if not t.deleted]
        self.data = data

    def delete_list(self, list_uid: str) -> None:
        lists: list[TaskListData] = self.task_lists
        for lst in lists:
            if lst.uid == list_uid:
                lst.deleted = True
                break
        tasks: list[TaskData] = [t for t in self.tasks if not t.list_uid == list_uid]
        self.tasks = tasks
        self.task_lists = lists

    def delete_task(self, list_uid: str, uid: str) -> None:
        tasks: list[TaskData] = [
            t for t in self.tasks if t.list_uid != list_uid and t.uid != uid
        ]
        self.tasks = tasks

    def delete_tasks_from_trash(self) -> None:
        tasks: list[TaskData] = self.tasks
        for task in tasks:
            if task.trash and not task.deleted:
                task.deleted = True
        self.tasks = tasks

    def get_lists_as_dicts(self) -> list[TaskListData]:
        return self.task_lists

    def get_prop(self, list_uid: str, uid: str, prop: str) -> Any:
        tasks: list[TaskData] = self.tasks
        for t in tasks:
            if t.list_uid == list_uid and t.uid == uid:
                task = t
                break

        return getattr(task, prop)

    def get_list_prop(self, list_uid: str, prop: str) -> Any:
        list: TaskListData = [lst for lst in self.task_lists if lst.uid == list_uid][0]
        return getattr(list, prop)

    def update_list_prop(self, list_uid: str, prop: str, value: Any) -> None:
        lists: list[TaskListData] = self.task_lists
        for lst in lists:
            if lst.uid == list_uid:
                setattr(lst, prop, value)
                break
        self.task_lists = lists

    def update_list_props(
        self, list_uid: str, props: list[str], values: list[Any]
    ) -> None:
        lists: list[TaskListData] = self.task_lists
        for lst in lists:
            if lst.uid == list_uid:
                for i, prop in enumerate(props):
                    setattr(lst, prop, values[i])
                break
        self.task_lists = lists

    def get_status(self, list_uid: str, parent_uid: str = None) -> tuple[int, int]:
        """Gets tuple (total_tasks, completed_tasks)"""

        tasks: list[TaskData] = [
            t
            for t in self.tasks
            if t.list_uid == list_uid and not t.deleted and not t.trash
        ]
        if parent_uid:
            tasks: list[TaskData] = [t for t in tasks if t.parent == parent_uid]
        total: int = len(tasks)
        completed: int = len([t for t in tasks if t.completed])

        return total, completed

    def add_tag(self, tag: str) -> None:
        new_tags = self.tags
        for t in new_tags:
            if t.text == tag:
                return
        new_tags.append(TagsData(text=tag))
        self.tags = new_tags

    def update_tags(self) -> None:
        tasks_tags_texts: list[str] = []
        for task in self.tasks:
            tasks_tags_texts.extend(task.tags)

        current_tags = self.tags
        current_tags_texts = [t.text for t in current_tags]

        added_tag: bool = False
        for tag in tasks_tags_texts:
            if tag not in current_tags_texts:
                current_tags.append(TagsData(text=tag))
                added_tag = True

        if added_tag:
            self.tags = current_tags

    def remove_tag(self, tag: str) -> None:
        self.tags = [t for t in self.tags if t.text != tag]
        changed = False
        tasks = self.tasks
        for task in tasks:
            if task.tags != [] and tag in task.tags:
                task.tags = [t for t in task.tags if t != tag]
                task.synced = False
                changed = True
        if changed:
            self.tasks = tasks

    def get_parents_uids_tree(cls, list_uid: str, task_uid: str) -> list[str]:
        parents_uids: list[str] = []
        parent: str = cls.get_prop(list_uid, task_uid, "parent")
        while parent != "":
            parents_uids.append(parent)
            parent = cls.get_prop(list_uid, parent, "parent")
        return parents_uids

    def get_task(self, list_uid: str, uid: str) -> TaskData:
        try:
            task: TaskData = [
                t for t in self.tasks if t.list_uid == list_uid and t.uid == uid
            ][0]
            return task
        except Exception as e:
            Log.error(f"Data: can't get task '{uid}'. {e}")
            return TaskData()

    def get_tasks_as_dicts(
        self, list_uid: str = None, parent: str = None
    ) -> list[TaskData]:
        if not list_uid:
            return self.tasks
        elif list_uid and parent is None:
            return [t for t in self.tasks if t.list_uid == list_uid]
        elif list_uid and parent is not None:
            return [
                t for t in self.tasks if t.list_uid == list_uid and t.parent == parent
            ]

    def init(self) -> None:
        Log.debug("Data: Initialize")
        if not os.path.exists(self.__data_dir):
            Log.debug("Data: Create data directory")
            os.mkdir(self.__data_dir)

        self.__convert_data()

        if not os.path.exists(self.__data_file_path):
            Log.debug("Data: Create data.json file")
            self.__write_data()

        self.__read_data()

    def move_task_after(
        self, list_uid: str, task_uid: str, task_after_uid: str
    ) -> None:
        tasks: list[TaskData] = self.tasks

        # Get indexes
        for task in tasks:
            if task.list_uid == list_uid:
                if task.uid == task_uid:
                    task_idx: int = tasks.index(task)
                elif task.uid == task_after_uid:
                    task_after_idx: int = tasks.index(task)

        # Swap items
        if task_idx < task_after_idx:
            i = task_idx
            while i < task_after_idx:
                tasks[i], tasks[i + 1] = tasks[i + 1], tasks[i]
                i += 1
        else:
            i = task_idx
            while task_after_idx + 1 > i:
                tasks[i], tasks[i - 1] = tasks[i - 1], tasks[i]
                i -= 1

        # Save tasks
        self.tasks = tasks

    def move_task_before(
        self, list_uid: str, task_uid: str, task_before_uid: str
    ) -> None:
        tasks: list[TaskData] = self.tasks

        # Get indexes
        for task in tasks:
            if task.list_uid == list_uid:
                if task.uid == task_uid:
                    task_idx: int = tasks.index(task)
                elif task.uid == task_before_uid:
                    task_before_idx: int = tasks.index(task)

        tasks.insert(task_before_idx, tasks.pop(task_idx))

        # Save tasks
        self.tasks = tasks

    def move_task_to_list(
        self, task_uid: str, from_list_uid: str, to_list_uid: str, new_parent: str = ""
    ) -> TaskData:
        tasks: list[TaskData] = self.tasks
        to_delete_tasks: list[TaskData] = [
            self.get_task(from_list_uid, task_uid)
        ] + self.__get_sub_tasks_tree(from_list_uid, task_uid)

        # Move task
        new_main_task: TaskData = TaskData(
            **asdict(self.get_task(from_list_uid, task_uid))
        )
        new_main_task.parent = new_parent
        new_main_task.list_uid = to_list_uid
        new_main_task.synced = False
        tasks.append(new_main_task)

        for task in self.__get_sub_tasks_tree(from_list_uid, task_uid):
            new_task: TaskData = TaskData(**asdict(task))
            new_task.list_uid = to_list_uid
            new_task.synced = False
            tasks.append(new_task)

        for task in tasks:
            if task in to_delete_tasks:
                task.deleted = True
                task.synced = False

        self.tasks = tasks

        return new_main_task

    def update_props(
        self, list_uid: str, uid: str, props: Iterable[str], values: Iterable[Any]
    ):
        tasks = self.tasks
        for task in tasks:
            if task.list_uid == list_uid and task.uid == uid:
                for idx, prop in enumerate(props):
                    setattr(task, prop, values[idx])
                break
        self.tasks = tasks

    # ------ PRIVATE METHODS ------ #

    def __get_sub_tasks(self, list_uid: str, task_uid: str) -> list[TaskData]:
        return [
            t for t in self.tasks if t.list_uid == list_uid and t.parent == task_uid
        ]

    def __get_sub_tasks_tree(self, list_uid: str, task_uid: str) -> list[TaskData]:
        tree: list[TaskData] = []

        def __add_sub_tasks(uid: str):
            sub_tasks: list[TaskData] = self.__get_sub_tasks(list_uid, uid)
            if sub_tasks:
                tree.extend(sub_tasks)
                for sub_task in sub_tasks:
                    __add_sub_tasks(sub_task.uid)

        __add_sub_tasks(task_uid)
        return tree

    def __get_task_parents_tree(self, list_uid: str, task_uid: str) -> list[TaskData]:
        tree: list[TaskData] = []
        tasks = [t for t in self.tasks if t.list_uid == list_uid]

        def __add_parent(task_uid: str):
            for task in tasks:
                if task.parent:
                    tree.append(task)
                    __add_parent(task.parent)

        __add_parent(task_uid)
        return tree

    def __backup_data(self) -> None:
        Log.info("Data: Backup")
        shutil.copyfile(self.__data_file_path, self.__data_file_path + ".old")

    def __convert_data(self):
        old_db_path: str = os.path.join(self.__data_dir, "data.db")
        if not os.path.exists(old_db_path):
            return

        Log.info("Data: Convert old data format to a new one")

        connection = sqlite3.connect(old_db_path)
        cur = connection.cursor()

        # Convert lists
        cur.execute("SELECT * FROM lists")
        lists_data: tuple[tuple] = cur.fetchall()
        for item in lists_data:
            self.__task_lists_data.append(
                TaskListData(deleted=item[0], synced=item[2], name=item[1], uid=item[3])
            )

        # Convert tasks
        cur.execute("SELECT * FROM tasks")
        tasks_data: tuple[tuple] = cur.fetchall()
        for item in tasks_data:
            self.__tasks_data.append(
                TaskData(
                    color=item[0],
                    completed=item[1],
                    deleted=item[2],
                    due_date=item[3],
                    expanded=item[4],
                    list_uid=item[5],
                    notes=item[6],
                    parent=item[7],
                    percent_complete=item[8],
                    priority=item[9],
                    start_date=item[10],
                    synced=item[11],
                    tags=item[12].split(",") if item[12] else [],
                    text=item[13],
                    toolbar_shown=item[14],
                    trash=item[15],
                    uid=item[16],
                )
            )

        os.remove(old_db_path)

        self.__write_data()

    def __read_data(self) -> None:
        try:
            Log.debug("Data: Read data")
            with open(self.__data_file_path, "r") as f:
                data: dict[str, Any] = json.load(f)
                self.__task_lists_data = [TaskListData(**lst) for lst in data["lists"]]
                self.__tasks_data = [TaskData(**t) for t in data["tasks"]]
                self.__tags_data = [TagsData(**t) for t in data["tags"]]
        except Exception as e:
            Log.error(
                f"Data: Can't read data file from disk. {e}. Creating new data file"
            )
            self.__backup_data()
            self.__write_data()

    def __write_data(self) -> None:
        try:
            Log.debug("Data: Write data")
            data: dict[str, list[TaskListData | TaskData]] = {
                "lists": [asdict(lst) for lst in self.task_lists],
                "tags": [asdict(t) for t in self.tags],
                "tasks": [asdict(t) for t in self.tasks],
            }
            w = ThreadSafeWriter(self.__data_file_path, "w")
            w.write(json.dumps(data, ensure_ascii=False))
            w.close()
        except Exception as e:
            Log.error(f"Data: Can't write to disk. {e}.")


class ThreadSafeWriter:
    def __init__(self, path: str, mode: str) -> None:
        self.filewriter = open(path, mode)
        self.queue = Queue()
        self.finished = False
        Thread(name="ThreadSafeWriter", target=self.internal_writer).start()

    def write(self, data):
        self.queue.put(data)

    def internal_writer(self):
        while not self.finished:
            try:
                data = self.queue.get(True, 1)
            except Empty:
                continue
            self.filewriter.write(data)
            self.queue.task_done()

    def close(self):
        self.queue.join()
        self.finished = True
        self.filewriter.close()


# Handle for UserData. For easily changing serialization methods.
UserData = UserDataJSON()
