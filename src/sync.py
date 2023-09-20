from .utils import GSettings, Log
from nextcloud_tasks_api import NextcloudTasksApi, TaskFile, get_nextcloud_tasks_api


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
    url: str
    username: str
    password: str
    api: NextcloudTasksApi
    tasks: list[TaskFile] = []

    def __init__(self) -> None:
        Log.debug("Initialize Nextcloud sync provider")

        self.url = GSettings.get("nc-url")
        self.username = GSettings.get("nc-username")
        self.password = GSettings.get("nc-password")

        # if self.url == "" or self.username == "" or self.password == "":
        #     Log.debug("No Nextcloud credentials provided")
        #     return

        self.connect()

    def connect(self) -> None:
        Log.debug(f"Connecting to Nextcloud at '{self.url}' as user '{self.username}'")
        self.api = get_nextcloud_tasks_api(self.url, self.username, self.password)
        self.get_tasks()
        print(self.tasks)

    def get_tasks(self):
        try:
            for task_list in self.api.get_lists():
                self.tasks.extend(task_list)
        except:
            Log.error("Can't connect to Nextcloud server")

    def sync(self) -> None:
        pass


class SyncProviderTodoist:
    token: str

    def __init__(self) -> None:
        pass

    def connect(self) -> None:
        pass

    def sync(self) -> None:
        pass
