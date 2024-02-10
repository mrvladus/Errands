import uuid

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.providers.caldav import SyncProviderCalDAV
from errands.lib.data import UserData

from pprint import pprint

class SyncProviderVikunja(SyncProviderCalDAV):
    def __init__(self, *args, **kwargs) -> None:
        return super().__init__(name="Vikunja", *args, **kwargs)

    def _update_calendars(self) -> bool:
        try:
            remote_calendars = []

            for calendar in self.calendars:
                if "VTODO" not in calendar.get_supported_components():
                    continue

                calendar_namespace = f"{calendar.id}.{calendar.name}".lower().replace(" ", "_")
                calendar_uid = str(uuid.uuid5(uuid.NAMESPACE_DNS, calendar_namespace))
                calendar.id = calendar_uid
                for todo in calendar.todos(include_completed=True):
                    todo.icalendar_component["uid"] = calendar_uid

                remote_calendars.append(calendar)

            self.calendars = remote_calendars
            return True
        except Exception as e:
            Log.error(f"Sync: Can't get caldendars from remote")
            Log.error(e)
            return False

