# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GObject, GtkSource  # type:ignore

from errands.widgets.components.datetime_picker import DateTimePicker
from errands.widgets.sidebar.sidebar import Sidebar
from errands.widgets.tags.tags import Tags
from errands.widgets.tags.tags_sidebar_row import TagsSidebarRow
from errands.widgets.today.today import Today
from errands.widgets.today.today_sidebar_row import TodaySidebarRow
from errands.widgets.trash.trash import Trash
from errands.widgets.trash.trash_sidebar_row import TrashSidebarRow

GObject.type_ensure(Sidebar)
GObject.type_ensure(TodaySidebarRow)
GObject.type_ensure(TagsSidebarRow)
GObject.type_ensure(TrashSidebarRow)
GObject.type_ensure(Tags)
GObject.type_ensure(Today)
GObject.type_ensure(Trash)
GObject.type_ensure(DateTimePicker)
GObject.type_ensure(GtkSource.View)
GObject.type_ensure(GtkSource.Buffer)
