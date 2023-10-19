import re
from gi.repository import GLib


class Markup:
    @classmethod
    def escape(self, text: str) -> str:
        return GLib.markup_escape_text(text)

    @classmethod
    def add_crossline(self, text: str) -> str:
        return f"<s>{text}</s>"

    @classmethod
    def rm_crossline(self, text: str) -> str:
        return text.replace("<s>", "").replace("</s>", "")

    @classmethod
    def find_url(self, text: str) -> str:
        """Convert urls to markup. Make sure to escape text before calling."""

        string = text
        urls = re.findall(r"(https?://\S+)", string)
        for url in urls:
            string = string.replace(url, f'<a href="{url}">{url}</a>')
        return string
