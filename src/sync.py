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
        Log.info("Sync tasks with Nextcloud")

        def from_user_to_nc():
            # Get local tasks
            data: dict = UserData.get()
            # Get NC tasks
            nc_tasks: list[TaskFile] = self.get_tasks()
            # Get NC tasks ids
            nc_ids: list[str] = [Task(t.content).uid for t in nc_tasks]
            print(nc_ids)
            for task in data["tasks"]:
                # Create new task
                if task["id"] not in nc_ids:
                    new_task = Task()
                    new_task.summary = task["text"]
                    new_task.related_to = task["parent"]
                    if task["completed"]:
                        new_task.data.upsert_value("STATUS", "COMPLETED")
                    created_task = self.api.create(
                        self.errands_task_list, new_task.to_string()
                    )
                    task["id"] = Task(created_task.content).uid
                # Update task data
                # else:
                #     updated_task = Task()
                #     updated_task.summary = task["text"]
                #     updated_task.related_to = task["parent"]
                #     if task["completed"]:
                #         updated_task.data.upsert_value("STATUS", "COMPLETED")
                #     for nc_task in nc_tasks:
                #         if Task(nc_task.content).uid == task["id"]:
                #             nc_task.content = updated_task.to_string()
                #             self.api.update(nc_task)
                #             break
            UserData.set(data)

        def from_nc_to_user():
            # Get local tasks
            data: dict = UserData.get()
            # Get local tasks ids
            l_ids: list[str] = [task["id"] for task in data["tasks"]]
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
                        True
                        if task_obj.data.find_value("STATUS") == "COMPLETED"
                        else False
                    )
                    task_to_upd["parent"] = (
                        task_obj.related_to if task_obj.related_to else ""
                    )
            UserData.set(data)

        # from_nc_to_user()
        from_user_to_nc()


class SyncProviderTodoist:
    token: str

    def __init__(self) -> None:
        pass

    def connect(self) -> None:
        pass

    def sync(self) -> None:
        pass
