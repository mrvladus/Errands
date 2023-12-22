# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk, GLib, GObject
from errands.utils.animation import scroll
from errands.utils.gsettings import GSettings
from errands.utils.data import UserData
from errands.utils.functions import get_children
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.widgets.details import Details
from errands.widgets.task import Task


class TaskList(Adw.Bin):
    # State
    scrolling: bool = False  # Is window scrolling

    def __init__(self, window, list_uid: str, parent):
        super().__init__()
        self.window = window
        self.list_uid = list_uid
        self.parent = parent
        self.build_ui()
        self.load_tasks()

    def build_ui(self):
        # Title
        self.title = Adw.WindowTitle(
            title=UserData.run_sql(
                f"SELECT name FROM lists WHERE uid = '{self.list_uid}'",
                fetch=True,
            )[0][0]
        )
        # Toggle sidebar button
        self.toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="sidebar-show-symbolic",
            tooltip_text=_("Toggle Sidebar"),  # type:ignore
        )
        toggle_ctrl = Gtk.ShortcutController(scope=1)
        toggle_ctrl.add_shortcut(
            Gtk.Shortcut(
                trigger=Gtk.ShortcutTrigger.parse_string("F9"),
                action=Gtk.ShortcutAction.parse_string("activate"),
            )
        )
        self.toggle_sidebar_btn.add_controller(toggle_ctrl)
        # Delete completed button
        self.delete_completed_btn = Gtk.Button(
            valign="center",
            icon_name="edit-clear-all-symbolic",
            tooltip_text=_("Delete Completed Tasks"),  # type:ignore
            sensitive=False,
        )
        self.delete_completed_btn.connect(
            "clicked", self.on_delete_completed_btn_clicked
        )
        # Scroll up btn
        self.scroll_up_btn = Gtk.Button(
            valign="center",
            icon_name="go-up-symbolic",
            tooltip_text=_("Scroll Up"),  # type:ignore
            sensitive=False,
        )
        self.scroll_up_btn.connect("clicked", lambda *_: scroll(self.scrl, False))

        # Header Bar
        self.hb = Adw.HeaderBar(title_widget=self.title)
        self.hb.pack_start(self.toggle_sidebar_btn)
        self.hb.pack_start(self.delete_completed_btn)
        self.hb.pack_end(self.scroll_up_btn)

        # ---------- BOTTOMBAR ---------- #

        toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="sidebar-show-symbolic",
            tooltip_text=_("Toggle Sidebar"),  # type:ignore
        )
        toggle_sidebar_btn.bind_property(
            "active",
            self.toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

        delete_completed_btn = Gtk.Button(
            valign="center",
            icon_name="edit-clear-all-symbolic",
            tooltip_text=_("Delete Completed Tasks"),  # type:ignore
            sensitive=False,
        )
        delete_completed_btn.bind_property(
            "sensitive",
            self.delete_completed_btn,
            "sensitive",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        delete_completed_btn.connect("clicked", self.on_delete_completed_btn_clicked)

        scroll_up_btn = Gtk.Button(
            valign="center",
            icon_name="go-up-symbolic",
            tooltip_text=_("Scroll Up"),  # type:ignore
            sensitive=False,
        )
        scroll_up_btn.bind_property(
            "sensitive",
            self.scroll_up_btn,
            "sensitive",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        scroll_up_btn.connect("clicked", lambda *_: scroll(self.scrl, False))

        self.bottom_bar = Gtk.Box(css_classes=["toolbar"])
        self.bottom_bar.append(toggle_sidebar_btn)
        self.bottom_bar.append(delete_completed_btn)
        self.bottom_bar.append(Gtk.Separator(hexpand=True, css_classes=["spacer"]))
        self.bottom_bar.append(scroll_up_btn)

        # ---------- TASKS LIST ---------- #

        # Entry
        entry = Adw.EntryRow(
            activatable=False,
            height_request=60,
            title=_("Add new Task"),  # type:ignore
        )
        entry.connect("entry-activated", self.on_task_added)
        entry_box = Gtk.ListBox(
            selection_mode=0,
            css_classes=["boxed-list"],
            margin_start=12,
            margin_end=12,
            margin_top=12,
            margin_bottom=12,
        )
        entry_box.append(entry)

        # Srolled window
        adj = Gtk.Adjustment()
        adj.connect(
            "value-changed",
            lambda *_: self.scroll_up_btn.set_sensitive(adj.get_value() > 0),
        )
        self.scrl = Gtk.ScrolledWindow(
            propagate_natural_height=True,
            propagate_natural_width=True,
            vadjustment=adj,
        )
        self.dnd_ctrl = Gtk.DropControllerMotion()
        self.dnd_ctrl.connect("motion", self.on_dnd_scroll)
        self.scrl.add_controller(self.dnd_ctrl)

        # Tasks list
        self.tasks_list = Gtk.Box(
            orientation="vertical", hexpand=True, margin_bottom=18
        )
        self.tasks_list.add_css_class("tasks-list")
        self.scrl.set_child(
            Adw.Clamp(maximum_size=850, tightening_threshold=300, child=self.tasks_list)
        )
        # Tasks list box
        box = Gtk.Box(orientation="vertical", vexpand=True)
        box.append(
            Adw.Clamp(
                maximum_size=850,
                tightening_threshold=300,
                child=entry_box,
            )
        )
        box.append(self.scrl)
        # Tasks list toolbar view
        tasks_toolbar_view = Adw.ToolbarView(
            content=box,
            reveal_bottom_bars=False,
        )
        tasks_toolbar_view.add_top_bar(self.hb)
        tasks_toolbar_view.add_bottom_bar(self.bottom_bar)
        # Breakpoint
        tasks_brb = Adw.BreakpointBin(
            width_request=360, height_request=360, child=tasks_toolbar_view
        )
        tasks_brb_bp = Adw.Breakpoint.new(
            Adw.breakpoint_condition_parse("max-width: 400px")
        )
        tasks_brb_bp.add_setter(self.toggle_sidebar_btn, "visible", False)
        tasks_brb_bp.add_setter(self.delete_completed_btn, "visible", False)
        tasks_brb_bp.add_setter(self.scroll_up_btn, "visible", False)
        tasks_brb_bp.add_setter(tasks_toolbar_view, "reveal-bottom-bars", True)
        tasks_brb.add_breakpoint(tasks_brb_bp)

        # Split view
        self.split_view = Adw.OverlaySplitView(
            content=tasks_brb,
            sidebar_position="start",
            min_sidebar_width=360,
            max_sidebar_width=360,
        )
        self.split_view.bind_property(
            "show-sidebar",
            self.toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        GSettings.bind("sidebar-open", self.split_view, "show-sidebar")
        # Details
        self.details = Details(self.window, self)
        self.split_view.set_sidebar(self.details)
        # Breakpoint
        brb = Adw.BreakpointBin(
            width_request=360, height_request=360, child=self.split_view
        )
        bp = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 720px"))
        bp.add_setter(self.split_view, "collapsed", True)
        brb.add_breakpoint(bp)

        self.set_child(brb)

    def add_task(self, uid: str) -> None:
        new_task = Task(uid, self.list_uid, self.window, self, self, False)
        self.tasks_list.append(new_task)
        new_task.toggle_visibility(not new_task.get_prop("trash"))

    def get_all_tasks(self) -> list[Task]:
        """
        Get list of all tasks widgets including sub-tasks
        """

        tasks: list[Task] = []

        def append_tasks(items: list[Task]) -> None:
            for task in items:
                tasks.append(task)
                children: list[Task] = get_children(task.tasks_list)
                if len(children) > 0:
                    append_tasks(children)

        append_tasks(get_children(self.tasks_list))
        return tasks

    def get_toplevel_tasks(self) -> list[Task]:
        return get_children(self.tasks_list)

    def load_tasks(self) -> None:
        Log.debug(f"Loading tasks for '{self.list_uid}'")
        for uid in UserData.get_sub_tasks_uids(self.list_uid, ""):
            self.add_task(uid)
        self.update_status()

    def update_status(self) -> None:
        """
        Update status bar on the top
        """

        n_total: int = UserData.run_sql(
            f"""SELECT COUNT(*) FROM tasks
            WHERE parent = '' 
            AND trash = 0
            AND list_uid = '{self.list_uid}'""",
            fetch=True,
        )[0][0]
        n_completed: int = UserData.run_sql(
            f"""SELECT COUNT(*) FROM tasks 
            WHERE parent = '' 
            AND completed = 1
            AND trash = 0
            AND list_uid = '{self.list_uid}'""",
            fetch=True,
        )[0][0]
        n_all_completed: int = UserData.run_sql(
            f"""SELECT COUNT(*) FROM tasks 
            WHERE completed = 1
            AND trash = 0 
            AND list_uid = '{self.list_uid}'""",
            fetch=True,
        )[0][0]

        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # type:ignore
            if n_total > 0
            else ""
        )
        self.delete_completed_btn.set_sensitive(n_all_completed > 0)

    def update_ui(self) -> None:
        Log.debug(f"Task list {self.list_uid}: Update UI")

        # Rename list
        self.title.set_title(
            UserData.run_sql(
                f"""SELECT name FROM lists 
                WHERE uid = '{self.list_uid}'""",
                fetch=True,
            )[0][0]
        )

        # Remove deleted tasks
        ids = UserData.get_tasks_uids(self.list_uid)
        for task in self.get_all_tasks():
            if task.uid not in ids:
                task.purge()

        # Update existing tasks
        tasks_widgets: list[Task] = self.get_all_tasks()
        for task in tasks_widgets:
            # Update widget title and completed toggle
            if task.completed_btn.get_active() != task.get_prop("completed"):
                task.just_added = True
                task.completed_btn.set_active(task.get_prop("completed"))
                task.just_added = False

            # Change parent
            if isinstance(task.parent, Task) and task.parent.uid != task.get_prop(
                "parent"
            ):
                Log.debug(f"Task list: change parent for {task.uid}")
                task.purge()
                if task.get_prop("parent") == "":
                    self.add_task(task.uid)
                else:
                    for t in tasks_widgets:
                        if t.uid == task.get_prop("parent"):
                            t.add_task(task.uid)
                            break

        # Create new tasks
        for task_dict in UserData.get_tasks_as_dicts(self.list_uid):
            if task_dict["uid"] not in [task.uid for task in self.get_all_tasks()]:
                # Add toplevel task and its sub-tasks
                if task_dict["parent"] == "":
                    self.add_task(task_dict["uid"])
                # Add sub-task and its sub-tasks
                else:
                    for t in self.get_all_tasks():
                        if t.uid == task_dict["parent"]:
                            t.add_task(task_dict["uid"])

        # Update details
        if self.details.parent not in self.get_all_tasks():
            self.details.status.set_visible(True)
        else:
            self.details.update_info(self.details.parent)

    def on_delete_completed_btn_clicked(self, _) -> None:
        """
        Hide completed tasks and move them to trash
        """
        Log.info("Delete completed tasks")

        for task in self.get_all_tasks():
            if task.get_prop("completed") and not task.get_prop("trash"):
                task.delete()
        self.update_status()

    def on_dnd_scroll(self, _motion, _x, y) -> bool:
        """
        Autoscroll while dragging task
        """

        def _auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.dnd_ctrl.contains_pointer():
                return False
            adj = self.scrl.get_vadjustment()
            adj.set_value(adj.get_value() + (2 if scroll_up else -2))
            return True

        MARGIN: int = 50
        if y < MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, _auto_scroll, True)
        elif y > self.scrl.get_allocation().height - MARGIN:
            self.scrolling = True
            GLib.timeout_add(100, _auto_scroll, False)
        else:
            self.scrolling = False

    def on_task_added(self, entry: Gtk.Entry) -> None:
        text: str = entry.props.text
        if text.strip(" \n\t") == "":
            return
        self.add_task(UserData.add_task(list_uid=self.list_uid, text=text))
        entry.props.text = ""
        scroll(self.scrl, True)
        Sync.sync()
