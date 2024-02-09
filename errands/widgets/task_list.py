# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.sync.sync import Sync
from errands.widgets.components import Box
from errands.widgets.details import Details
from gi.repository import Adw, Gtk, GLib, GObject  # type:ignore
from errands.lib.animation import scroll
from errands.lib.data import UserData
from errands.lib.functions import get_children
from errands.lib.logging import Log
from errands.widgets.task import Task
from errands.lib.gsettings import GSettings


class TaskList(Adw.Bin):
    # State
    scrolling: bool = False  # Is window scrolling

    def __init__(self, window: Window, list_uid: str, parent):
        super().__init__()
        self.window = window
        self.list_uid = list_uid
        self.parent = parent
        self._build_ui()
        self.update_ui()

    def _build_ui(self) -> None:
        # Title
        self.title = Adw.WindowTitle(
            title=UserData.run_sql(
                f"SELECT name FROM lists WHERE uid = '{self.list_uid}'",
                fetch=True,
            )[0][0]
        )

        # Toggle sidebar button
        self.left_toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="errands-sidebar-left-symbolic",
            tooltip_text=_("Toggle Sidebar"),
        )
        toggle_ctrl = Gtk.ShortcutController(scope=1)
        toggle_ctrl.add_shortcut(
            Gtk.Shortcut(
                trigger=Gtk.ShortcutTrigger.parse_string("F9"),
                action=Gtk.ShortcutAction.parse_string("activate"),
            )
        )
        self.left_toggle_sidebar_btn.add_controller(toggle_ctrl)
        self.left_toggle_sidebar_bin = Adw.Bin(child=self.left_toggle_sidebar_btn)
        GSettings.bind("right-sidebar", self.left_toggle_sidebar_btn, "visible", True)

        self.right_toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="errands-sidebar-right-symbolic",
            tooltip_text=_("Toggle Sidebar"),
        )
        self.right_toggle_sidebar_btn.bind_property(
            "active",
            self.left_toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        self.right_toggle_sidebar_bin = Adw.Bin(child=self.right_toggle_sidebar_btn)
        GSettings.bind("right-sidebar", self.right_toggle_sidebar_btn, "visible")

        # Delete completed button
        self.delete_completed_btn = Gtk.Button(
            valign="center",
            icon_name="edit-clear-all-symbolic",
            tooltip_text=_("Delete Completed Tasks"),
            sensitive=False,
        )
        self.delete_completed_btn.connect(
            "clicked", self._on_delete_completed_btn_clicked
        )
        # Scroll up btn
        self.scroll_up_btn = Gtk.Button(
            valign="center",
            icon_name="go-up-symbolic",
            tooltip_text=_("Scroll Up"),
            sensitive=False,
        )
        self.scroll_up_btn.connect("clicked", lambda *_: scroll(self.scrl, False))

        # Header Bar
        hb = Adw.HeaderBar(title_widget=self.title)
        hb.pack_start(self.left_toggle_sidebar_bin)
        hb.pack_start(self.delete_completed_btn)
        hb.pack_end(self.right_toggle_sidebar_bin)
        hb.pack_end(self.scroll_up_btn)

        # ---------- BOTTOMBAR ---------- #

        left_toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="errands-sidebar-left-symbolic",
            tooltip_text=_("Toggle Sidebar"),
        )
        left_toggle_sidebar_btn.bind_property(
            "active",
            self.left_toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        GSettings.bind("right-sidebar", left_toggle_sidebar_btn, "visible", True)
        right_toggle_sidebar_btn = Gtk.ToggleButton(
            icon_name="errands-sidebar-right-symbolic",
            tooltip_text=_("Toggle Sidebar"),
        )
        right_toggle_sidebar_btn.bind_property(
            "active",
            self.left_toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        GSettings.bind("right-sidebar", right_toggle_sidebar_btn, "visible")

        delete_completed_btn = Gtk.Button(
            valign="center",
            icon_name="edit-clear-all-symbolic",
            tooltip_text=_("Delete Completed Tasks"),
            sensitive=False,
        )
        delete_completed_btn.bind_property(
            "sensitive",
            self.delete_completed_btn,
            "sensitive",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        delete_completed_btn.connect("clicked", self._on_delete_completed_btn_clicked)

        scroll_up_btn = Gtk.Button(
            valign="center",
            icon_name="go-up-symbolic",
            tooltip_text=_("Scroll Up"),
            sensitive=False,
        )
        scroll_up_btn.bind_property(
            "sensitive",
            self.scroll_up_btn,
            "sensitive",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        scroll_up_btn.connect("clicked", lambda *_: scroll(self.scrl, False))

        # ---------- TASKS LIST ---------- #

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
        self.dnd_ctrl.connect("motion", self._on_dnd_scroll)
        self.scrl.add_controller(self.dnd_ctrl)

        # Tasks list
        self.tasks_list = Gtk.Box(orientation="vertical", hexpand=True, vexpand=True)
        self.tasks_list.add_css_class("tasks-list")
        self.scrl.set_child(
            Adw.Clamp(
                maximum_size=1000,
                tightening_threshold=300,
                child=self.tasks_list,
            )
        )

        # Content box
        content_box = Box(
            children=[TaskListEntry(self), self.scrl],
            orientation="vertical",
            vexpand=True,
        )
        content_box_click_ctrl = Gtk.GestureClick()
        content_box_click_ctrl.connect("released", self._on_empty_area_clicked)
        content_box.add_controller(content_box_click_ctrl)

        # Split View
        self.split_view = Adw.OverlaySplitView(
            min_sidebar_width=360,
            max_sidebar_width=400,
            sidebar_width_fraction=0.40,
            sidebar_position=int(GSettings.get("right-sidebar")),
        )
        self.split_view.bind_property(
            "show-sidebar",
            self.left_toggle_sidebar_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        GSettings.bind("sidebar-open", self.split_view, "show-sidebar")

        # Details
        self.details = Details(self)
        self.split_view.set_sidebar(self.details)

        # Tasks list toolbar view
        self.tasks_toolbar_view = Adw.ToolbarView(
            content=content_box,
            reveal_bottom_bars=False,
        )
        self.tasks_toolbar_view.add_top_bar(hb)
        self.tasks_toolbar_view.add_bottom_bar(
            Box(
                children=[
                    left_toggle_sidebar_btn,
                    delete_completed_btn,
                    Gtk.Separator(hexpand=True, css_classes=["spacer"]),
                    scroll_up_btn,
                    right_toggle_sidebar_btn,
                ],
                css_classes=["toolbar"],
            )
        )
        self.split_view.set_content(self.tasks_toolbar_view)

        # Breakpoints
        tasks_brb = Adw.BreakpointBin(
            width_request=360, height_request=360, child=self.split_view
        )
        bp = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 720px"))
        bp.add_setter(self.split_view, "collapsed", True)
        tasks_brb.add_breakpoint(bp)
        bp1 = Adw.Breakpoint.new(Adw.breakpoint_condition_parse("max-width: 370px"))
        bp1.add_setter(self.split_view, "collapsed", True)
        bp1.add_setter(self.left_toggle_sidebar_bin, "visible", False)
        bp1.add_setter(self.right_toggle_sidebar_bin, "visible", False)
        bp1.add_setter(self.delete_completed_btn, "visible", False)
        bp1.add_setter(self.scroll_up_btn, "visible", False)
        bp1.add_setter(self.tasks_toolbar_view, "reveal-bottom-bars", True)
        tasks_brb.add_breakpoint(bp1)

        self.set_child(tasks_brb)

    def add_task(self, uid: str) -> None:
        new_task = Task(uid, self, self, False)
        self.tasks_list.append(new_task)
        new_task.toggle_visibility(not new_task.get_prop("trash"))

    def get_all_tasks(self) -> list[Task]:
        """
        Get list of all tasks widgets including sub-tasks
        """

        tasks: list[Task] = []

        for task in self.get_toplevel_tasks():
            tasks.append(task)
            tasks.extend(task.tasks_list.get_all_sub_tasks())

        return tasks

    def get_toplevel_tasks(self) -> list[Task]:
        return get_children(self.tasks_list)

    def update_status(self) -> None:
        """
        Update status bar on the top
        """

        tasks: list[Task] = self.get_toplevel_tasks()
        n_total: int = len([t for t in tasks if t.get_reveal_child()])
        n_completed: int = len(
            [
                t
                for t in tasks
                if t.task_row.complete_btn.get_active() and t.get_reveal_child()
            ]
        )

        self.title.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}" if n_total > 0 else ""
        )
        self.delete_completed_btn.set_sensitive(
            len(
                [
                    t
                    for t in self.get_all_tasks()
                    if t.task_row.complete_btn.get_active() and t.get_reveal_child()
                ]
            )
            > 0
        )

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
        all_tasks: list[Task] = self.get_all_tasks()
        ids: list[str] = UserData.get_tasks_uids(self.list_uid)
        for task in all_tasks:
            if task.uid not in ids:
                task.purge()
                all_tasks.remove(task)

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
                            t.tasks_list.add_sub_task(task_dict["uid"])

        # Update details
        if (
            self.details.parent
            and self.details.parent.uid in UserData.get_tasks_uids(self.list_uid)
            and not self.details.parent.get_prop("trash")
        ):
            self.details.update_info(self.details.parent)
        else:
            self.details.update_info(None)

        for task in self.get_toplevel_tasks():
            task.update_ui()

        self.update_status()

    def _on_delete_completed_btn_clicked(self, _) -> None:
        """
        Hide completed tasks and move them to trash
        """
        Log.info("Delete completed tasks")
        for task in self.get_all_tasks():
            if not task.get_prop("trash") and task.get_prop("completed"):
                task.delete()
        self.update_ui()

    def _on_dnd_scroll(self, _motion, _x, y: float) -> bool:
        """
        Autoscroll while dragging task
        """

        def _auto_scroll(scroll_up: bool) -> bool:
            """Scroll while drag is near the edge"""
            if not self.scrolling or not self.dnd_ctrl.contains_pointer():
                return False
            adj: Gtk.Adjustment = self.scrl.get_vadjustment()
            adj.set_value(adj.get_value() - (2 if scroll_up else -2))
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

    def _on_empty_area_clicked(self, _gesture, _n, x: float, y: float) -> None:
        """Close Details panel on click on empty space"""

        height = 0
        for task in self.get_toplevel_tasks():
            if task.get_child_revealed():
                height = (
                    task.compute_bounds(self.tasks_toolbar_view.get_content())
                    .out_bounds.get_bottom_right()
                    .y
                )
        left_area_end: int = self.tasks_list.get_allocation().x
        right_area_start: int = left_area_end + self.tasks_list.get_width()

        on_sides: bool = x < left_area_end or x > right_area_start
        on_bottom: bool = y > height
        if on_sides or on_bottom:
            self.split_view.set_show_sidebar(False)


class TaskListEntry(Adw.Bin):

    # Public elements
    entry: Adw.EntryRow

    def __init__(self, task_list: TaskList) -> None:
        super().__init__()
        self.task_list = task_list
        self._build_ui()

    def _build_ui(self) -> None:
        self.entry = Adw.EntryRow(
            height_request=60,
            title=_("Add new Task"),
            css_classes=["card"],
            margin_top=6,
            margin_bottom=6,
            margin_end=12,
            margin_start=12,
        )
        self.entry.connect("entry-activated", self._on_task_added)

        self.set_child(
            Adw.Clamp(
                maximum_size=1000,
                tightening_threshold=300,
                child=self.entry,
            )
        )

    def _on_task_added(self, entry: Adw.EntryRow | Gtk.Entry) -> None:
        text: str = entry.get_text()
        if text.strip(" \n\t") == "":
            return
        self.task_list.add_task(
            UserData.add_task(list_uid=self.task_list.list_uid, text=text)
        )
        entry.set_text("")
        scroll(self.task_list.scrl, True)
        Sync.sync()
