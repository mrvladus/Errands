# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

from datetime import date, datetime

from dateutil.rrule import DAILY, MONTHLY, WEEKLY, YEARLY, rrule


# Mapping of preset names to RRULE strings
PRESETS: dict[str, str] = {
    "daily": "FREQ=DAILY;INTERVAL=1",
    "weekly": "FREQ=WEEKLY;INTERVAL=1",
    "biweekly": "FREQ=WEEKLY;INTERVAL=2",
    "monthly": "FREQ=MONTHLY;INTERVAL=1",
    "yearly": "FREQ=YEARLY;INTERVAL=1",
}

_FREQ_MAP: dict[str, int] = {
    "DAILY": DAILY,
    "WEEKLY": WEEKLY,
    "MONTHLY": MONTHLY,
    "YEARLY": YEARLY,
}

_FREQ_STR_MAP: dict[int, str] = {v: k for k, v in _FREQ_MAP.items()}


def build_rrule(freq: str, interval: int) -> str:
    """Build an RRULE string from frequency name and interval.

    Args:
        freq: One of "DAILY", "WEEKLY", "MONTHLY", "YEARLY"
        interval: Positive integer for repeat interval
    """
    return f"FREQ={freq};INTERVAL={interval}"


def parse_rrule_parts(rrule_str: str) -> tuple[str, int]:
    """Extract frequency and interval from an RRULE string.

    Returns:
        Tuple of (frequency_name, interval). Defaults to ("DAILY", 1) on parse failure.
    """
    freq = "DAILY"
    interval = 1
    for part in rrule_str.split(";"):
        if part.startswith("FREQ="):
            freq = part.split("=", 1)[1]
        elif part.startswith("INTERVAL="):
            try:
                interval = int(part.split("=", 1)[1])
            except ValueError:
                interval = 1
    return freq, interval


def get_next_occurrence(rrule_str: str, current_due: str) -> str | None:
    """Compute the next due date given an RRULE and current due date.

    If the next occurrence is still in the past, advances until >= today.

    Args:
        rrule_str: RRULE string (e.g. "FREQ=DAILY;INTERVAL=1")
        current_due: Due date in YYYYMMDD or YYYYMMDDTHHMMSS format

    Returns:
        Next due date in the same format as input, or None if unable to compute.
    """
    if not rrule_str or not current_due:
        return None

    freq_str, interval = parse_rrule_parts(rrule_str)
    freq_int = _FREQ_MAP.get(freq_str)
    if freq_int is None:
        return None

    is_date_only = "T" not in current_due
    try:
        dtstart = datetime.strptime(
            current_due, "%Y%m%d" if is_date_only else "%Y%m%dT%H%M%S"
        )
    except ValueError:
        return None

    rule = rrule(freq=freq_int, interval=interval, dtstart=dtstart)

    # Get the first occurrence strictly after dtstart
    next_dt = rule.after(dtstart)
    if next_dt is None:
        return None

    # If still in the past, keep advancing until >= today
    today = datetime.combine(date.today(), dtstart.time())
    while next_dt < today:
        after_dt = rule.after(next_dt)
        if after_dt is None:
            break
        next_dt = after_dt

    if is_date_only:
        return next_dt.strftime("%Y%m%d")
    return next_dt.strftime("%Y%m%dT%H%M%S")


def get_human_recurrence(rrule_str: str) -> str:
    """Return a human-readable label for an RRULE string.

    Examples: "Repeats daily", "Repeats weekly", "Repeats every 3 days"
    """
    if not rrule_str:
        return ""

    freq_str, interval = parse_rrule_parts(rrule_str)

    freq_labels = {
        "DAILY": (_("daily"), _("days")),
        "WEEKLY": (_("weekly"), _("weeks")),
        "MONTHLY": (_("monthly"), _("months")),
        "YEARLY": (_("yearly"), _("years")),
    }

    labels = freq_labels.get(freq_str)
    if not labels:
        return _("Repeats")

    singular, plural = labels
    if interval == 1:
        return _("Repeats") + " " + singular
    # Translators: e.g. "Repeats every 3 days"
    return _("Repeats every") + f" {interval} " + plural
