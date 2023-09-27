from gi.repository import Adw
from caldav import Calendar, DAVClient, Todo
from .utils import GSettings, Log, TaskUtils, UserData, threaded

# from nextcloud_tasks_api import (
#     NextcloudTasksApi,
#     TaskFile,
#     TaskList,
#     get_nextcloud_tasks_api,
# )
# from nextcloud_tasks_api.ical import Task


class Sync:
    providers: list = []
    window: Adw.ApplicationWindow = None

    @classmethod
    def init(self, window: Adw.ApplicationWindow) -> None:
        self.window = window
        self.providers.append(SyncProviderNextcloud())
        # self.providers.append(SyncProviderTodoist())

    # @threaded
    @classmethod
    def sync(self) -> None:
        if not self.window.can_sync:
            return
        for provider in self.providers:
            if provider.can_sync:
                provider.sync()

    def _setup_providers(self) -> None:
        pass


class SyncProviderNextcloud:
    can_sync: bool = False
    calendar: Calendar = None

    def __init__(self):
        if not GSettings.get("nc-enabled"):
            Log.debug("Nextcloud sync disabled")
            return

        self.url = GSettings.get("nc-url")
        self.username = GSettings.get("nc-username")
        self.password = GSettings.get("nc-password")

        if self.url == "" or self.username == "" or self.password == "":
            Log.error("Not all Nextcloud credentials provided")
            return

        self.url = f"{self.url}/remote.php/dav/"

        with DAVClient(
            url=self.url, username=self.username, password=self.password
        ) as client:
            try:
                principal = client.principal()
                Log.debug(f"Connected to Nextcloud DAV server at '{self.url}'")
                self.can_sync = True
            except:
                Log.error(f"Can't connect to Nextcloud DAV server at '{self.url}'")
                self.can_sync = False
                return

            calendars = principal.calendars()
            errands_cal_exists: bool = False
            for cal in calendars:
                if cal.name == "Errands":
                    self.calendar = cal
                    errands_cal_exists = True
            if not errands_cal_exists:
                Log.debug("Create new calendar 'Errands' on Nextcloud")
                self.calendar = principal.make_calendar(
                    "Errands", supported_calendar_component_set=["VTODO"]
                )

    def get_tasks(self):
        todos = self.calendar.todos(include_completed=True)
        tasks: list[tuple[Todo, dict]] = []
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
            tasks.append((todo, data))

        return tasks

    def sync(self):
        Log.info("Sync tasks with Nextcloud")

        data: dict = UserData.get()
        nc_ids: list[str] = [t[1]["id"] for t in self.get_tasks()]
        # to_delete: list[dict] = []

        for task in data["tasks"]:
            # Create new task on NC that was created offline
            if task["id"] not in nc_ids and not task["synced_nc"]:
                Log.debug(f"Create new task on Nextcloud: {task['id']}")
                new_todo = self.calendar.save_todo(
                    uid=task["id"],
                    summary=task["text"],
                    related_to=task["parent"],
                    x_errands_color=task["color"],
                )
                if task["completed"]:
                    new_todo.complete()
                task["synced_nc"] = True

            # Update task that was changed locally
            elif task["id"] in nc_ids and not task["synced_nc"]:
                Log.debug(f"Update task on Nextcloud: {task['id']}")
                todo = self.calendar.todo_by_uid(task["id"])
                todo.icalendar_component["summary"] = task["text"]
                todo.icalendar_component["related-to"] = task["parent"]
                todo.icalendar_component["x-errands-color"] = task["color"]
                todo.save()
                if task["completed"]:
                    todo.complete()
                else:
                    todo.uncomplete()
                task["synced_nc"] = True

        UserData.set(data)


# class SyncProviderNextcloud:
#     connected: bool = False
#     disabled: bool = False

#     def __init__(self) -> None:
#         if not GSettings.get("nc-enabled"):
#             Log.debug("Nextcloud sync disabled")
#             return

#         Log.debug("Initialize Nextcloud sync provider")

#         self.url = GSettings.get("nc-url")
#         self.username = GSettings.get("nc-username")
#         self.password = GSettings.get("nc-password")

#         if self.url == "" or self.username == "" or self.password == "":
#             Log.error("Not all Nextcloud credentials provided")
#             return

#         self.connect()

#     def connect(self) -> None:
#         Log.info(f"Connecting to Nextcloud at '{self.url}' as user '{self.username}'")
#         self.api: NextcloudTasksApi = get_nextcloud_tasks_api(
#             self.url, self.username, self.password
#         )
#         try:
#             self.errands_task_list: TaskList = None
#             for task_list in self.api.get_lists():
#                 if task_list.name == "Errands":
#                     self.errands_task_list = task_list

#             if not self.errands_task_list:
#                 Log.debug("Creating new list 'Errands'")
#                 self.errands_task_list = self.api.create_list("Errands")

#             self.connected = True
#             Log.info("Connected to Nextcloud")
#         except:
#             Log.error("Can't connect to Nextcloud server")
#             return None

#     def get_tasks(self) -> list[TaskFile] | None:
#         if self.disabled or not self.connected:
#             return

#         try:
#             Log.debug("Getting tasks from Nextcloud")
#             tasks = self.api.get_list(self.errands_task_list)
#             return [task for task in tasks]
#         except:
#             Log.error("Can't connect to Nextcloud server")
#             return None

#     def sync(self) -> None:
#         if self.disabled or not self.connected:
#             return

#         Log.info("Sync tasks with Nextcloud")

#         data: dict = UserData.get()
#         nc_ids: list[str] = [Task(t.content).uid for t in self.get_tasks()]
#         to_delete: list[dict] = []

#         for task in data["tasks"]:
#             # Create new task on NC that was created offline
#             if task["id"] not in nc_ids and not task["synced_nc"]:
#                 new_task = Task()
#                 new_task.summary = task["text"]
#                 new_task.related_to = task["parent"]
#                 if task["completed"]:
#                     new_task.data.upsert_value("STATUS", "COMPLETED")
#                 new_task.data.upsert_value("ERRANDS-COLOR", task["color"])
#                 created_task = self.api.create(
#                     self.errands_task_list, new_task.to_string()
#                 )
#                 task["id"] = Task(created_task.content).uid
#                 task["synced"] = True

#             # Delete local task that was deleted on NC
#             elif task["id"] not in nc_ids and task["synced_nc"]:
#                 to_delete.append(task)

#             # Update task that was changed locally
#             elif task["id"] in nc_ids and not task["synced_nc"]:
#                 updated_task = Task()
#                 updated_task.summary = task["text"]
#                 updated_task.related_to = task["parent"]
#                 if task["completed"]:
#                     updated_task.data.upsert_value("STATUS", "COMPLETED")
#                 updated_task.data.upsert_value("ERRANDS-COLOR", task["color"])
#                 for nc_task in self.get_tasks():
#                     if Task(nc_task.content).uid == task["id"]:
#                         nc_task.content = updated_task.to_string()
#                         self.api.update(nc_task)
#                         break
#                 task["synced_nc"] = True

#             # Update task that was changed on NC
#             elif task["id"] in nc_ids and task["synced_nc"]:
#                 for nc_task in self.get_tasks():
#                     task_obj = Task(nc_task.content)
#                     if task_obj.uid == task["id"]:
#                         task["text"] = task_obj.summary
#                         task["parent"] = task_obj.related_to
#                         task["completed"] = (
#                             task_obj.data.find_value("STATUS") == "COMPLETED"
#                         )
#                         task["color"] = task_obj.data.find_value("ERRANDS-COLOR")
#                         break

#         # Remove deleted tasks from data
#         for task in to_delete:
#             data["tasks"].remove(task)

#         # Create new local task that was created on NC
#         l_ids: list = [t["id"] for t in data["tasks"]]
#         for nc_task in self.get_tasks():
#             task_obj = Task(nc_task.content)
#             if task_obj.uid not in l_ids:
#                 new_task = TaskUtils.new_task(
#                     task_obj.summary,
#                     task_obj.uid,
#                     task_obj.related_to or "",
#                     task_obj.data.find_value("STATUS") == "COMPLETED",
#                     False,
#                     True,
#                 )
#                 data["tasks"].append(new_task)

#         UserData.set(data)


class SyncProviderTodoist:
    token: str

    def __init__(self) -> None:
        pass

    def connect(self) -> None:
        pass

    def sync(self) -> None:
        pass
