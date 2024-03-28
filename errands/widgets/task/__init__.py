from gi.repository import GObject, GtkSource
from errands.widgets.components.datetime_picker import DateTimePicker

GObject.type_ensure(DateTimePicker)
GObject.type_ensure(GtkSource.View)
GObject.type_ensure(GtkSource.Buffer)
