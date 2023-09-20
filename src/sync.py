from .utils import GSettings, Log, TaskUtils, UserData
from nextcloud_tasks_api import NextcloudTasksApi, TaskFile, get_nextcloud_tasks_api
from nextcloud_tasks_api.ical import Task

from nextcloud_tasks_api.ical.ical import ICal


class Sync:
    providers: list = []

    @classmethod
    def init(self) -> None:
        self.providers.append(SyncProviderNextcloud())
        # self.providers.append(SyncProviderTodoist())

    @classmethod
    def sync(self) -> None:
        for provider in self.providers:
            provider.sync()

    def _setup_providers(self) -> None:
        pass


class SyncProviderNextcloud:
    def __init__(self) -> None:
        if not GSettings.get("nc-enabled"):
            Log.debug("Nextcloud sync disabled")
            return

        Log.debug("Initialize Nextcloud sync provider")

        self.url = GSettings.get("nc-url")
        self.username = GSettings.get("nc-username")
        self.password = GSettings.get("nc-password")

        if self.url == "" or self.username == "" or self.password == "":
            Log.error("Not all Nextcloud credentials provided")
            return

        self.connect()
        self.get_tasks()

    def connect(self) -> None:
        Log.debug(f"Connecting to Nextcloud at '{self.url}' as user '{self.username}'")
        self.api: NextcloudTasksApi = get_nextcloud_tasks_api(
            self.url, self.username, self.password
        )

    def get_tasks(self) -> list[TaskFile] | None:
        try:
            tasks: list[TaskFile] = []
            for task_list in self.api.get_lists():
                for task in self.api.get_list(task_list):
                    tasks.append(task)
                    # data = Task(task.content).data
                    # print(data.find_value("STATUS"))
                    print(task.content)
            return tasks
        except:
            Log.error("Can't connect to Nextcloud server")
            return None

    def sync(self) -> None:
        # Get local tasks
        data: dict = UserData.get()
        # Get local tasks ids
        l_ids: list = [task["id"] for task in data["tasks"]]
        # Get server tasks
        for task in self.get_tasks():
            task_obj: Task = Task(task.content)
            # Add new task to local tasks
            if task_obj.uid not in l_ids:
                new_task: dict = TaskUtils.new_task(
                    task_obj.summary,
                    task_obj.uid,
                    task_obj.related_to if task_obj.related_to else "",
                    True
                    if task_obj.data.find_value("STATUS") == "COMPLETED"
                    else False,
                )
                data["tasks"].append(new_task)
            # Update existing task
            else:
                # Get local task with same id
                task_to_upd: dict
                for t in data["tasks"]:
                    if t["id"] == task_obj.uid:
                        task_to_upd = t
                        break
                # Sync its values
                task_to_upd["text"] = task_obj.summary
                task_to_upd["completed"] = (
                    True if task_obj.data.find_value("STATUS") == "COMPLETED" else False
                )
                task_to_upd["parent"] = (
                    task_obj.related_to if task_obj.related_to else ""
                )

        UserData.set(data)


class SyncProviderTodoist:
    token: str

    def __init__(self) -> None:
        pass

    def connect(self) -> None:
        pass

    def sync(self) -> None:
        pass
