# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from unittest import TestCase
from gi.repository import Adw, Gio

from errands.widgets.window import WINDOW


class TestWindow(TestCase):
    def setUp(self):
        self.window = WINDOW

    def test_empty_state(self):
        name: str = self.window.stack.get_visible_child_name()
        self.assertEqual(name, "status")

    def test_min_stack_pages(self):
        pages_list: Gio.ListModel = self.window.stack.get_pages()
        pages: list[Adw.ViewStackPage] = [
            pages_list.get_item(i).get_name() for i in range(pages_list.get_n_items())
        ]
        expected_pages = ["trash", "status"]
        for page in expected_pages:
            self.assertTrue(page in pages)

    def test_add_toast(self):
        self.window.add_toast("Test toast")

    def test_existing_actions(self):
        actions: list[str] = (
            self.window.list_actions() + self.window.get_application().list_actions()
        )
        expected_actions: list[str] = [
            "show-help-overlay",
            "preferences",
            "sync",
            "import",
            "about",
            "quit",
        ]
        self.assertEqual(actions, expected_actions)

    def test_about_window(self):
        app: Adw.Application = self.window.get_application()
        app.activate_action("about")
        self.assertTrue(self.window.about_window.get_visible())
        self.window.about_window.close()
        self.assertFalse(self.window.about_window.get_visible())
