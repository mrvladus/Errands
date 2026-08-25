# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from datetime import datetime

from gi.repository import Gio, GLib  # type:ignore

from errands.lib.data import TaskData, UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.reminder import get_reminder_timedelta
from errands.state import State


class ErrandsNotificationsDaemon:
    CHECK_INTERVAL_SEC: int = 20  # Check tasks every _ seconds

    def __init__(self) -> None:
        State.notifications_daemon = self
        self.start()

    # ------ PROPERTIES ------ #

    @property
    def due_tasks(self) -> list[TaskData]:
        """Get due tasks that haven't been notified yet"""

        now: datetime = datetime.now()
        tasks: list[TaskData] = [
            t
            for t in UserData.tasks
            if t.due_date
            and datetime.fromisoformat(t.due_date) < now
            and not t.deleted
            and not t.completed
            and not t.trash
            and not t.notified
        ]

        return tasks

    @property
    def reminder_tasks(self) -> list[TaskData]:
        """Get tasks whose reminder time has passed but haven't been notified yet"""

        now: datetime = datetime.now()
        tasks: list[TaskData] = []
        for t in UserData.tasks:
            if (
                t.due_date
                and t.reminder
                and not t.reminder_notified
                and not t.deleted
                and not t.completed
                and not t.trash
            ):
                due = datetime.fromisoformat(t.due_date)
                offset = get_reminder_timedelta(t.reminder)
                if offset is not None:
                    trigger_time = due + offset  # offset is negative timedelta
                    if trigger_time <= now:
                        tasks.append(t)
        return tasks

    # ------ PUBLIC METHODS ------ #

    def send(self, id: str, notification: Gio.Notification) -> None:
        """Send desktop Notification"""

        State.application.send_notification(id, notification)

    def start(self) -> None:
        """Start notifications daemon"""

        GLib.timeout_add_seconds(self.CHECK_INTERVAL_SEC, self.__check_data)

    # ------ PRIVATE METHODS ------ #

    def __check_data(self) -> bool:
        """Get due tasks and send notifications"""

        # If notifications is disabled - do nothing
        if not GSettings.get("notifications-enabled"):
            return True

        Log.debug("Notifications: Check")

        for task in self.reminder_tasks:
            self.__send_reminder_notification(task)
            UserData.update_props(task.list_uid, task.uid, ["reminder_notified"], [True])

        for task in self.due_tasks:
            self.__send_due_notification(task)
            UserData.update_props(task.list_uid, task.uid, ["notified"], [True])

        return True

    def __send_reminder_notification(self, task: TaskData) -> None:
        Log.debug(f"Notifications: Send reminder: {task.uid}")

        notification = Gio.Notification()
        notification.set_title(_("Reminder"))
        notification.set_body(task.text)
        self.send(task.uid, notification)

    def __send_due_notification(self, task: TaskData) -> None:
        Log.debug(f"Notifications: Send: {task.uid}")

        notification = Gio.Notification()
        notification.set_title(_("Task is Due"))
        notification.set_body(task.text)
        self.send(task.uid, notification)
