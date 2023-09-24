from .utils import GSettings, Log, TaskUtils, UserData
from nextcloud_tasks_api import NextcloudTasksApi, TaskFile, get_nextcloud_tasks_api
from nextcloud_tasks_api.ical import Task


class Sync:
    providers: list = []

    @classmethod
    def init(self) -> None:
        self.providers.append(SyncProviderNextcloud())
        # self.providers.append(SyncProviderTodoist())

    @classmethod
    def sync(self) -> None:
        return
        for provider in self.providers:
            provider.sync()

    def _setup_providers(self) -> None:
        pass


class SyncProviderNextcloud:
    can_sync: bool = False

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

    def connect(self) -> None:
        Log.info(f"Connecting to Nextcloud at '{self.url}' as user '{self.username}'")
        self.api: NextcloudTasksApi = get_nextcloud_tasks_api(
            self.url, self.username, self.password
        )
        try:
            self.errands_task_list = None
            for task_list in self.api.get_lists():
                if task_list.name == "Errands":
                    self.errands_task_list = task_list

            if not self.errands_task_list:
                Log.debug("Creating new list 'Errands'")
                self.errands_task_list = self.api.create_list("Errands")

            Log.info("Connected to Nextcloud")
        except:
            Log.error("Can't connect to Nextcloud server")
            return None

    def get_tasks(self) -> list[TaskFile] | None:
        try:
            return [task for task in self.api.get_list(self.errands_task_list)]
        except:
            Log.error("Can't connect to Nextcloud server")
            return None

    def sync(self) -> None:
        # TODO: can_sync property
        Log.info("Sync tasks with Nextcloud")

        data: dict = UserData.get()
        nc_ids: list[str] = [Task(t.content).uid for t in self.get_tasks()]
        to_delete: list[dict] = []

        for task in data["tasks"]:
            # Create new task on NC that was created offline
            if task["id"] not in nc_ids and not task["synced_nc"]:
                new_task = Task()
                new_task.summary = task["text"]
                new_task.related_to = task["parent"]
                if task["completed"]:
                    new_task.data.upsert_value("STATUS", "COMPLETED")
                new_task.data.upsert_value("ERRANDS-COLOR", task["color"])
                created_task = self.api.create(
                    self.errands_task_list, new_task.to_string()
                )
                task["id"] = Task(created_task.content).uid
                task["synced"] = True

            # Delete local task that was deleted on NC
            elif task["id"] not in nc_ids and task["synced_nc"]:
                to_delete.append(task)

            # Update task that was changed locally
            elif task["id"] in nc_ids and not task["synced_nc"]:
                updated_task = Task()
                updated_task.summary = task["text"]
                updated_task.related_to = task["parent"]
                if task["completed"]:
                    updated_task.data.upsert_value("STATUS", "COMPLETED")
                updated_task.data.upsert_value("ERRANDS-COLOR", task["color"])
                for nc_task in self.get_tasks():
                    if Task(nc_task.content).uid == task["id"]:
                        nc_task.content = updated_task.to_string()
                        self.api.update(nc_task)
                        break
                task["synced_nc"] = True

            # Update task that was changed on NC
            elif task["id"] in nc_ids and task["synced_nc"]:
                for nc_task in self.get_tasks():
                    task_obj = Task(nc_task.content)
                    if task_obj.uid == task["id"]:
                        task["text"] = task_obj.summary
                        task["parent"] = task_obj.related_to
                        task["completed"] = (
                            task_obj.data.find_value("STATUS") == "COMPLETED"
                        )
                        task["color"] = task_obj.data.find_value("ERRANDS-COLOR")
                        break

        # Remove deleted tasks from data
        for task in to_delete:
            data["tasks"].remove(task)

        # Create new local task that was created on NC
        l_ids: list = [t["id"] for t in data["tasks"]]
        for task in nc_task:
            task_obj = Task(nc_task.content)
            if task_obj.uid not in l_ids:
                new_task = TaskUtils.new_task(
                    task_obj.summary,
                    task_obj.uid,
                    task_obj.related_to,
                    task_obj.data.find_value("STATUS") == "COMPLETED",
                )
                data["tasks"].append(new_task)

        UserData.set(data)


class SyncProviderTodoist:
    token: str

    def __init__(self) -> None:
        pass

    def connect(self) -> None:
        pass

    def sync(self) -> None:
        pass
