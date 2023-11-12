# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Self
from gi.repository import Gtk, Adw, Gdk, GObject

# Import modules
import errands.utils.tasks as TaskUtils
from errands.utils.sync import Sync
from errands.utils.logging import Log
from errands.utils.data import UserData, UserDataTask
from errands.utils.markup import Markup
from errands.utils.functions import get_children


class Task(Gtk.Revealer):
    # - State - #
    just_added: bool = True
    is_sub_task: bool = False
    can_sync: bool = True

    def __init__(
        self,
        uid: str,
        list_name: str,
        window: Adw.ApplicationWindow,
        tasks_panel,
        parent=None,
    ) -> None:
        super().__init__()
        Log.info(f"Add task: {uid}")

        self.uid = uid
        self.list_name = list_name
        self.window = window
        self.tasks_panel = tasks_panel
        self.parent = tasks_panel if not parent else parent

        self.build_ui()
        # Add to trash if needed
        # if UserData.get_task_prop(self.list_name, self.uid, "deleted"):
        #     self.tasks_panel.trash_panel.trash_add(self.uid)
        # self.check_is_sub()
        # self.add_sub_tasks()
        self.just_added = False
        # self.parent.update_status()

    def get_prop(self, prop: str):
        res = UserData.get_prop(self.list_name, self.uid, prop)
        if prop in "deleted completed":
            res = bool(res)
        return res

    def build_ui(self):
        # Top drop area
        top_drop_img = Gtk.Image(
            icon_name="list-add-symbolic",
            hexpand=True,
            css_classes=["dim-label", "task-drop-area"],
        )
        top_drop_img_target = Gtk.DropTarget.new(actions=Gdk.DragAction.MOVE, type=Task)
        top_drop_img_target.connect("drop", self.on_task_top_drop)
        top_drop_img.add_controller(top_drop_img_target)
        top_drop_area = Gtk.Revealer(child=top_drop_img, transition_type=5)
        # Drop controller
        drop_ctrl = Gtk.DropControllerMotion.new()
        drop_ctrl.bind_property(
            "contains-pointer",
            top_drop_area,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )
        self.add_controller(drop_ctrl)
        # Task row
        self.task_row = Adw.ActionRow(
            height_request=60, use_markup=True, css_classes=["task-title"]
        )
        self.task_row.set_title(Markup.find_url(Markup.escape(self.get_prop("text"))))
        # Task row controllers
        task_row_drag_source = Gtk.DragSource.new()
        task_row_drag_source.set_actions(Gdk.DragAction.MOVE)
        task_row_drag_source.connect("prepare", self.on_drag_prepare)
        task_row_drag_source.connect("drag-begin", self.on_drag_begin)
        task_row_drag_source.connect("drag-cancel", self.on_drag_end)
        task_row_drag_source.connect("drag-end", self.on_drag_end)
        self.task_row.add_controller(task_row_drag_source)
        task_row_drop_target = Gtk.DropTarget.new(
            actions=Gdk.DragAction.MOVE, type=Task
        )
        task_row_drop_target.connect("drop", self.on_drop)
        self.task_row.add_controller(task_row_drop_target)
        task_row_click_ctrl = Gtk.GestureClick.new()
        task_row_click_ctrl.connect("released", self.on_expand)
        self.task_row.add_controller(task_row_click_ctrl)
        # Mark as completed button
        self.completed_btn = Gtk.CheckButton(
            valign="center",
            tooltip_text=_("Mark as Completed"),  # type:ignore
            active=self.get_prop("completed"),
        )
        self.completed_btn.connect("toggled", self.on_completed_btn_toggled)
        self.task_row.add_prefix(self.completed_btn)
        # Expand icon
        self.expand_icon = Gtk.Image(icon_name="go-down-symbolic", css_classes=["fade"])
        expand_icon_rev = Gtk.Revealer(
            transition_type=1, margin_end=5, child=self.expand_icon
        )
        task_row_hover_ctrl = Gtk.EventControllerMotion.new()
        task_row_hover_ctrl.bind_property(
            "contains-pointer",
            expand_icon_rev,
            "reveal-child",
            GObject.BindingFlags.SYNC_CREATE,
        )
        self.task_row.add_controller(task_row_hover_ctrl)
        # Details button
        details_btn = Gtk.Button(
            icon_name="view-more-symbolic",
            valign="center",
            tooltip_text=_("Details"),  # type:ignore
            css_classes=["flat"],
        )
        details_btn.connect("clicked", self.on_details_btn_clicked)
        # Task row suffix box
        task_row_suffix_box = Gtk.Box()
        task_row_suffix_box.append(expand_icon_rev)
        task_row_suffix_box.append(details_btn)
        self.task_row.add_suffix(task_row_suffix_box)
        # Sub-tasks entry
        sub_tasks_entry = Gtk.Entry(
            hexpand=True,
            margin_bottom=6,
            margin_start=12,
            margin_end=12,
            placeholder_text=_("Add new Sub-Task"),  # type:ignore
        )
        sub_tasks_entry.connect("activate", self.on_sub_task_added)
        # Sub-tasks
        self.tasks_list = Gtk.Box(orientation="vertical", css_classes=["sub-tasks"])
        # Sub-tasks box
        sub_tasks_box = Gtk.Box(orientation="vertical")
        sub_tasks_box.append(sub_tasks_entry)
        sub_tasks_box.append(self.tasks_list)
        # Sub-tasks box revealer
        self.sub_tasks_revealer = Gtk.Revealer(child=sub_tasks_box)
        # Task card
        self.main_box = Gtk.Box(
            orientation="vertical", hexpand=True, css_classes=["fade", "card"]
        )
        self.main_box.append(self.task_row)
        self.main_box.append(self.sub_tasks_revealer)
        if self.get_prop("color") != "":
            self.main_box.add_css_class(f'task-{self.get_prop("color")}')
        # Box
        box = Gtk.Box(
            orientation="vertical",
            margin_start=12,
            margin_end=12,
            margin_bottom=6,
            margin_top=6,
        )
        box.append(top_drop_area)
        box.append(self.main_box)
        self.set_child(box)

    def add_task(self, task: dict) -> None:
        sub_task: Task = Task(task, self.window, self.tasks_panel, self)
        self.tasks_list.append(sub_task)
        sub_task.toggle_visibility(not task["deleted"])

    def add_sub_tasks(self) -> None:
        sub_count: int = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                sub_count += 1
                self.add_task(task)
        self.update_status()
        self.tasks_panel.update_status()

    def check_is_sub(self) -> None:
        if self.task["parent"] != "":
            self.is_sub_task = True
            self.main_box.add_css_class("sub-task")
            if not self.window.startup and self.parent != self.tasks_panel:
                self.parent.expand(True)
        else:
            self.main_box.add_css_class("task")

    def delete(self, *_) -> None:
        Log.info(f"Move task to trash: {self.task['id']}")

        self.toggle_visibility(False)
        self.task["deleted"] = True
        self.update_data()
        self.completed_btn.set_active(True)
        self.tasks_panel.trash_panel.trash_add(self.task)
        for task in get_children(self.tasks_list):
            if not task.task["deleted"]:
                task.delete()
        self.tasks_panel.details_panel.status.set_visible(True)

    def expand(self, expanded: bool) -> None:
        self.sub_tasks_revealer.set_reveal_child(expanded)
        if expanded:
            self.expand_icon.add_css_class("rotate")
        else:
            self.expand_icon.remove_css_class("rotate")

    def purge(self) -> None:
        """
        Completely remove widget
        """

        self.parent.tasks_list.remove(self)
        self.run_dispose()

    def toggle_visibility(self, on: bool) -> None:
        self.set_reveal_child(on)

    def update_status(self) -> None:
        n_completed: int = 0
        n_total: int = 0
        for task in UserData.get()["tasks"]:
            if task["parent"] == self.task["id"]:
                if not task["deleted"]:
                    n_total += 1
                    if task["completed"]:
                        n_completed += 1

        self.task_row.set_subtitle(
            _("Completed:") + f" {n_completed} / {n_total}"  # pyright: ignore
            if n_total > 0
            else ""
        )

    def update_data(self) -> None:
        """
        Sync self.task with user data.json
        """

        # data: UserDataDict = UserData.get()
        # for i, task in enumerate(data["tasks"]):
        #     if self.task["id"] == task["id"]:
        #         data["tasks"][i] = self.task
        #         UserData.set(data)
        #         return

    def on_completed_btn_toggled(self, btn: Gtk.Button) -> None:
        """
        Toggle check button and add style to the text
        """

        def set_text():
            if btn.get_active():
                text = Markup.add_crossline(self.task["text"])
                self.add_css_class("task-completed")
            else:
                text = Markup.rm_crossline(self.task["text"])
                self.remove_css_class("task-completed")
            self.task_row.set_title(text)

        # If task is just added set text and return to avoid useless sync
        if self.just_added:
            set_text()
            return

        # Update data
        self.task["completed"] = btn.get_active()
        self.task["synced_caldav"] = False
        self.update_data()
        # Update children
        children: list[Task] = get_children(self.tasks_list)
        for task in children:
            task.can_sync = False
            task.completed_btn.set_active(btn.get_active())
        # Update status
        if self.is_sub_task:
            self.parent.update_status()
        # Set text
        set_text()
        # Sync
        if self.can_sync:
            Sync.sync()
            self.tasks_panel.update_status()
            for task in children:
                task.can_sync = True

    def on_expand(self, *_) -> None:
        """
        Expand task row
        """

        self.expand(not self.sub_tasks_revealer.get_child_revealed())

    def on_details_btn_clicked(self, _btn):
        self.tasks_panel.sidebar.set_visible_child_name("details")
        self.tasks_panel.details_panel.update_info(self)

    def on_sub_task_added(self, entry: Gtk.Entry) -> None:
        """
        Add new Sub-Task
        """
        return

        # Return if entry is empty
        if entry.get_buffer().props.text == "":
            return
        # Add new sub-task
        new_sub_task: UserDataTask = TaskUtils.new_task(
            entry.get_buffer().props.text, parent=self.task["id"]
        )
        data: UserDataDict = UserData.get()
        data["tasks"].append(new_sub_task)
        UserData.set(data)
        # Add sub-task
        self.add_task(new_sub_task)
        # Clear entry
        entry.get_buffer().props.text = ""
        # Update status
        self.task["completed"] = False
        self.update_data()
        self.just_added = True
        self.completed_btn.set_active(False)
        self.just_added = False
        self.update_status()
        # Sync
        Sync.sync()

    # --- Drag and Drop --- #

    def on_drag_end(self, *_) -> bool:
        self.set_sensitive(True)

    def on_drag_begin(self, _, drag) -> bool:
        icon: Gtk.DragIcon = Gtk.DragIcon.get_for_drag(drag)
        icon.set_child(
            Gtk.Button(
                label=self.task["text"]
                if len(self.task["text"]) < 20
                else f"{self.task['text'][0:20]}..."
            )
        )

    def on_drag_prepare(self, *_) -> Gdk.ContentProvider:
        self.set_sensitive(False)
        value = GObject.Value(Task)
        value.set_object(self)
        return Gdk.ContentProvider.new_for_value(value)

    def on_task_top_drop(self, _drop, task, _x, _y) -> bool:
        """
        When task is dropped on "+" area on top of task
        """
        return

        # Return if task is itself
        if task == self:
            return False

        # Move data
        data: UserDataDict = UserData.get()
        tasks = data["tasks"]
        for i, t in enumerate(tasks):
            if t["id"] == self.task["id"]:
                self_idx = i
            elif t["id"] == task.task["id"]:
                task_idx = i
        tasks.insert(self_idx, tasks.pop(task_idx))
        UserData.set(data)

        # If task has the same parent
        if task.parent == self.parent:
            # Move widget
            self.parent.tasks_list.reorder_child_after(task, self)
            self.parent.tasks_list.reorder_child_after(self, task)
            return True

        # Change parent if different parents
        task.task["parent"] = self.task["parent"]
        task.task["synced_caldav"] = False
        task.update_data()
        task.purge()
        # Add new task widget
        new_task = Task(task.task, self.window, self.parent)
        self.parent.tasks_list.append(new_task)
        self.parent.tasks_list.reorder_child_after(new_task, self)
        self.parent.tasks_list.reorder_child_after(self, new_task)
        new_task.toggle_visibility(True)
        # Update status
        self.parent.update_status()
        task.parent.update_status()

        # Sync
        Sync.sync()

        return True

    def on_drop(self, _drop, task: Self, _x, _y) -> None:
        """
        When task is dropped on task and becomes sub-task
        """
        return

        if task == self or task.parent == self:
            return

        # Change parent
        task.task["parent"] = self.task["id"]
        task.task["synced_caldav"] = False
        task.update_data()
        # Move data
        data: UserDataDict = UserData.get()
        tasks = data["tasks"]
        last_sub_idx: int = 0
        for i, t in enumerate(tasks):
            if t["parent"] == self.task["id"]:
                last_sub_idx = tasks.index(t)
            if t["id"] == self.task["id"]:
                self_idx = i
            if t["id"] == task.task["id"]:
                task_idx = i
        tasks.insert(self_idx + last_sub_idx, tasks.pop(task_idx))
        UserData.set(data)
        # Remove old task
        task.purge()
        # Add new sub-task
        self.add_task(task.task.copy())
        self.task["completed"] = False
        self.update_data()
        self.just_added = True
        self.completed_btn.set_active(False)
        self.just_added = False
        # Update status
        task.parent.update_status()
        self.update_status()
        self.parent.update_status()

        # Sync
        Sync.sync()

        return True
