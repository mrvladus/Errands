from gi.repository import Adw
from ..globals import APP_ID, VERSION


def AboutWindow(parent):
    win = Adw.AboutWindow(
        transient_for=parent,
        application_name="List",
        application_icon=APP_ID,
        developer_name="Vlad Krupinski",
        version=VERSION,
        copyright="© 2023 Vlad Krupinski",
        website="https://github.com/mrvladus/List",
    )
    return win
