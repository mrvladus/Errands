# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from dataclasses import dataclass

from gi.repository import Gio  # type:ignore


@dataclass
class ErrandsMenuItem:
    label: str
    detailed_action: str


class ErrandsSimpleMenu(Gio.Menu):
    def __init__(self, items: tuple[ErrandsMenuItem]) -> None:
        super().__init__()
        for item in items:
            self.append(item.label, item.detailed_action)
