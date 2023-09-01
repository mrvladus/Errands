# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Code modified from Dialect
# https://github.com/dialect-app/dialect/blob/main/dialect/widgets/theme_switcher.py

from gi.repository import Adw, GObject, Gtk
from .utils import GSettings


@Gtk.Template(resource_path="/io/github/mrvladus/Errands/theme_switcher.ui")
class ThemeSwitcher(Gtk.Box):
    __gtype_name__ = "ThemeSwitcher"

    # Child widgets
    system = Gtk.Template.Child()
    light = Gtk.Template.Child()
    dark = Gtk.Template.Child()

    # Properties
    color_scheme = "light"

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.style_manager = Adw.StyleManager.get_default()
        self.color_scheme = GSettings.get("theme")
        GSettings.bind("theme", self, "selected_color_scheme")

    @GObject.Property(type=str)
    def selected_color_scheme(self) -> str:
        return self.color_scheme

    @selected_color_scheme.setter
    def selected_color_scheme(self, color_scheme) -> None:
        self.color_scheme = color_scheme

        if color_scheme == "auto":
            self.system.props.active = True
            self.style_manager.props.color_scheme = Adw.ColorScheme.PREFER_LIGHT
        if color_scheme == "light":
            self.light.props.active = True
            self.style_manager.props.color_scheme = Adw.ColorScheme.FORCE_LIGHT
        if color_scheme == "dark":
            self.dark.props.active = True
            self.style_manager.props.color_scheme = Adw.ColorScheme.FORCE_DARK

    @Gtk.Template.Callback()
    def on_theme_change(self, _) -> None:
        if self.system.props.active:
            self.selected_color_scheme = "auto"
        if self.light.props.active:
            self.selected_color_scheme = "light"
        if self.dark.props.active:
            self.selected_color_scheme = "dark"
