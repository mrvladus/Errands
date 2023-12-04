# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from caldav import Calendar, CalendarObjectResource, DAVClient, Principal, Todo
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
        Log.debug("Sync: Initialize sync provider")
        match GSettings.get("sync-provider"):
            case 0:
                Log.info("Sync disabled")
                # self.window.sync_btn.set_visible(False)
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
        self.init(testing=True)
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
                principal: Principal = client.principal()
                Log.info(f"Sync: Connected to {self.name} server at '{self.url}'")
                self.can_sync = True
                self.calendars = [
                    cal
                    for cal in principal.calendars()
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

    def _get_tasks(self, calendar) -> list[dict]:
        """
        Get todos from calendar and convert them to dict
        """

        try:
            Log.debug(f"Getting tasks from CalDAV")
            todos: list[Todo] = calendar.todos(include_completed=True)
            tasks: list[dict] = []
            for todo in todos:
                data: dict = {
                    "uid": str(todo.icalendar_component.get("uid", "")),
                    "parent": str(todo.icalendar_component.get("related-to", "")),
                    "text": str(todo.icalendar_component.get("summary", "")),
                }
                tasks.append(data)
            return tasks
        except:
            Log.error(f"Sync: Can't get tasks from CalDAV")
            return []

    def _fetch(self):
        """
        Update local tasks that was changed on CalDAV
        """

        Log.debug(f"Sync: Fetch tasks from {self.name}")

        caldav_tasks: list[dict] | None = self._get_tasks()
        if not caldav_tasks:
            Log.debug(f"No tasks on server")
            return

        data: dict = UserData.get()
        caldav_ids: list[str] = [task["id"] for task in caldav_tasks]

        to_delete: list[dict] = []
        for task in data["tasks"]:
            # Update local task that was changed on CalDAV
            if task["id"] in caldav_ids and task["synced_caldav"]:
                for caldav_task in caldav_tasks:
                    if caldav_task["id"] == task["id"]:
                        updated: bool = False
                        for key in ["text", "parent", "completed", "color"]:
                            if task[key] != caldav_task[key]:
                                task[key] = caldav_task[key]
                                updated = True
                        if updated:
                            Log.debug(
                                f"Sync: Update local task from CalDAV: {task['id']}"
                            )
                        break
            # Delete local task that was deleted on CalDAV
            if task["id"] not in caldav_ids and task["synced_caldav"]:
                Log.debug(f"Sync: Delete local task deleted on CalDAV: {task['id']}")
                to_delete.append(task)

        # Remove deleted on CalDAV tasks from data
        for task in to_delete:
            data["tasks"].remove(task)

        # Create new local task that was created on CalDAV
        # l_ids: list[str] = [t["id"] for t in data["tasks"]]
        # for task in caldav_tasks:
        #     if task["id"] not in l_ids and task["id"] not in data["deleted"]:
        #         Log.debug(f"Sync: Copy new task from CalDAV: {task['id']}")
        #         new_task: dict = TaskUtils.new_task(
        #             task["text"],
        #             task["id"],
        #             task["parent"],
        #             task["completed"],
        #             False,
        #             task["color"],
        #             True,
        #         )
        #         data["tasks"].append(new_task)

        UserData.set(data)

    def sync(self, fetch: bool) -> None:
        """
        Sync local tasks with provider
        """
        Log.info(f"Sync: Sync tasks with remote")

        # Get new calendars
        user_lists_uids = [i[0] for i in UserData.get_lists()]
        for calendar in self.calendars:
            # Add new lists
            if calendar.id not in user_lists_uids:
                UserData.add_list(name=calendar.name, uuid=calendar.id)
                # Fetch tasks for the new list
                for task in self._get_tasks(calendar):
                    UserData.add_task(
                        calendar.id, task["text"], task["uid"], task["parent"]
                    )
            # Get tasks
            local_tasks = UserData.get_tasks_as_dicts(calendar.id)
            remote_tasks = self._get_tasks(calendar)
            remote_ids = [task["uid"] for task in remote_tasks]

            for task in local_tasks:
                # Create new task on CalDAV that was created offline
                if task["uid"] not in remote_ids and not task["synced"]:
                    try:
                        Log.debug(f"Sync: Create new task on remote: {task['uid']}")
                        new_todo = calendar.save_todo(
                            uid=task["uid"],
                            summary=task["text"],
                            related_to=task["parent"],
                            # x_errands_color=task["color"],
                        )
                        # if task["completed"]:
                        #     new_todo.complete()
                        UserData.update_prop(calendar.id, task["uid"], "synced", True)
                    except:
                        Log.error(
                            f"Sync: Can't create new task on remote: {task['uid']}"
                        )

                # Update task on CalDAV that was changed locally
                # elif task["uid"] in remote_ids and not task["synced"]:
                #     try:
                #         Log.debug(f"Sync: Update task on remote: {task['uid']}")
                #         todo: CalendarObjectResource = calendar.todo_by_uid(task["uid"])
                #         todo.uncomplete()
                #         todo.icalendar_component["summary"] = task["text"]
                #         todo.icalendar_component["related-to"] = task["parent"]
                #         # todo.icalendar_component["x-errands-color"] = task["color"]
                #         todo.save()
                #         if task["completed"]:
                #             todo.complete()
                #         UserData.update_prop(calendar.id, task["uid"], "synced", True)
                #     except:
                #         Log.error(f"Sync: Can't update task on remote: {task['uid']}")

        # # Delete tasks on CalDAV if they were deleted locally
        # for task_id in data["deleted"]:
        #     try:
        #         Log.debug(f"Sync: Delete task from CalDAV: {task_id}")
        #         todo: CalendarObjectResource = self.calendar.todo_by_uid(task_id)
        #         todo.delete()
        #     except:
        #         Log.error(f"Sync: Can't delete task from CalDAV: {task_id}")
        # data["deleted"] = []

        GLib.idle_add(self.window.lists.update_ui)

        # if fetch:
        #     self._fetch()
        #     GLib.idle_add(self.window.update_ui)
