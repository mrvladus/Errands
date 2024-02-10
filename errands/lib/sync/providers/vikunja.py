import uuid

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.providers.caldav import SyncProviderCalDAV
from errands.lib.data import UserData

from pprint import pprint

class SyncProviderVikunja(SyncProviderCalDAV):
    def __init__(self, *args, **kwargs) -> None:
        return super().__init__(name="Vikunja", *args, **kwargs)

    def _create_list_and_tasks(self, calendar_uid, calendar):
        # Vikunja doesnt provide a uuid for lists, so we use their name, since id is the
        # main project.

        UserData.add_list(name=calendar.name, uuid=calendar_uid, synced=True)

        for task in self._get_tasks(calendar):
            print("Creating task")
            task["parent"] = calendar_uid
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

        # for task in calendar.todos(include_completed=True):
        #     from pprint import pprint
        #     pprint(task.icalendar_component)
        #     # VTODO({
                # 'UID': vText('b'3880108b-80c7-42da-9ca1-40bcbc0d1f63''),
                # 'DTSTAMP': vDDDTypes(2024-02-10 19:08:52+00:00, Parameters({})),
                # 'SUMMARY': vText('b'Mirar smartTV 32" vesa mount amazon.es ~ 200eur''),
                # 'DESCRIPTION': vText('b'<p>Test description</p>''),
                # 'DUE': vDDDTypes(2024-02-17 21:00:00+00:00, Parameters({})),
                # 'CREATED': vDDDTypes(2024-02-06 23:20:36+00:00, Parameters({})),
                # 'PRIORITY': 3,
                # 'CATEGORIES': <icalendar.prop.vCategory object at 0x7f8bbffb1a90>,
                # 'LAST-MODIFIED': vDDDTypes(2024-02-10 19:08:52+00:00, Parameters({}))})

    def _update_list_tasks(self, vik_cal_id, vik_cal_obj):
        existing_tasks_by_id = {task["uid"]: task for task in UserData.get_tasks_as_dicts(vik_cal_id)}
        for task in self._get_tasks(vik_cal_obj):
            task["parent"] = vik_cal_id

            if task["uid"] not in existing_tasks_by_id:
                Log.debug("Creating missing task...")
                UserData.add_task(
                    color=task["color"],
                    completed=task["completed"],
                    end_date=task["end_date"],
                    list_uid=vik_cal_id,
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
            else:
                Log.debug("Ensuring task is up to date...")

    def sync(self) -> None:
        Log.info(f"Sync: Sync tasks with remote")

        if not self._update_calendars():
            return

        Log.debug(f"Sync: Vikunja: {self.calendars}")
        vikunja_calendars_by_id = {}

        for calendar in self.calendars:
            calendar_namespace = f"{calendar.id}.{calendar.name}".lower().replace(" ", "_")
            calendar_uid = str(uuid.uuid5(uuid.NAMESPACE_DNS, calendar_namespace))
            vikunja_calendars_by_id[calendar_uid] = calendar

        existing_calendars_by_id = {cal["uid"]: cal for cal in UserData.get_lists_as_dicts()}

        for vik_cal_id, vik_cal_obj in vikunja_calendars_by_id.items():
            Log.debug(f"{vik_cal_id}: {vik_cal_obj}")
            calendar = existing_calendars_by_id.get(vik_cal_id, {})

            if not calendar:
                Log.debug("Creating List...")
                self._create_list_and_tasks(vik_cal_id, vik_cal_obj)
            else:
                Log.debug("Updating List...")
                self._update_list_tasks(vik_cal_id, vik_cal_obj)

        return


        
        # remote_lists_uids = [c.id for c in self.calendars]
        # Log.debug(f"lists_as_dicts:\n{UserData.get_lists_as_dicts()}")
        # for list in UserData.get_lists_as_dicts():
        #     for cal in self.calendars:
        #         # Rename list on remote
        #         if (
        #             cal.id == list["uid"]
        #             and cal.name != list["name"]
        #             and not list["synced"]
        #         ):
        #             Log.debug(f"Sync: Rename remote list '{list['uid']}'")
        #             cal.set_properties([dav.DisplayName(list["name"])])
        #             GLib.idle_add(
        #                 UserData.run_sql,
        #                 f"UPDATE lists SET synced = 1 WHERE uid = '{cal.id}'",
        #             )
        #         # Rename local list
        #         elif (
        #             cal.id == list["uid"]
        #             and cal.name != list["name"]
        #             and list["synced"]
        #         ):
        #             Log.debug(f"Sync: Rename local list '{list['uid']}'")
        #             GLib.idle_add(
        #                 UserData.run_sql,
        #                 f"UPDATE lists SET name = '{cal.name}', synced = 1 WHERE uid = '{cal.id}'",
        #             )

        #     # Delete local list deleted on remote
        #     if (
        #         list["uid"] not in remote_lists_uids
        #         and list["synced"]
        #         and not list["deleted"]
        #     ):
        #         Log.debug(f"Sync: Delete local list deleted on remote '{list['uid']}'")
        #         GLib.idle_add(
        #             UserData.run_sql,
        #             f"""DELETE FROM lists WHERE uid = '{list["uid"]}'""",
        #         )

        #     # Delete remote list deleted locally
        #     elif (
        #         list["uid"] in remote_lists_uids and list["deleted"] and list["synced"]
        #     ):
        #         for cal in self.calendars:
        #             if cal.id == list["uid"]:
        #                 Log.debug(f"Sync: Delete list on remote {cal.id}")
        #                 cal.delete()
        #                 GLib.idle_add(
        #                     UserData.run_sql,
        #                     f"DELETE FROM lists WHERE uid = '{cal.id}'",
        #                 )
        #                 break

        #     # Create new remote list
        #     elif (
        #         list["uid"] not in remote_lists_uids
        #         and not list["synced"]
        #         and not list["deleted"]
        #     ):
        #         Log.debug(f"Sync: Create list on remote {list['uid']}")
        #         self.principal.make_calendar(
        #             cal_id=list["uid"],
        #             supported_calendar_component_set=["VTODO"],
        #             name=list["name"],
        #         )
        #         GLib.idle_add(
        #             UserData.run_sql,
        #             f"""UPDATE lists SET synced = 1
        #             WHERE uid = '{list['uid']}'""",
        #         )

        # if not self._update_calendars():
        #     return

        # user_lists_uids = [i["uid"] for i in UserData.get_lists_as_dicts()]
        # remote_lists_uids = [c.id for c in self.calendars]

        # for calendar in self.calendars:
        #     # Get tasks
        #     local_tasks = UserData.get_tasks_as_dicts(calendar.id)
        #     local_ids = [t["uid"] for t in local_tasks]
        #     remote_tasks = self._get_tasks(calendar)
        #     remote_ids = [task["uid"] for task in remote_tasks]
        #     deleted_uids = [
        #         i[0]
        #         for i in UserData.run_sql(
        #             "SELECT uid FROM tasks WHERE deleted = 1", fetch=True
        #         )
        #     ]

        #     # Add new local lists
        #     if calendar.id not in user_lists_uids:
        #         Log.debug(f"Sync: Copy list from remote {calendar.id}")
        #         UserData.add_list(name=calendar.name, uuid=calendar.id, synced=True)

        #     for task in local_tasks:
        #         # Update local task that was changed on remote
        #         if task["uid"] in remote_ids and task["synced"]:
        #             for remote_task in remote_tasks:
        #                 if remote_task["uid"] == task["uid"]:
        #                     updated: bool = False
        #                     for key in task.keys():
        #                         if (
        #                             key not in "deleted list_uid synced expanded trash"
        #                             and task[key] != remote_task[key]
        #                         ):
        #                             UserData.update_props(
        #                                 calendar.id,
        #                                 task["uid"],
        #                                 [key],
        #                                 [remote_task[key]],
        #                             )
        #                             updated = True
        #                     if updated:
        #                         Log.debug(
        #                             f"Sync: Update local task from remote: {task['uid']}"
        #                         )
        #                     break

        #         # Create new task on remote that was created offline
        #         if task["uid"] not in remote_ids and not task["synced"]:
        #             try:
        #                 Log.debug(f"Sync: Create new task on remote: {task['uid']}")
        #                 new_todo = calendar.save_todo(
        #                     categories=task["tags"] if task["tags"] != "" else None,
        #                     description=task["notes"],
        #                     dtstart=(
        #                         datetime.datetime.fromisoformat(task["start_date"])
        #                         if task["start_date"]
        #                         else None
        #                     ),
        #                     due=(
        #                         datetime.datetime.fromisoformat(task["end_date"])
        #                         if task["end_date"]
        #                         else None
        #                     ),
        #                     priority=task["priority"],
        #                     percent_complete=task["percent_complete"],
        #                     related_to=task["parent"],
        #                     summary=task["text"],
        #                     uid=task["uid"],
        #                     x_errands_color=task["color"],
        #                 )
        #                 if task["completed"]:
        #                     new_todo.complete()
        #                 UserData.update_props(
        #                     calendar.id, task["uid"], ["synced"], [True]
        #                 )
        #             except Exception as e:
        #                 Log.error(
        #                     f"Sync: Can't create new task on remote: {task['uid']}\n{e}"
        #                 )

        #         # Update task on remote that was changed locally
        #         elif task["uid"] in remote_ids and not task["synced"]:
        #             try:
        #                 Log.debug(f"Sync: Update task on remote: {task['uid']}")
        #                 todo = calendar.todo_by_uid(task["uid"])
        #                 todo.uncomplete()
        #                 todo.icalendar_component["summary"] = task["text"]
        #                 if task["end_date"]:
        #                     todo.icalendar_component["due"] = task["end_date"]
        #                 else:
        #                     todo.icalendar_component.pop("DUE", None)
        #                 if task["start_date"]:
        #                     todo.icalendar_component["dtstart"] = task["start_date"]
        #                 else:
        #                     todo.icalendar_component.pop("DTSTART", None)
        #                 todo.icalendar_component["percent-complete"] = task[
        #                     "percent_complete"
        #                 ]
        #                 todo.icalendar_component["description"] = task["notes"]
        #                 todo.icalendar_component["priority"] = task["priority"]
        #                 if task["tags"] != "":
        #                     todo.icalendar_component["categories"] = task["tags"].split(
        #                         ","
        #                     )
        #                 else:
        #                     todo.icalendar_component["categories"] = []
        #                 todo.icalendar_component["related-to"] = task["parent"]
        #                 todo.icalendar_component["x-errands-color"] = task["color"]
        #                 todo.save()
        #                 if task["completed"]:
        #                     todo.complete()
        #                 UserData.update_props(
        #                     calendar.id, task["uid"], ["synced"], [True]
        #                 )
        #             except Exception as e:
        #                 Log.error(
        #                     f"Sync: Can't update task on remote: {task['uid']}. {e}"
        #                 )

        #         # Delete local task that was deleted on remote
        #         elif task["uid"] not in remote_ids and task["synced"]:
        #             Log.debug(
        #                 f"Sync: Delete local task deleted on remote: {task['uid']}"
        #             )
        #             UserData.run_sql(
        #                 f"""DELETE FROM tasks WHERE uid = '{task["uid"]}'"""
        #             )

        #         # Delete tasks on remote if they were deleted locally
        #         elif task["uid"] in remote_ids and task["deleted"]:
        #             try:
        #                 Log.debug(f"Sync: Delete task from remote: '{task['uid']}'")
        #                 if todo := calendar.todo_by_uid(task["uid"]):
        #                     todo.delete()
        #             except Exception as e:
        #                 Log.error(
        #                     f"Sync: Can't delete task from remote: '{task['uid']}'. {e}"
        #                 )

        #     # Create new local task that was created on CalDAV
        #     for task in remote_tasks:
        #         if task["uid"] not in local_ids and task["uid"] not in deleted_uids:
        #             Log.debug(
        #                 f"Sync: Copy new task from remote to list '{calendar.id}': {task['uid']}"
        #             )
        #             UserData.add_task(
        #                 color=task["color"],
        #                 completed=task["completed"],
        #                 end_date=task["end_date"],
        #                 list_uid=calendar.id,
        #                 notes=task["notes"],
        #                 parent=task["parent"],
        #                 percent_complete=task["percent_complete"],
        #                 priority=task["priority"],
        #                 start_date=task["start_date"],
        #                 synced=True,
        #                 tags=task["tags"],
        #                 text=task["text"],
        #                 uid=task["uid"],
        #             )

