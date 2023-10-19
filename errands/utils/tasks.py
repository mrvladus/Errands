# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import uuid
from errands.utils.data import UserDataTask


def generate_id() -> str:
    """Generate unique immutable id for task"""
    return str(uuid.uuid4())


def new_task(
    text: str,
    id: str = None,
    pid: str = "",
    cmpd: bool = False,
    dltd: bool = False,
    color: str = "",
    synced_caldav: bool = False,
) -> UserDataTask:
    return {
        "id": generate_id() if not id else id,
        "parent": pid,
        "text": text,
        "color": color,
        "completed": cmpd,
        "deleted": dltd,
        "synced_caldav": synced_caldav,
    }
