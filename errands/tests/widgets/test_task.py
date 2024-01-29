# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from unittest import TestCase
from errands.widgets.window import WINDOW


class TestTask(TestCase):
    def setUp(self):
        self.window = WINDOW
