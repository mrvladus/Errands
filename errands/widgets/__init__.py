# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GObject, GtkSource  # type:ignore

from errands.widgets.tags.tags import Tags
from errands.widgets.components.datetime_picker import DateTimePicker


GObject.type_ensure(Tags)
GObject.type_ensure(DateTimePicker)
GObject.type_ensure(GtkSource.View)
GObject.type_ensure(GtkSource.Buffer)
