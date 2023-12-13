# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
from caldav import Calendar, DAVClient, Principal, Todo
from gi.repository import Adw, GLib

# Import modules
from errands.utils.gsettings import GSettings
from errands.utils.logging import Log
from errands.utils.data import UserData
from errands.utils.functions import threaded


class Sync:
    provider = None
    window: Adw.ApplicationWindow = None

    @classmethod
    def init(self, window, testing: bool = False) -> None:
        self.window = window
        Log.info("Sync: Initialize sync provider")
        match GSettings.get("sync-provider"):
            case 0:
                Log.info("Sync: Sync disabled")
            case 1:
                self.provider = SyncProviderCalDAV("Nextcloud", self.window, testing)
            case 2:
                self.provider = SyncProviderCalDAV("CalDAV", self.window, testing)

    @classmethod
    @threaded
    def sync(self, fetch: bool = False) -> None:
        """
        Sync tasks without blocking the UI
        """
        if GSettings.get("sync-provider") == 0:
            return
        if not self.provider:
            self.init()
        if self.provider and self.provider.can_sync:
            self.provider.sync(fetch)

    @classmethod
    def test_connection(self) -> bool:
        self.init(testing=True, window=self.window)
        return self.provider.can_sync


class SyncProviderCalDAV:
    can_sync: bool = False
    calendars: list[Calendar] = None
    window = None

    def __init__(self, name: str, window: Adw.ApplicationWindow, testing: bool) -> None:
        Log.info(f"Sync: Initialize {name} sync provider")

        self.name: str = name
        self.window: Adw.ApplicationWindow = window
        self.testing: bool = testing  # Only for connection test

        if not self._check_credentials():
            return

        self._check_url()
        return self._connect()

    def _check_credentials(self) -> bool:
        Log.debug("Sync: Checking credentials")

        self.url: str = GSettings.get("sync-url")
        self.username: str = GSettings.get("sync-username")
        self.password: str = GSettings.get_secret(self.name)

        if self.url == "" or self.username == "" or self.password == "":
            Log.error(f"Sync: Not all {self.name} credentials provided")
            if not self.testing:
                self.window.add_toast(
                    _(  # pyright:ignore
                        "Not all sync credentials provided. Please check settings."
                    )
                )
            # self.window.sync_btn.set_visible(False)
            return False

        # self.window.sync_btn.set_visible(True)
        return True

    def _check_url(self) -> None:
        Log.debug("Sync: Checking URL")

        # Add prefix if needed
        if not self.url.startswith("http"):
            self.url = "http://" + self.url
            GSettings.set("sync-url", "s", self.url)
        # For Nextcloud provider
        if self.name == "Nextcloud":
            # Add suffix if needed
            if not "remote.php/dav" in GSettings.get("sync-url"):
                self.url = f"{self.url}/remote.php/dav/"
                GSettings.set("sync-url", "s", self.url)
            else:
                self.url = GSettings.get("sync-url")
        # For other CalDAV providers do not modify url
        if self.name == "CalDAV":
            self.url = GSettings.get("sync-url")

        Log.debug(f"Sync: URL is set to {self.url}")

    def _connect(self) -> bool:
        Log.debug(f"Sync: Attempting connection")

        with DAVClient(
            url=self.url, username=self.username, password=self.password
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
                # self.window.sync_btn.set_visible(True)
            except:
                Log.error(f"Sync: Can't connect to {self.name} server at '{self.url}'")
                if not self.testing:
                    self.window.add_toast(
                        _("Can't connect to CalDAV server at:")  # pyright:ignore
                        + " "
                        + self.url
                    )
                # self.window.sync_btn.set_visible(False)

    def _get_tasks(self, calendar: Calendar) -> list[dict]:
        """
        Get todos from calendar and convert them to dict
        """

        try:
            Log.debug(f"Sync: Getting tasks from remote")
            todos: list[Todo] = calendar.todos(include_completed=True)
            tasks: list[dict] = []
            for todo in todos:
                data: dict = {
                    "color": str(todo.icalendar_component.get("x-errands-color", "")),
                    "completed": str(todo.icalendar_component.get("status", ""))
                    == "COMPLETED",
                    "notes": str(todo.icalendar_component.get("description", "")),
                    "parent": str(todo.icalendar_component.get("related-to", "")),
                    "percent_complete": int(
                        todo.icalendar_component.get("percent-complete", 0)
                    ),
                    "priority": int(todo.icalendar_component.get("priority", 0)),
                    "text": str(todo.icalendar_component.get("summary", "")),
                    "uid": str(todo.icalendar_component.get("uid", "")),
                }
                # Set tags
                # It is super readable, I know. It works, okay?!
                if (tags := todo.icalendar_component.get("categories", "")) != "":
                    data["tags"] = ",".join(
                        [
                            i.to_ical().decode("utf-8")
                            for i in (tags if isinstance(tags, list) else tags.cats)
                        ]
                    )
                else:
                    data["tags"] = ""
                # Set date
                if todo.icalendar_component.get("due", "") != "":
                    data["end_date"] = (
                        todo.icalendar_component.get("due", "")
                        .to_ical()
                        .decode("utf-8")
                        .strip("Z")
                    )
                else:
                    data["end_date"] = ""

                if todo.icalendar_component.get("dtstart", "") != "":
                    data["start_date"] = (
                        todo.icalendar_component.get("dtstart", "")
                        .to_ical()
                        .decode("utf-8")
                        .strip("Z")
                    )
                else:
                    data["start_date"] = ""
                tasks.append(data)

            # Delete orphaned sub-tasks
            ids = [task["uid"] for task in tasks]
            par = [task["parent"] for task in tasks if task["parent"] != ""]
            orph = [t for t in par if t not in ids]
            for o in orph:
                Log.debug(f"Sync: Delete orphaned task: {o}")
                calendar.todo_by_uid(o).delete()

            return [task for task in tasks if task["uid"] not in orph]
        except Exception as e:
            Log.error(f"Sync: Can't get tasks from remote. {e}")
            return []

    def _fetch(self):
        """
        Update local tasks that was changed on remote
        """

        Log.debug("Sync: Fetch tasks from remote")

        for calendar in self.calendars:
            local_tasks = UserData.get_tasks_as_dicts(calendar.id)
            local_ids = UserData.get_tasks(calendar.id)
            remote_tasks = self._get_tasks(calendar)
            remote_ids = [task["uid"] for task in remote_tasks]

            for task in local_tasks:
                # Update local task that was changed on remote
                if task["uid"] in remote_ids and task["synced"]:
                    for remote_task in remote_tasks:
                        if remote_task["uid"] == task["uid"]:
                            updated: bool = False
                            for key in task.keys():
                                if (
                                    key not in "deleted list_uid synced"
                                    and task[key] != remote_task[key]
                                ):
                                    UserData.update_prop(
                                        calendar.id, task["uid"], key, remote_task[key]
                                    )
                                    updated = True
                            if updated:
                                Log.debug(
                                    f"Sync: Update local task from remote: {task['uid']}"
                                )
                            break

                # Delete local task that was deleted on remote
                if task["uid"] not in remote_ids and task["synced"]:
                    Log.debug(
                        f"Sync: Delete local task deleted on remote: {task['uid']}"
                    )
                    UserData.run_sql(
                        f"""DELETE FROM tasks WHERE uid = '{task["uid"]}'"""
                    )

            # Create new local task that was created on CalDAV
            for task in remote_tasks:
                if task["uid"] not in local_ids and task["uid"] not in [
                    i[0]
                    for i in UserData.run_sql("SELECT uid FROM deleted", fetch=True)
                ]:
                    Log.debug(f"Sync: Copy new task from remote: {task['uid']}")
                    UserData.add_task(
                        color=task["color"],
                        completed=task["completed"],
                        end_date=task["end_date"],
                        list_uid=calendar.id,
                        notes=task["notes"],
                        parent=task["parent"],
                        percent_complete=task["percent_complete"],
                        priority=task["priority"],
                        start_date=task["start_date"],
                        synced=True,
                        tags=task["tags"],
                        text=task["text"],
                        uid=task["uid"],
                    )

    def sync(self, fetch: bool) -> None:
        """
        Sync local tasks with provider
        """

        Log.info(f"Sync: Sync tasks with remote")

        # Get new calendars
        self.calendars = [
            cal
            for cal in self.principal.calendars()
            if "VTODO" in cal.get_supported_components()
        ]
        user_lists_uids = [i[0] for i in UserData.get_lists()]

        # Create new remote calendars
        remote_cal_uids = [c.id for c in self.calendars]
        for id in user_lists_uids:
            if id not in remote_cal_uids:
                Log.debug(f"Sync: Create list on remote {id}")
                self.principal.make_calendar(
                    cal_id=id,
                    supported_calendar_component_set=["VTODO"],
                    name=UserData.run_sql(
                        f"SELECT name FROM lists WHERE uid = '{id}'", fetch=True
                    )[0][0],
                )

        for calendar in self.calendars:
            # Add new local lists
            if calendar.id not in user_lists_uids:
                Log.debug(f"Sync: Copy list from remote {calendar.id}")
                UserData.add_list(name=calendar.name, uuid=calendar.id)
                # Fetch tasks for the new list
                for task in self._get_tasks(calendar):
                    UserData.add_task(
                        color=task["color"],
                        completed=task["completed"],
                        end_date=task["end_date"],
                        list_uid=calendar.id,
                        notes=task["notes"],
                        parent=task["parent"],
                        percent_complete=task["percent_complete"],
                        priority=task["priority"],
                        start_date=task["start_date"],
                        synced=True,
                        tags=task["tags"],
                        text=task["text"],
                        uid=task["uid"],
                    )

            # Get tasks
            local_tasks = UserData.get_tasks_as_dicts(calendar.id)
            remote_tasks = self._get_tasks(calendar)
            remote_ids = [task["uid"] for task in remote_tasks]

            for task in local_tasks:
                # Create new task on remote that was created offline
                if task["uid"] not in remote_ids and not task["synced"]:
                    try:
                        Log.debug(f"Sync: Create new task on remote: {task['uid']}")
                        new_todo = calendar.save_todo(
                            categories=task["tags"] if task["tags"] != "" else None,
                            description=task["notes"],
                            dtstart=datetime.datetime.fromisoformat(task["start_date"]),
                            due=datetime.datetime.fromisoformat(task["end_date"]),
                            priority=task["priority"],
                            percent_complete=task["percent_complete"],
                            related_to=task["parent"],
                            summary=task["text"],
                            uid=task["uid"],
                            x_errands_color=task["color"],
                        )
                        if task["completed"]:
                            new_todo.complete()
                        UserData.update_prop(calendar.id, task["uid"], "synced", True)
                    except Exception as e:
                        Log.error(
                            f"Sync: Can't create new task on remote: {task['uid']}\n{e}"
                        )

                # Update task on remote that was changed locally
                elif task["uid"] in remote_ids and not task["synced"]:
                    try:
                        Log.debug(f"Sync: Update task on remote: {task['uid']}")
                        todo = calendar.todo_by_uid(task["uid"])
                        todo.uncomplete()
                        todo.icalendar_component["summary"] = task["text"]
                        todo.icalendar_component["due"] = task["end_date"]
                        todo.icalendar_component["dtstart"] = task["start_date"]
                        todo.icalendar_component["percent-complete"] = task[
                            "percent_complete"
                        ]
                        todo.icalendar_component["description"] = task["notes"]
                        todo.icalendar_component["priority"] = task["priority"]
                        if task["tags"] != "":
                            todo.icalendar_component["categories"] = task["tags"].split(
                                ","
                            )
                        todo.icalendar_component["related-to"] = task["parent"]
                        todo.icalendar_component["x-errands-color"] = task["color"]
                        todo.save()
                        if task["completed"]:
                            todo.complete()
                        UserData.update_prop(calendar.id, task["uid"], "synced", True)
                    except Exception as e:
                        Log.error(
                            f"Sync: Can't update task on remote: {task['uid']}\n{e}"
                        )

            # Delete tasks on remote if they were deleted locally
            for task in UserData.run_sql(f"SELECT uid FROM deleted", fetch=True):
                try:
                    Log.debug(f"Sync: Delete task from remote: {task[0]}")
                    if todo := calendar.todo_by_uid(task[0]):
                        todo.delete()
                except Exception as e:
                    Log.error(f"Sync: Can't delete task from remote: {task[0]}. {e}")
            UserData.run_sql(
                "DELETE FROM deleted",
            )

        if fetch:
            self._fetch()

        GLib.idle_add(self.window.lists.update_ui)
