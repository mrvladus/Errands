import urllib.request
import json
import uuid


class SyncProviderTodoist:
    def __init__(self, api_token):
        self.api_token = api_token
        self.base_url = "https://api.todoist.com/sync/v9/sync"

    def sync(self, commands):
        url = self.base_url
        headers = {
            "Authorization": f"Bearer {self.api_token}",
            "Content-Type": "application/json",
        }
        data = {"token": self.api_token, "commands": json.dumps(commands)}
        req = urllib.request.Request(
            url, headers=headers, data=json.dumps(data).encode("utf-8")
        )
        with urllib.request.urlopen(req) as response:
            response_json = json.loads(response.read().decode("utf-8"))
        return response_json

    def get_projects(self):
        commands = [{"type": "projects/get_data"}]
        response = self.sync(commands)
        projects = response["Projects"]
        return projects

    def create_task(self, project_id, content):
        commands = [
            {
                "type": "item_add",
                "temp_id": self._generate_temp_id(),
                "uuid": self._generate_uuid(),
                "args": {"content": content, "project_id": project_id},
            }
        ]
        response = self.sync(commands)
        task = response["TempIdMapping"][0]
        return task

    def complete_task(self, task_id):
        commands = [
            {
                "type": "item_complete",
                "uuid": self._generate_uuid(),
                "args": {"id": task_id},
            }
        ]
        response = self.sync(commands)
        return response["Items"][0]["id"] == task_id

    def _generate_temp_id(self):
        return f"temp_id_{uuid.uuid4().hex}"

    def _generate_uuid(self):
        return str(uuid.uuid4())
