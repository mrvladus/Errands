# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import uuid
import datetime
from icalendar import Event, Calendar
from errands.utils.data import UserDataTask


def new_task(
    text: str,
    list_name: str,
    id: str = "",
    parent: str = "",
    completed: bool = False,
    deleted: bool = False,
    color: str = "",
    synced_caldav: bool = False,
    start_date: str = "",
    end_date: str = "",
    notes: str = "",
    percent_complete: int = 0,
    priority: int = 0,
    tags: str = "",
) -> UserDataTask:
    task = {
        "id": id if id else str(uuid.uuid4()),
        "parent": parent,
        "text": text,
        "color": color,
        "completed": completed,
        "deleted": deleted,
        "synced_caldav": synced_caldav,
        "start_date": start_date
        if start_date
        else datetime.datetime.now().strftime("%Y%m%dT%H%M%S"),
        "end_date": end_date
        if end_date
        else format(
            datetime.datetime.now() + datetime.timedelta(days=1), "%Y%m%dT%H%M%S"
        ),
        "notes": notes,
        "percent_complete": percent_complete,
        "priority": priority,
        "tags": tags,
        "list_name": list_name,
    }
    return task


def task_to_ics(task: UserDataTask) -> str:
    cal = Calendar()
    cal.add("version", "2.0")
    cal.add("prodid", "-//Errands")
    todo = Event()
    todo.add("uid", task["id"])
    todo.add("summary", task["text"])
    todo.add("dtstart", datetime.datetime.fromisoformat(task["start_date"]))
    todo.add("dtend", datetime.datetime.fromisoformat(task["end_date"]))
    todo.add("description", task["notes"])
    todo.add("percent-complete", task["percent_complete"])
    todo.add("priority", task["priority"])
    cal.add_component(todo)
    return cal.to_ical().decode("utf-8")
