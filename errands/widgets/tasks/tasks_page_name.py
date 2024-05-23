# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


class TasksPageName:
    page_name: str = "tasks"

    @property
    def name(self) -> str:
        return f"errands_{self.page_name}_page"

    @property
    def title(self) -> str:
        return self.page_name.capitalize()
