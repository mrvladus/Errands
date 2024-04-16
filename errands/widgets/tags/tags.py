# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations


from gi.repository import Adw, GObject, Gtk  # type:ignore

from errands.lib.data import UserData
from errands.lib.logging import Log
from errands.lib.utils import get_children
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.entries import ErrandsEntryRow
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView


class Tags(Adw.Bin):
    def __init__(self):
        super().__init__()
        State.tags_page = self
        self.__build_ui()
        # Load tags
        for tag in UserData.tags:
            self.tags_list.append(Tag(tag.text, self))
        self.update_ui()

    def __build_ui(self):
        # Status Page
        self.status_page = Adw.StatusPage(
            title=_("No Tags Found"),
            description=_("Add new Tags in the entry above"),
            icon_name="errands-info-symbolic",
            vexpand=True,
            css_classes=["compact"],
        )

        # Content
        self.tags_list = Gtk.ListBox(
            selection_mode=Gtk.SelectionMode.NONE,
            margin_bottom=32,
            margin_end=12,
            margin_start=12,
            margin_top=6,
            css_classes=["boxed-list"],
        )
        content = Gtk.ScrolledWindow(
            propagate_natural_height=True,
            child=Adw.Clamp(
                maximum_size=1000, tightening_threshold=300, child=self.tags_list
            ),
        )
        content.bind_property(
            "visible",
            self.status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar(title_widget=Adw.WindowTitle(title=_("Tags")))],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    children=[
                        self.status_page,
                        Adw.Clamp(
                            maximum_size=1000,
                            tightening_threshold=300,
                            child=ErrandsListBox(
                                selection_mode=Gtk.SelectionMode.NONE,
                                margin_bottom=6,
                                margin_start=12,
                                margin_end=12,
                                css_classes=["boxed-list"],
                                children=[
                                    ErrandsEntryRow(
                                        on_entry_activated=self._on_tag_added,
                                        height_request=60,
                                        activatable=False,
                                        title=_("Add new Tag"),
                                    )
                                ],
                            ),
                        ),
                        content,
                    ],
                ),
            )
        )

    @property
    def tags(self) -> list[Tag]:
        return get_children(self.tags_list)

    def update_ui(self, update_tag_rows: bool = True):
        tags: list[str] = [t.text for t in UserData.tags]

        # Remove tags
        for row in self.tags:
            if row.get_title() not in tags:
                self.tags_list.remove(row)

        # Add tags
        tags_rows_texts: list[str] = [t.get_title() for t in self.tags]
        for tag in tags:
            if tag not in tags_rows_texts:
                self.tags_list.append(Tag(tag, self))

        # Set activatable rows
        if update_tag_rows:
            tags_in_tasks = [t.tags for t in UserData.tasks if t.tags]
            for tag in self.tags:
                tag.update_ui(tags_in_tasks)

        self.tags_list.set_visible(len(self.tags) > 0)
        if State.main_window:
            State.tags_sidebar_row.update_ui()

    def _on_tag_added(self, entry: Adw.EntryRow):
        text: str = entry.get_text().strip().strip(",")
        if text.strip(" \n\t") == "" or text in [t.text for t in UserData.tags]:
            return
        self.tags_list.append(Tag(text, self))
        UserData.add_tag(text)
        entry.set_text("")
        self.update_ui()


class Tag(Adw.ActionRow):
    def __init__(self, title: str, tags: Tags):
        super().__init__()
        self.tags = tags
        self.set_title(title)
        delete_btn = Gtk.Button(
            icon_name="errands-trash-symbolic",
            css_classes=["flat", "error"],
            tooltip_text=_("Delete"),
            valign=Gtk.Align.CENTER,
        )
        delete_btn.connect("clicked", self.delete)
        self.add_prefix(delete_btn)
        arrow = Gtk.Image(icon_name="errands-right-symbolic")
        arrow.bind_property(
            "visible",
            self,
            "activatable",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        self.number_of_tasks: Gtk.Button = Gtk.Button(
            css_classes=["flat", "dim-label", "circular"],
            can_target=False,
            halign=Gtk.Align.CENTER,
            valign=Gtk.Align.CENTER,
        )
        self.add_suffix(self.number_of_tasks)

    def delete(self, _btn: Gtk.Button):
        Log.info(f"Tags: Delete Tag '{self.get_title()}'")
        for task in State.get_tasks():
            for tag in task.tags:
                if self.get_title() == tag.title:
                    task.tags_bar.remove(tag)
                    if len(task.tags) == 0:
                        task.tags_bar_rev.set_reveal_child(False)
                    break
        UserData.remove_tags([self.get_title()])
        self.tags.update_ui(False)

    def update_ui(self, tags_in_tasks: list[list[str]] = None):
        if not tags_in_tasks:
            tags_in_tasks: list[list[str]] = [t.tags for t in UserData.tasks if t.tags]

        count: int = 0
        for tags in tags_in_tasks:
            if self.get_title() in tags:
                count += 1

        self.set_activatable(False)
        self.number_of_tasks.set_label(str(count) if count > 0 else "")
