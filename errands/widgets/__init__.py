# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GObject  # type:ignore

from errands.widgets.tags.tags import Tags
from errands.widgets.today.today import Today

GObject.type_ensure(Tags)
GObject.type_ensure(Today)
