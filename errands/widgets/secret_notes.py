# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING, TypedDict

from errands.lib.functions import get_children

if TYPE_CHECKING:
    from errands.widgets.window import Window

from uuid import uuid4
from errands.lib.gsettings import GSettings
from errands.lib.data import UserData
from errands.widgets.components import Box
from errands.lib.encryption import encrypt, decrypt
from gi.repository import Adw, Gtk, GtkSource, GObject  # type:ignore


class NoteDict(TypedDict):
    text: str
    uid: str


class SecretNote(Gtk.ListBoxRow):
    def __init__(self, notes_page: SecretNotesPage, note: NoteDict):
        super().__init__()
        self.notes_page: SecretNotesPage = notes_page
        self.notes_list: Gtk.ListBox = notes_page.notes_list
        self.password: str = notes_page.password
        self.uid: str = note["uid"]
        self.text: str = note["text"]
        self._build_ui()

    def _build_ui(self):
        self.set_activatable(False)
        self.set_margin_top(6)
        self.set_margin_bottom(6)
        self.set_margin_start(12)
        self.set_margin_end(12)

        # Text view
        self.text_view = GtkSource.View(
            css_classes=["card", "secret-note"],
            height_request=100,
            top_margin=12,
            bottom_margin=12,
            right_margin=12,
            left_margin=12,
            wrap_mode=Gtk.WrapMode.WORD_CHAR,
        )
        self.buffer = self.text_view.get_buffer()
        self.buffer.props.text = self.text
        lm: GtkSource.LanguageManager = GtkSource.LanguageManager.get_default()
        self.buffer.set_language(lm.get_language("markdown"))
        self.buffer.connect("changed", self._on_text_changed)

        # Delete button
        delete_btn = Gtk.Button(
            icon_name="errands-trash-symbolic",
            css_classes=["flat", "circular", "error"],
            halign=Gtk.Align.END,
            valign=Gtk.Align.END,
            margin_end=6,
            margin_bottom=6,
        )
        delete_btn.connect("clicked", self._on_delete_clicked)

        # Overlay
        overlay = Gtk.Overlay(child=self.text_view)
        overlay.add_overlay(delete_btn)

        self.set_child(overlay)

    def _on_text_changed(self, buffer: GtkSource.Buffer):
        text: str = encrypt(buffer.props.text, self.password)
        UserData.execute(
            "UPDATE secret_notes SET text = ? WHERE uid = ?", (text, self.uid)
        )

    def _on_delete_clicked(self, _btn):
        UserData.execute("DELETE FROM secret_notes WHERE uid = ?", (self.uid,))
        self.notes_list.remove(self)
        self.notes_page.update_ui()


class SecretNotesPage(Adw.Bin):
    password: str = None

    def __init__(self, password: str):
        super().__init__()
        self.password: str = password
        self._build_ui()
        self._load_notes()
        self.update_ui()

    def _add_note(self, note: NoteDict):
        self.notes_list.append(SecretNote(self, note))

    def _build_ui(self):
        # Headerbar
        hb: Adw.HeaderBar = Adw.HeaderBar(
            title_widget=Adw.WindowTitle(title=_("Secret Notes"))
        )

        # Add note button
        self.add_btn: Gtk.Button = Gtk.Button(
            child=Adw.ButtonContent(icon_name="errands-add-symbolic", label=_("Add"))
        )
        self.add_btn.connect("clicked", self._on_add_note_clicked)
        hb.pack_start(self.add_btn)

        self.notes_list: Gtk.ListBox = Gtk.ListBox(
            selection_mode=0, css_classes=["background"]
        )

        # Status
        self.status_page = Adw.StatusPage(
            title="Add Note",
            icon_name="errands-notes-symbolic",
            vexpand=True,
        )

        # Scrolled window
        self.scrl = Gtk.ScrolledWindow(
            child=Adw.Clamp(
                child=self.notes_list,
                margin_bottom=36,
                maximum_size=1000,
                tightening_threshold=300,
            ),
            propagate_natural_height=True,
        )
        self.scrl.bind_property(
            "visible",
            self.status_page,
            "visible",
            GObject.BindingFlags.SYNC_CREATE
            | GObject.BindingFlags.BIDIRECTIONAL
            | GObject.BindingFlags.INVERT_BOOLEAN,
        )

        # Toolbar view
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(
            content=Box(
                children=[
                    self.status_page,
                    self.scrl,
                ],
                orientation=Gtk.Orientation.VERTICAL,
            )
        )
        toolbar_view.add_top_bar(hb)
        self.set_child(toolbar_view)

    def _load_notes(self):
        notes: list[NoteDict] = [
            NoteDict(text=decrypt(i[1], self.password), uid=i[0])
            for i in UserData.execute("SELECT uid, text FROM secret_notes", fetch=True)
        ]
        for note in notes:
            self._add_note(note)

    def _on_add_note_clicked(self, _btn):
        uid: str = str(uuid4())
        UserData.execute(
            "INSERT INTO secret_notes (text, uid) VALUES (?, ?)", ("", uid)
        )
        self._add_note(NoteDict(text="", uid=uid))
        self.update_ui()

    def update_ui(self):
        self.status_page.set_visible(not get_children(self.notes_list))


class SecretNotesPasswordPage(Adw.Bin):
    attempts: int = 0

    def __init__(self, notes_win: SecretNotesWindow):
        super().__init__()
        self.notes_win: SecretNotesWindow = notes_win
        self._build_ui()

    def _build_ui(self):
        password: str | None = GSettings.get_secret("errands_secret_notes")

        # Status page
        self.status_page = Adw.StatusPage(
            title=_("Secret Notes"),
            description=(
                _("Enter password") if password != None else _("Create password")
            ),
            icon_name="errands-private-symbolic",
            height_request=300,
        )

        # Entry
        password_entry = Adw.PasswordEntryRow(
            title=_("Password"),
            css_classes=["card"],
            height_request=60,
            margin_end=12,
            margin_start=12,
        )
        password_entry.connect("entry-activated", self._on_password_entered)

        # Attempts bar
        self.attempts_bar = Gtk.ProgressBar(
            text=_("Attempts"),
            show_text=True,
            margin_top=12,
            margin_end=12,
            margin_start=12,
        )
        self.attempts_bar.add_css_class("secret-notes-progressbar")
        self.attempts_bar.set_fraction(0.5)
        self.attempts_rev = Gtk.Revealer(child=self.attempts_bar)

        # Delete notes button
        self.delete_btn = Gtk.Button(
            label=_("Delete Notes"),
            css_classes=["pill", "destructive-action"],
            halign=Gtk.Align.CENTER,
            margin_top=24,
            visible=password != None,
        )
        self.delete_btn.connect("clicked", self._on_delete_clicked)

        # Toolbar view
        toolbar_view: Adw.ToolbarView = Adw.ToolbarView(
            content=Adw.Clamp(
                child=Box(
                    children=[
                        self.status_page,
                        password_entry,
                        self.attempts_rev,
                        self.delete_btn,
                    ],
                    orientation=Gtk.Orientation.VERTICAL,
                ),
                margin_bottom=36,
                valign=Gtk.Align.CENTER,
            )
        )
        toolbar_view.add_top_bar(Adw.HeaderBar(show_title=False))
        self.set_child(toolbar_view)

    def _on_password_entered(self, entry: Adw.PasswordEntryRow):
        password: str = entry.get_text()

        # Ignore empty password
        if not password:
            return

        # Get user password
        notes_password: str | None = GSettings.get_secret("errands_secret_notes")

        # Create password if not exists and open notes
        if not notes_password:
            # Save password
            GSettings.set_secret("errands_secret_notes", password)
            # Create notes table
            UserData.execute(
                """
                CREATE TABLE IF NOT EXISTS secret_notes (
                text TEXT,
                uid TEXT NOT NULL
                )
                """
            )
            # Open notes
            self.notes_win.set_content(SecretNotesPage(password))
            return

        # Verify password within 3 attempts and open notes
        if notes_password != password:
            # Delete data after 3 failed attempts
            if self.attempts > 2:
                UserData.execute("DROP TABLE secret_notes")
                GSettings.delete_secret("errands_secret_notes")
                self.delete_btn.set_visible(False)
                self.status_page.set_description(_("Create password"))
            # Update progressbar after failed attempt
            else:
                self.attempts += 1
                self.attempts_bar.set_fraction(self.attempts / 3)
                self.attempts_rev.set_reveal_child(True)
        else:
            # Open notes
            self.notes_win.set_content(SecretNotesPage(password))

    def _on_delete_clicked(self, btn: Gtk.Button):
        btn.set_visible(False)
        UserData.execute("DROP TABLE secret_notes")
        GSettings.delete_secret("errands_secret_notes")
        self.status_page.set_description(_("Create password"))


class SecretNotesWindow(Adw.Window):
    def __init__(self, window: Window):
        super().__init__()
        self.window: Window = window
        self._build_ui()

    def _build_ui(self):
        self.props.width_request = 360
        self.props.height_request = 360
        self.set_hide_on_close(True)
        self.set_title(_("Secret Notes"))
        GSettings.bind("secret-notes-width", self, "default_width")
        GSettings.bind("secret-notes-height", self, "default_height")

        self.set_content(SecretNotesPasswordPage(self))
