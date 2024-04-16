from gi.repository import Adw, Gtk, GObject  # type:ignore

from errands.lib.utils import get_children
from errands.widgets.shared.components.boxes import ErrandsBox, ErrandsListBox
from errands.widgets.shared.components.buttons import ErrandsButton
from errands.widgets.task import Task
from errands.widgets.task import Tag


class TodayTask(Adw.Bin):
    def __init__(self, task: Task) -> None:
        super().__init__()
        self.task: Task = task
        self.__build_ui()
        self.update_ui()

    def __build_ui(self):
        # Title row
        self.title_row = Adw.ActionRow(
            height_request=60,
            use_markup=True,
            css_classes=["transparent", "rounded-corners"],
        )
        self.task.title.title_row.bind_property(
            "title",
            self.title_row,
            "title",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        self.task.title.title_row.bind_property(
            "css-classes",
            self.title_row,
            "css-classes",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )
        self.task.task_list.header_bar.title.bind_property(
            "title",
            self.title_row,
            "subtitle",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

        # Complete button
        self.complete_btn: Gtk.CheckButton = Gtk.CheckButton(
            tooltip_text=_("Toggle Completion"),
            valign=Gtk.Align.CENTER,
            css_classes=["selection-mode"],
        )
        self.task.title.complete_btn.bind_property(
            "active",
            self.complete_btn,
            "active",
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL,
        )

        self.title_row.add_prefix(self.complete_btn)

        # Edit row
        self.edit_row: Adw.EntryRow = Adw.EntryRow(
            title=_("Edit"),
            visible=False,
            height_request=60,
            show_apply_button=True,
            css_classes=["transparent", "rounded-corners"],
        )
        self.edit_row.bind_property(
            "visible",
            self.title_row,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.INVERT_BOOLEAN
            | GObject.BindingFlags.INVERT_BOOLEAN,
        )
        self.edit_row.connect("apply", self._on_edit_row_applied)

        # Cancel edit button
        self.edit_row.add_suffix(
            ErrandsButton(
                tooltip_text=_("Cancel"),
                valign=Gtk.Align.CENTER,
                icon_name="window-close-symbolic",
                css_classes=["circular"],
                on_click=self._on_cancel_edit_btn_clicked,
            )
        )

        # Tags bar
        self.tags_bar: Gtk.FlowBox = Gtk.FlowBox(
            height_request=20,
            margin_start=12,
            margin_end=12,
            margin_bottom=3,
            selection_mode=Gtk.SelectionMode.NONE,
            max_children_per_line=1000,
        )
        self.tags_bar_rev = Gtk.Revealer(child=self.tags_bar)

        # Tool bar
        # self.toolbar = TaskToolbar(self.task)

        # Main box
        self.main_box: ErrandsBox = ErrandsBox(
            orientation=Gtk.Orientation.VERTICAL,
            margin_start=12,
            margin_end=12,
            margin_top=6,
            margin_bottom=6,
            css_classes=["card", "fade"],
            children=[
                ErrandsListBox(
                    children=[self.title_row, self.edit_row],
                    css_classes=["transparent", "rounded-corners"],
                ),
                self.tags_bar_rev,
                # self.toolbar,
            ],
        )

        self.set_child(self.main_box)

    # ------ PROPERTIES ------ #

    @property
    def tags(self) -> list[Tag]:
        """Top-level Tasks"""

        return get_children(self.tags_bar)

    # ------ PUBLIC METHODS ------ #

    def add_tag(self, tag: str) -> None:
        self.tags_bar.append(Tag(tag, self.tags_bar))

    def update_ui(self):
        # Update tags
        tags_list_text: list[str] = [tag.title for tag in self.tags]

        # Delete tags
        for tag in self.tags:
            if tag.title not in self.task.task_data.tags:
                self.tags_bar.remove(tag)

        # Add tags
        for tag in self.task.task_data.tags:
            if tag not in tags_list_text:
                self.add_tag(tag)

        self.tags_bar_rev.set_reveal_child(self.task.task_data.tags != [])

    # ------ SIGNAL HANDLERS ------ #

    def _on_edit_row_applied(self, entry: Adw.EntryRow) -> None:
        return
        text: str = entry.props.text.strip()
        entry.set_visible(False)
        if not text or text == self.task.task_data.text:
            return
        self.update_props(["text", "synced"], [text, False])
        self.update_ui()
        Sync.sync()

    def _on_cancel_edit_btn_clicked(self, _btn: Gtk.Button) -> None:
        self.edit_row.props.text = ""
        self.edit_row.emit("apply")
