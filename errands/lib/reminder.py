# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations

import re
from datetime import timedelta


# Ordered list of (translatable_label, iCal duration string) tuples.
# The index is used directly for the Gtk.DropDown selection.
PRESETS: list[tuple[str, str]] = [
    (_("None"), ""),
    (_("At due time"), "-PT0M"),
    (_("5 minutes before"), "-PT5M"),
    (_("15 minutes before"), "-PT15M"),
    (_("30 minutes before"), "-PT30M"),
    (_("1 hour before"), "-PT1H"),
    (_("2 hours before"), "-PT2H"),
    (_("1 day before"), "-P1D"),
]

# Regex for parsing simple iCal durations: -PT5M, -PT1H, -P1D, -PT0M
_DURATION_RE = re.compile(r"^-P(?:T(\d+)([MH])|(\d+)D)$")


def get_human_reminder(duration_str: str) -> str:
    """Return a human-readable label for a reminder duration string."""
    if not duration_str:
        return ""
    for label, dur in PRESETS:
        if dur == duration_str:
            return label
    # Fallback for non-preset durations
    td = get_reminder_timedelta(duration_str)
    if td is None:
        return ""
    total_seconds = abs(int(td.total_seconds()))
    if total_seconds == 0:
        return _("At due time")
    minutes = total_seconds // 60
    if minutes < 60:
        return _("{n} min before").format(n=minutes)
    hours = minutes // 60
    if hours < 24:
        return _("{n} hr before").format(n=hours)
    days = hours // 24
    return _("{n} day before").format(n=days)


def get_reminder_timedelta(duration_str: str) -> timedelta | None:
    """Parse an iCal duration string to a negative timedelta.

    Supports: -PT{N}M, -PT{N}H, -P{N}D
    Returns a negative timedelta (or zero for -PT0M), or None if unparseable.
    """
    if not duration_str:
        return None
    m = _DURATION_RE.match(duration_str)
    if not m:
        return None
    if m.group(3) is not None:
        # Days: -P{N}D
        return -timedelta(days=int(m.group(3)))
    n = int(m.group(1))
    unit = m.group(2)
    if unit == "M":
        return -timedelta(minutes=n)
    if unit == "H":
        return -timedelta(hours=n)
    return None


def preset_index_for(duration_str: str) -> int:
    """Return the PRESETS index matching a duration string, or 0 (None) if not found."""
    for i, (_, dur) in enumerate(PRESETS):
        if dur == duration_str:
            return i
    return 0
