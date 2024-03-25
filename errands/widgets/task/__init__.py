from gi.repository import GObject, GtkSource
from errands.widgets.components.datetime_picker import DateTimePicker
from errands.widgets.task.toolbar.toolbar import TaskToolbar

GObject.type_ensure(TaskToolbar)
GObject.type_ensure(DateTimePicker)
GObject.type_ensure(GtkSource.View)
GObject.type_ensure(GtkSource.Buffer)
