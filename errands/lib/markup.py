# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import re
from gi.repository import GLib  # type:ignore


class Markup:
    @classmethod
    def escape(self, text: str) -> str:
        return GLib.markup_escape_text(text)

    @classmethod
    def find_url(self, text: str) -> str:
        """Convert urls to markup. Make sure to escape text before calling."""

        string: str = text
        urls: list[str] = re.findall(r"(https?://\S+)", string)
        for url in urls:
            string = string.replace(url, f'<a href="{url}">{url}</a>')
        return string
