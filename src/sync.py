from gi.repository import Adw, GLib
from caldav import Calendar, DAVClient
from .utils import GSettings, Log, TaskUtils, UserData, threaded


class Sync:
    provider = None
    window: Adw.ApplicationWindow = None

    @classmethod
    def init(self, window: Adw.ApplicationWindow = None) -> None:
        if window:
            self.window = window
        match GSettings.get("sync-provider"):
            case 0:
                Log.debug("Sync disabled")
                self.window.sync_btn.set_visible(False)
                return
            case 1:
                self.provider = SyncProviderCalDAV("Nextcloud", self.window)
                self.window.sync_btn.set_visible(True)

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
    def sync_blocking(self, fetch: bool = False):
        """
        Sync tasks while blocking the UI
        """
        if GSettings.get("sync-provider") == 0:
            return
        if not self.provider:
            self.init()
        if self.provider and self.provider.can_sync:
            self.provider.sync(fetch)


class SyncProviderCalDAV:
    can_sync: bool = False
    calendar: Calendar = None
    window = None

    def __init__(self, name: str, window) -> None:
        self.name = name
        self.window = window
        self.url = GSettings.get("sync-url")
        self.username = GSettings.get("sync-username")
        self.password = GSettings.get("sync-password")

        if self.url == "" or self.username == "" or self.password == "":
            Log.error(f"Not all {self.name} credentials provided")
            return

        self._set_url()

        with DAVClient(
            url=self.url, username=self.username, password=self.password
        ) as client:
            try:
                supports_caldav = client.check_cdav_support()
                if not supports_caldav:
                    Log.error(f"Server doesn't support CalDAV. Maybe wrong adress?")
                    return

                principal = client.principal()
                Log.info(f"Connected to {self.name} CalDAV server at '{self.url}'")
                self.can_sync = True
            except:
                Log.error(f"Can't connect to {self.name} CalDAV server at '{self.url}'")
                return

            calendars = principal.calendars()
            errands_cal_exists: bool = False
            for cal in calendars:
                if cal.name == "Errands":
                    self.calendar = cal
                    errands_cal_exists = True
            if not errands_cal_exists:
                Log.debug(f"Create new calendar 'Errands' on {self.name}")
                self.calendar = principal.make_calendar(
                    "Errands", supported_calendar_component_set=["VTODO"]
                )

    def _set_url(self):
        # TODO: check url start
        if self.name == "Nextcloud":
            self.url = f"{self.url}/remote.php/dav/"

    def _get_tasks(self) -> list[dict]:
        try:
            todos = self.calendar.todos(include_completed=True)
            tasks: list[dict] = []
            for todo in todos:
                data: dict = {
                    "id": str(todo.icalendar_component.get("uid", "")),
                    "parent": str(todo.icalendar_component.get("related-to", "")),
                    "text": str(todo.icalendar_component.get("summary", "")),
                    "completed": True
                    if str(todo.icalendar_component.get("status", False)) == "COMPLETED"
                    else False,
                    "color": str(todo.icalendar_component.get("x-errands-color", "")),
                }
                tasks.append(data)
            return tasks
        except:
            return None

    def _fetch(self):
        """
        Update local tasks that was changed on CalDAV
        """
        caldav_tasks: list[dict] | None = self._get_tasks()
        if not caldav_tasks:
            Log.error(f"Can't connect to {self.name}")
            return

        Log.debug(f"Fetch tasks from {self.name}")

        data: dict = UserData.get()
        caldav_ids: list[str] = [task["id"] for task in caldav_tasks]

        to_delete: list[dict] = []
        for task in data["tasks"]:
            # Update local task that was changed on CalDAV
            if task["id"] in caldav_ids and task["synced_caldav"]:
                for caldav_task in caldav_tasks:
                    if caldav_task["id"] == task["id"]:
                        Log.debug(f"Update local task from {self.name}: {task['id']}")
                        task["text"] = caldav_task["text"]
                        task["parent"] = caldav_task["parent"]
                        task["completed"] = caldav_task["completed"]
                        task["color"] = caldav_task["color"]
                        break
            # Delete local task that was deleted on CalDAV
            if task["id"] not in caldav_ids and task["synced_caldav"]:
                Log.debug(f"Delete local task deleted on {self.name}: {task['id']}")
                to_delete.append(task)

        # Remove deleted on CalDAV tasks from data
        for task in to_delete:
            data["tasks"].remove(task)

        # Create new local task that was created on CalDAV
        l_ids: list[str] = [t["id"] for t in data["tasks"]]
        for task in caldav_tasks:
            if task["id"] not in l_ids and task["id"] not in data["deleted"]:
                Log.debug(f"Copy new task from {self.name}: {task['id']}")
                new_task: dict = TaskUtils.new_task(
                    task["text"],
                    task["id"],
                    task["parent"],
                    task["completed"],
                    False,
                    task["color"],
                    True,
                )
                data["tasks"].append(new_task)

        UserData.set(data)

    def sync(self, fetch: bool) -> None:
        """
        Sync local tasks with provider
        """

        caldav_tasks: list[dict] | None = self._get_tasks()
        if not caldav_tasks:
            Log.error(f"Can't connect to {self.name}")
            return

        Log.info(f"Sync tasks with {self.name}")

        data: dict = UserData.get()
        caldav_ids: list[str] = [task["id"] for task in caldav_tasks]

        for task in data["tasks"]:
            # Create new task on CalDAV that was created offline
            if task["id"] not in caldav_ids and not task["synced_caldav"]:
                Log.debug(f"Create new task on {self.name}: {task['id']}")
                new_todo = self.calendar.save_todo(
                    uid=task["id"],
                    summary=task["text"],
                    related_to=task["parent"],
                    x_errands_color=task["color"],
                )
                if task["completed"]:
                    new_todo.complete()
                task["synced_caldav"] = True

            # Update task on CalDAV that was changed locally
            elif task["id"] in caldav_ids and not task["synced_caldav"]:
                Log.debug(f"Update task on {self.name}: {task['id']}")
                todo = self.calendar.todo_by_uid(task["id"])
                todo.uncomplete()
                todo.icalendar_component["summary"] = task["text"]
                todo.icalendar_component["related-to"] = task["parent"]
                todo.icalendar_component["x-errands-color"] = task["color"]
                todo.save()
                if task["completed"]:
                    todo.complete()
                task["synced_caldav"] = True

        # Delete tasks on CalDAV if they were deleted locally
        for task_id in data["deleted"]:
            try:
                Log.debug(f"Delete task from {self.name}: {task_id}")
                todo = self.calendar.todo_by_uid(task_id)
                todo.delete()
            except:
                Log.error(f"Can't delete task from {self.name}: {task_id}")
        data["deleted"] = []

        UserData.set(data)

        if fetch:
            self._fetch()
            GLib.idle_add(self.window.update_ui)
