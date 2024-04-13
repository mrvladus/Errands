# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

# In this file all custom widget templates imported and added to GObject so
# they can be used inside ".ui" template files directly.
# Source: https://developer.gnome.org/documentation/tutorials/widget-templates.html

from gi.repository import GObject  # type:ignore

from errands.widgets.shared.datetime_picker import DateTimePicker
from errands.widgets.sidebar import Sidebar
from errands.widgets.tags.tags import Tags
from errands.widgets.tags.tags_sidebar_row import TagsSidebarRow
from errands.widgets.today.today import Today
from errands.widgets.today.today_sidebar_row import TodaySidebarRow

GObject.type_ensure(DateTimePicker)
GObject.type_ensure(Sidebar)
GObject.type_ensure(TodaySidebarRow)
GObject.type_ensure(TagsSidebarRow)
GObject.type_ensure(Tags)
GObject.type_ensure(Today)
