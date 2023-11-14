# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import datetime
from icalendar import Event, Calendar


def task_to_ics(task) -> str:
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
