# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GLib  # type:ignore

from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.providers.caldav import SyncProviderCalDAV
from errands.lib.sync.providers.nextcloud import SyncProviderNextcloud
from errands.lib.utils import threaded
from errands.state import State


class Sync:
    provider = None
    syncing: bool = False
    sync_again: bool = False

    @classmethod
    def init(self, testing: bool = False) -> None:
        Log.info("Sync: Initialize sync provider")
        match GSettings.get("sync-provider"):
            case 0:
                Log.info("Sync: Sync disabled")
                UserData.clean_deleted()
            case 1:
                self.provider = SyncProviderNextcloud(testing=testing)
            case 2:
                self.provider = SyncProviderCalDAV(testing=testing)

    @classmethod
    @threaded
    def sync(self) -> None:
        """
        Sync tasks without blocking the UI
        """
        if GSettings.get("sync-provider") == 0:
            UserData.clean_deleted()
            return
        if not self.provider:
            GLib.idle_add(State.sidebar.toggle_sync_indicator, True)
            self.init()
            GLib.idle_add(State.sidebar.toggle_sync_indicator, False)
        if self.provider and self.provider.can_sync:
            if self.syncing:
                self.sync_again = True
                return
            self.syncing = True
            if State.view_stack.get_visible_child_name() == "errands_status_page":
                State.view_stack.set_visible_child_name("errands_syncing_page")
            GLib.idle_add(State.sidebar.toggle_sync_indicator, True)
            self.provider.sync()
            UserData.clean_deleted()
            if self.sync_again:
                self.sync()
                self.sync_again = False
            GLib.idle_add(State.sidebar.toggle_sync_indicator, False)
            self.syncing = False
            if (
                State.view_stack.get_visible_child_name() == "errands_syncing_page"
                and UserData.task_lists
            ):
                State.view_stack.set_visible_child_name("errands_today_page")

    # TODO: Needs to be threaded to not block UI
    @classmethod
    def test_connection(self) -> tuple:
        self.init(testing=True)
        return self.provider.can_sync, self.provider.err
