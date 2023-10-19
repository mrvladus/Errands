# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import uuid
import datetime
from icalendar import Event, Calendar
from errands.utils.data import UserDataTask


def new_task(
    text: str,
    id: str = None,
    parent: str = "",
    completed: bool = False,
    deleted: bool = False,
    color: str = "",
    synced_caldav: bool = False,
) -> UserDataTask:
    task = {
        "id": str(uuid.uuid4()) if not id else id,
        "parent": parent,
        "text": text,
        "color": color,
        "completed": completed,
        "deleted": deleted,
        "synced_caldav": synced_caldav,
    }
    return task


def task_to_ics(task: UserDataTask) -> str:
    cal = Calendar()
    cal.add("version", "2.0")
    cal.add("prodid", "-//Errands")
    todo = Event()
    todo.add("uid", task["id"])
    todo.add("summary", task["text"])
    dt = datetime.datetime.now()
    todo.add("dtstamp", dt)
    todo.add("dtstart", dt)
    todo.add("created", dt)
    todo.add("last-modified", dt)
    cal.add_component(todo)
    return cal.to_ical().decode("utf-8")
