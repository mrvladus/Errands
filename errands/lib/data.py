# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from dataclasses import dataclass, asdict, field
import datetime
import json
import os
import shutil
from typing import Any, Iterable, List
from uuid import uuid4

from gi.repository import GLib  # type:ignore

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.utils import threaded, timeit


@dataclass
class TagsData:
    text: str


@dataclass
class TaskListData:
    color: str = ""
    deleted: bool = False
    name: str = ""
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

        if not self.created_at:
            now: str = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
            self.created_at = now
            self.changed_at = now


class UserDataJSON:

    def __init__(self) -> None:
        self.__data_dir: str = os.path.join(GLib.get_user_data_dir(), "errands")
        self.__data_file_path: str = os.path.join(self.__data_dir, "data.json")
        self.__task_lists_data: list[TaskListData] = []
        self.__tasks_data: list[TaskData] = []
        self.__tags_data: list[TagsData] = []

    # ------ PROPERTIES ------ #

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

        lists = [l for l in self.task_lists if not l.deleted]
        tasks = [t for t in self.tasks if not t.deleted]
        self.task_lists = lists
        self.tasks = tasks

    def delete_list(self, list_uid: str) -> None:
        lists: list[TaskListData] = self.task_lists
        for l in lists:
            if l.uid == list_uid:
                l.deleted = True
                break
        tasks: list[TaskData] = [t for t in self.tasks if not t.list_uid == list_uid]
        self.tasks = tasks
        self.task_lists = lists

    def delete_tasks_from_trash(self) -> None:
        tasks: list[TaskData] = self.tasks
        for task in tasks:
            if task.trash and not task.deleted:
                task.deleted = True
        self.tasks = tasks

    def get_lists_as_dicts(self) -> list[TaskListData]:
        Log.debug(f"Data: Get lists")
        return self.task_lists

    def get_prop(self, list_uid: str, uid: str, prop: str) -> Any:
        tasks: list[TaskData] = self.tasks
        for t in tasks:
            if t.list_uid == list_uid and t.uid == uid:
                task = t
                break

        return getattr(task, prop)

    def get_list_prop(self, list_uid: str, prop: str) -> Any:
        list: TaskListData = [l for l in self.task_lists if l.uid == list_uid][0]
        return getattr(list, prop)

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
        tags_list: list[str] = [t.tags for t in self.tasks if t.tags]
        current_tags = self.tags
        current_tags_texts = [t.text for t in current_tags]

        tags: list[str] = []
        for item in tags_list:
            tags.extend(item.split(","))

        for tag in tags:
            if tag not in current_tags_texts:
                current_tags.append(TagsData(text=tag))

        self.tags = current_tags

    def remove_tags(self, tags: list[str]) -> None:
        Log.debug(f"Data: remove tags {tags}")
        self.tags = [t for t in self.tags if t.text not in tags]
        changed = False
        tasks = self.tasks
        for task in tasks:
            if task.tags != []:
                task.tags = [t for t in task.tags if t not in tags]
                changed = True
        if changed:
            self.tasks = tasks

    def get_task(self, list_uid: str, uid: str) -> TaskData:
        try:
            task: TaskData = [
                t for t in self.tasks if t.list_uid == list_uid and t.uid == uid
            ][0]
            return task
        except Exception as e:
            Log.error(f"Data: can't get task '{uid}'")
            return TaskData()

    def get_tasks_as_dicts(
        self, list_uid: str = None, parent: str = None
    ) -> list[TaskData]:
        if not list_uid:
            return self.tasks
        elif list_uid and parent == None:
            return [t for t in self.tasks if t.list_uid == list_uid]
        elif list_uid and parent != None:
            return [
                t for t in self.tasks if t.list_uid == list_uid and t.parent == parent
            ]

    def init(self) -> None:
        Log.debug("Data: Initialize")
        if not os.path.exists(self.__data_dir):
            Log.debug("Data: Create data directory")
            os.mkdir(self.__data_dir)
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

        # Swap items
        if task_idx < task_before_idx:
            i = task_idx
            while i < task_before_idx - 1:
                tasks[i], tasks[i + 1] = tasks[i + 1], tasks[i]
                i += 1
        else:
            i = task_idx
            while task_before_idx > i:
                tasks[i], tasks[i - 1] = tasks[i - 1], tasks[i]
                i -= 1

        # Save tasks
        self.tasks = tasks

    def move_task_to_list(
        self,
        task_uid: str,
        from_list_uid: str,
        to_list_uid: str,
        synced: bool,
        new_parent: str = "",
    ) -> None:
        pass

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

    def __get_sub_tasks_tree(self, list_uid: str, task_uid: str) -> list[TaskData]:
        tree: list[TaskData] = []
        tasks = [t for t in self.tasks if t.list_uid == list_uid]

        def __add_sub_tasks(parent_uid: str):
            for task in tasks:
                if task.parent == parent_uid:
                    tree.append(task)
                    __add_sub_tasks(task)

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
        shutil.copyfile(self.__data_file_path, self.__data_file_path + ".old")

    def __read_data(self) -> None:
        try:
            Log.debug("Data: Read data")
            with open(self.__data_file_path, "r") as f:
                data: dict[str, Any] = json.load(f)
                self.__task_lists_data = [TaskListData(**l) for l in data["lists"]]
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
            with open(self.__data_file_path, "w") as f:
                data: dict[str, list[TaskListData | TaskData]] = {
                    "lists": [asdict(l) for l in self.task_lists],
                    "tags": [asdict(t) for t in self.tags],
                    "tasks": [asdict(t) for t in self.tasks],
                }
                json.dump(data, f, ensure_ascii=False)
        except Exception as e:
            Log.error(f"Data: Can't write to disk. {e}.")


class UserDataSQLite:

    @classmethod
    def move_task_to_list(
        cls,
        task_uid: str,
        old_list_uid: str,
        new_list_uid: str,
        parent: str,
        synced: bool,
    ) -> None:
        sub_tasks_uids: list[str] = cls.__get_sub_tasks_uids_tree(
            old_list_uid, task_uid
        )
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
    def get_parents_uids_tree(cls, list_uid: str, task_uid: str) -> list[str]:
        parents_uids: list[str] = []
        parent: str = cls.get_prop(list_uid, task_uid, "parent")
        while parent != "":
            parents_uids.append(parent)
            parent = cls.get_prop(list_uid, parent, "parent")
        return parents_uids

    def __convert(cls):
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


# Handle for UserData. For easily changing serialization methods.
UserData = UserDataJSON()
