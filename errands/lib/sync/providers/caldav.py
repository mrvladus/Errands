# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from copy import deepcopy
import datetime
import time
from dataclasses import asdict, dataclass, field

import urllib3
from caldav import Calendar, DAVClient, Principal, Todo
from caldav.elements import dav

from errands.lib.data import TaskData, TaskListData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.utils import idle_add
from errands.state import State


@dataclass
class UpdateUIArgs:
    update_trash: bool = False
    update_tags: bool = False
    tasks_to_change_parent: list[TaskData] = field(default_factory=lambda: [])
    tasks_to_change_list: list[tuple[TaskData, TaskData]] = field(
        default_factory=lambda: []
    )
    tasks_to_update: list[TaskData] = field(default_factory=lambda: [])
    tasks_to_purge: list[TaskData] = field(default_factory=lambda: [])
    lists_to_add: list[TaskListData] = field(default_factory=lambda: [])
    lists_to_update_tasks: list[str] = field(default_factory=lambda: [])
    lists_to_update_name: list[str] = field(default_factory=lambda: [])
    lists_to_purge_uids: list[str] = field(default_factory=lambda: [])


class SyncProviderCalDAV:
    can_sync: bool = False
    calendars: list[Calendar] = None
    err: Exception = None

    def __init__(self, testing: bool, name: str = "CalDAV") -> bool:
        Log.info(f"Sync: Initialize '{name}' sync provider")

        self.name: str = name
        self.testing: bool = testing  # Only for connection test

        if not self._check_credentials():
            return

        self._check_url()
        self._connect()

    def _check_credentials(self) -> bool:
        Log.debug("Sync: Checking credentials")

        self.url: str = GSettings.get("sync-url")
        self.username: str = GSettings.get("sync-username")
        self.password: str = GSettings.get_secret(self.name)

        if self.url == "" or self.username == "" or self.password == "":
            Log.error(f"Sync: Not all {self.name} credentials provided")
            if not self.testing:
                State.main_window.add_toast(
                    _("Not all sync credentials provided. Please check settings.")
                )
            return False

        return True

    def _check_url(self) -> None:
        Log.debug("Sync: Checking URL")

        self.url = GSettings.get("sync-url")

        Log.debug(f"Sync: URL is set to {self.url}")

    def _connect(self) -> bool:
        Log.debug("Sync: Attempting connection")
        self.err = None

        urllib3.disable_warnings()

        with DAVClient(
            url=self.url,
            username=self.username,
            password=self.password,
            ssl_verify_cert=False,
        ) as client:
            try:
                self.principal: Principal = client.principal()
                Log.info(f"Sync: Connected to {self.name} server at '{self.url}'")
                self.can_sync = True
                self.calendars = [
                    cal
                    for cal in self.principal.calendars()
                    if "VTODO" in cal.get_supported_components()
                ]
            except Exception as e:
                time.sleep(2)
                self.err = e

                Log.error(
                    f"Sync: Can't connect to {self.name} server at '{self.url}'. {e}"
                )

                if not self.testing:
                    State.main_window.add_toast(
                        _("Can't connect to CalDAV server at:") + " " + self.url
                    )

    def __get_tasks(self, calendar: Calendar) -> list[TaskData]:
        """
        Get todos from calendar and convert them to TaskData list
        """

        # try:
        Log.debug(f"Sync: Getting tasks for list '{calendar.id}'")
        todos: list[Todo] = calendar.todos(include_completed=True)
        tasks: list[TaskData] = []
        for todo in todos:
            task: TaskData = TaskData(
                color=str(todo.icalendar_component.get("x-errands-color", "")),
                completed=(
                    True
                    if todo.icalendar_component.get("status", "") == "COMPLETED"
                    else False
                ),
                expanded=bool(
                    int(todo.icalendar_component.get("x-errands-expanded", "0"))
                ),
                notes=str(todo.icalendar_component.get("description", "")),
                parent=str(todo.icalendar_component.get("related-to", "")),
                percent_complete=int(
                    todo.icalendar_component.get("percent-complete", 0)
                ),
                priority=int(todo.icalendar_component.get("priority", 0)),
                text=str(todo.icalendar_component.get("summary", "")),
                toolbar_shown=bool(
                    int(todo.icalendar_component.get("x-errands-toolbar-shown", "0"))
                ),
                uid=str(todo.icalendar_component.get("uid", "")),
                list_uid=calendar.id,
            )

            # Set tags
            if (tags := todo.icalendar_component.get("categories", "")) != "":
                task.tags = [
                    i.to_ical().decode("utf-8")
                    for i in (tags if isinstance(tags, list) else tags.cats)
                ]
            else:
                task.tags = []

            # Set dates

            if todo.icalendar_component.get("due", "") != "":
                task.due_date = (
                    todo.icalendar_component.get("due", "")
                    .to_ical()
                    .decode("utf-8")
                    .strip("Z")
                )
                if task.due_date and "T" not in task.due_date:
                    task.due_date += "T000000"
            else:
                task.due_date = ""

            if todo.icalendar_component.get("dtstart", "") != "":
                task.start_date = (
                    todo.icalendar_component.get("dtstart", "")
                    .to_ical()
                    .decode("utf-8")
                    .strip("Z")
                )
                if task.start_date and "T" not in task.start_date:
                    task.start_date += "T000000"
            else:
                task.start_date = ""

            if todo.icalendar_component.get("dtstamp", "") != "":
                task.created_at = (
                    todo.icalendar_component.get("dtstamp", "")
                    .to_ical()
                    .decode("utf-8")
                    .strip("Z")
                )
            else:
                task.created_at = ""

            last_modified = todo.icalendar_component.get("LAST-MODIFIED", "")
            if last_modified != "":
                task.changed_at = last_modified.to_ical().decode("utf-8").strip("Z")
            else:
                task.changed_at = ""

            tasks.append(task)
        return tasks
        # except Exception as e:
        #     Log.error(f"Sync: Can't get tasks from remote. {e}")
        #     return []

    def __update_calendars(self) -> bool:
        try:
            self.calendars = [
                cal
                for cal in self.principal.calendars()
                if "VTODO" in cal.get_supported_components()
            ]
            return True
        except Exception as e:
            Log.error(f"Sync: Can't get caldendars from remote. {e}")
            return False

    @idle_add
    def __finish_sync(self) -> None:
        """Update UI thread safe"""

        Log.debug("Sync: Update UI")

        # Remove lists
        for lst in State.get_task_lists():
            if lst.list_uid in self.update_ui_args.lists_to_purge_uids:
                Log.debug(f"Sync: Remove Task List '{lst.list_uid}'")
                lst.purge()

        # Add lists
        for lst in self.update_ui_args.lists_to_add:
            State.sidebar.add_task_list(lst)

        # Rename lists
        for uid in self.update_ui_args.lists_to_update_name:
            task_list = State.get_task_list(uid)
            task_list.update_title()
            task_list.sidebar_row.update_ui(False)

        # Move orphans on top-level
        for task in UserData.clean_orphans():
            if task.list_uid not in self.update_ui_args.lists_to_update_tasks:
                self.update_ui_args.lists_to_update_tasks.append(task.list_uid)

        # Update lists
        for uid in self.update_ui_args.lists_to_update_tasks:
            list_to_upd = State.get_task_list(uid)
            list_to_upd.update_ui()
            for task in list_to_upd.all_tasks:
                task.update_tasks(False)
                task.update_title()
                task.update_progress_bar()

        # Remove tasks
        for task in self.update_ui_args.tasks_to_purge:
            to_remove = State.get_task(task.list_uid, task.uid)
            to_remove.parent.update_ui()
            to_remove.task_list.update_title()
            to_remove.purge()

        # Update tasks
        for task in self.update_ui_args.tasks_to_update:
            try:
                Log.debug(f"Sync: Update task '{task.uid}'")
                State.get_task(task.list_uid, task.uid).update_ui()
            except Exception:
                pass

        # Update tags
        if self.update_ui_args.update_tags:
            UserData.update_tags()
            State.tags_sidebar_row.update_ui()
            State.tags_page.update_ui()

        # Update trash
        if self.update_ui_args.update_trash:
            State.trash_sidebar_row.update_ui()

    def sync(self) -> None:
        Log.info("Sync: Sync tasks with remote")

        if not self.__update_calendars():
            return

        self.update_ui_args: UpdateUIArgs = UpdateUIArgs()
        self.__sync_lists()

        if not self.__update_calendars():
            return

        self.__sync_tasks()

        self.__finish_sync()

    # ----- SYNC LISTS FUNCTIONS ----- #

    def __sync_lists(self):
        self.__add_local_lists()

        remote_lists_uids = [c.id for c in self.calendars]

        for list in UserData.get_lists_as_dicts():
            for cal in self.calendars:
                if cal.id == list.uid and cal.name != list.name and not list.synced:
                    self.__rename_remote_list(cal, list)
                elif cal.id == list.uid and cal.name != list.name and list.synced:
                    self.__rename_local_list(cal, list)
            if list.uid not in remote_lists_uids and list.synced and not list.deleted:
                self.__delete_local_list(list)
            elif list.uid in remote_lists_uids and list.deleted and list.synced:
                self.__delete_remote_list(list)
            elif (
                list.uid not in remote_lists_uids
                and not list.synced
                and not list.deleted
            ):
                self.__create_local_list(list)

    def __add_local_lists(self) -> None:
        user_lists_uids = [lst.uid for lst in UserData.get_lists_as_dicts()]
        for calendar in self.calendars:
            if calendar.id not in user_lists_uids:
                Log.debug(f"Sync: Copy list from remote '{calendar.id}'")
                new_list: TaskListData = UserData.add_list(
                    name=calendar.name, uuid=calendar.id, synced=True
                )
                self.update_ui_args.lists_to_add.append(new_list)

    def __rename_remote_list(self, cal: Calendar, list: TaskListData) -> None:
        Log.debug(f"Sync: Rename remote list '{list.uid}'")

        cal.set_properties([dav.DisplayName(list.name)])
        UserData.update_list_prop(cal.id, "synced", False)

    def __rename_local_list(self, cal: Calendar, list: TaskListData) -> None:
        Log.debug(f"Sync: Rename local list '{list.uid}'")

        UserData.update_list_props(cal.id, ["name", "synced"], [cal.name, True])
        self.update_ui_args.lists_to_update_name.append(cal.id)
        self.update_ui_args.update_trash = True

    def __delete_local_list(self, list: TaskListData) -> None:
        Log.debug(f"Sync: Delete local list deleted on remote '{list.uid}'")

        UserData.delete_list(list.uid)
        self.update_ui_args.lists_to_purge_uids.append(list.uid)
        self.update_ui_args.update_trash = True
        self.update_ui_args.update_tags = True

    def __delete_remote_list(self, list: TaskListData) -> None:
        for cal in self.calendars:
            if cal.id == list.uid:
                Log.debug(f"Sync: Delete list on remote '{cal.id}'")
                cal.delete()
                UserData.delete_list(cal.id)
                return

    def __create_local_list(self, list: TaskListData) -> None:
        Log.debug(f"Sync: Create list on remote {list.uid}")
        try:
            self.principal.make_calendar(
                cal_id=list.uid,
                supported_calendar_component_set=["VTODO"],
                name=list.name,
            )
        except Exception as e:
            Log.error(f"Sync: Can't create local list '{list.uid}'. {e}")
        UserData.update_list_prop(list.uid, "synced", True)

    # ----- SYNC TASKS FUNCTIONS ----- #

    def __sync_tasks(self):
        for calendar in self.calendars:
            # Get tasks
            local_tasks: list[TaskData] = UserData.get_tasks_as_dicts(calendar.id)
            remote_tasks: list[TaskData] = self.__get_tasks(calendar)

            # Create tasks
            local_ids: list[str] = [t.uid for t in local_tasks]
            deleted_uids: list[str] = [
                t.uid for t in UserData.get_tasks_as_dicts() if t.deleted
            ]
            for task in remote_tasks:
                if task.uid not in local_ids and task.uid not in deleted_uids:
                    self.__create_local_task(calendar, task)

            remote_ids: list[str] = [task.uid for task in remote_tasks]
            for task in local_tasks:
                if task.uid not in remote_ids and task.synced:
                    self.__delete_local_task(calendar, task)
                elif task.uid in remote_ids and task.deleted:
                    self.__delete_remote_task(calendar, task)
                elif task.uid not in remote_ids and not task.synced:
                    self.__create_remote_task(calendar, task)
                elif task.uid in remote_ids and not task.synced:
                    self.__update_remote_task(calendar, task)
                elif task.uid in remote_ids and task.synced:
                    self.__update_local_task(calendar, task, remote_tasks)

    def __update_local_task(
        self, calendar: Calendar, task: TaskData, remote_tasks: list[TaskData]
    ):
        remote_task: TaskData = [t for t in remote_tasks if t.uid == task.uid][0]
        remote_task_keys = asdict(remote_task).keys()
        exclude_keys: str = (
            "synced trash expanded toolbar_shown deleted notified created_at"
        )
        updated_props = []
        updated_values = []
        for key in remote_task_keys:
            if key not in exclude_keys and getattr(remote_task, key) != getattr(
                task, key
            ):
                updated_props.append(key)
                updated_values.append(getattr(remote_task, key))

        if not updated_props or (
            updated_props == ["changed_at"] and updated_values == [""]
        ):
            return

        Log.debug(f"Sync: Update local task '{task.uid}'. Updated: {updated_props}")
        old_task: TaskData = deepcopy(task)
        UserData.update_props(calendar.id, task.uid, updated_props, updated_values)

        if "tags" in updated_props:
            self.update_ui_args.update_tags = True
        if "parent" in updated_props:
            if "list_uid" in updated_props:
                if old_task.list_uid not in self.update_ui_args.lists_to_update_tasks:
                    self.update_ui_args.lists_to_update_tasks.append(old_task.list_uid)
            if task.list_uid not in self.update_ui_args.lists_to_update_tasks:
                self.update_ui_args.lists_to_update_tasks.append(task.list_uid)
        else:
            self.update_ui_args.tasks_to_update.append(task)

    def __update_remote_task(self, calendar: Calendar, task: TaskData) -> None:
        Log.debug(f"Sync: Update remote task '{task.uid}'")

        try:
            todo: Todo = calendar.todo_by_uid(task.uid)
            if task.due_date:
                todo.icalendar_component["due"] = task.due_date
            if task.start_date:
                todo.icalendar_component["dtstart"] = task.start_date
            if task.created_at:
                todo.icalendar_component["dtstamp"] = task.created_at
            if task.changed_at:
                todo.icalendar_component["last-modified"] = task.changed_at
            todo.icalendar_component["summary"] = task.text
            todo.icalendar_component["percent-complete"] = int(task.percent_complete)
            todo.icalendar_component["description"] = task.notes
            todo.icalendar_component["priority"] = task.priority
            todo.icalendar_component["categories"] = (
                ",".join(task.tags) if task.tags else []
            )
            todo.icalendar_component["related-to"] = task.parent
            todo.icalendar_component["x-errands-color"] = task.color
            todo.icalendar_component["x-errands-toolbar-shown"] = int(
                task.toolbar_shown
            )
            todo.icalendar_component["x-errands-expanded"] = int(task.expanded)
            todo.complete() if task.completed else todo.uncomplete()
            todo.save()
            UserData.update_props(calendar.id, task.uid, ["synced"], [True])
        except Exception as e:
            Log.error(f"Sync: Can't update task on remote '{task.uid}'. {e}")

    def __create_remote_task(self, calendar: Calendar, task: TaskData) -> None:
        Log.debug(f"Sync: Create remote task '{task.uid}'")

        try:
            new_todo = calendar.save_todo(
                categories=",".join(task.tags) if task.tags else None,
                description=task.notes,
                dtstart=(
                    datetime.datetime.fromisoformat(task.start_date)
                    if task.start_date
                    else None
                ),
                due=(
                    datetime.datetime.fromisoformat(task.due_date)
                    if task.due_date
                    else None
                ),
                priority=task.priority,
                percent_complete=task.percent_complete,
                related_to=task.parent,
                summary=task.text,
                uid=task.uid,
                x_errands_color=task.color,
                x_errands_expanded=int(task.expanded),
                x_errands_toolbar_shown=int(task.toolbar_shown),
            )
            if task.completed:
                new_todo.complete()
            UserData.update_props(calendar.id, task.uid, ["synced"], [True])
        except Exception as e:
            Log.error(f"Sync: Can't create new task on remote: {task.uid}. {e}")

    def __delete_local_task(self, calendar: Calendar, task: TaskData) -> None:
        Log.debug(f"Sync: Delete local task '{task.uid}'")

        UserData.delete_task(calendar.id, task.uid)
        self.update_ui_args.tasks_to_purge.append(task)
        self.update_ui_args.update_trash = True
        self.update_ui_args.update_tags = True

    def __delete_remote_task(self, calendar: Calendar, task: TaskData) -> None:
        Log.debug(f"Sync: Delete remote task '{task.uid}'")

        try:
            if todo := calendar.todo_by_uid(task.uid):
                todo.delete()
        except Exception as e:
            Log.error(f"Sync: Can't delete task from remote: '{task.uid}'. {e}")

    def __create_local_task(self, calendar: Calendar, task: TaskData) -> None:
        Log.debug(
            f"Sync: Copy new task from remote to list '{calendar.id}': {task.uid}"
        )
        UserData.add_task(**asdict(task))
        if task.list_uid not in self.update_ui_args.lists_to_update_tasks:
            self.update_ui_args.lists_to_update_tasks.append(task.list_uid)
