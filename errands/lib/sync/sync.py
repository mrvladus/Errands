# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from __future__ import annotations
from typing import TYPE_CHECKING

from errands.state import State

if TYPE_CHECKING:
    from errands.widgets.window import Window

from errands.lib.sync.providers.caldav import SyncProviderCalDAV
from errands.lib.sync.providers.nextcloud import SyncProviderNextcloud
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.data import UserData
from errands.lib.utils import threaded
from gi.repository import GLib  # type:ignore


class Sync:
    provider = None
    window: Window = None
    syncing: bool = False
    sync_again: bool = False

    @classmethod
    def init(self, window: Window, testing: bool = False) -> None:
        self.window: Window = window
        Log.info("Sync: Initialize sync provider")
        match GSettings.get("sync-provider"):
            case 0:
                Log.info("Sync: Sync disabled")
                UserData.clean_deleted()
            case 1:
                self.provider = SyncProviderNextcloud(
                    window=self.window, testing=testing
                )
            case 2:
                self.provider = SyncProviderCalDAV(window=self.window, testing=testing)

    @classmethod
    @threaded
    def sync(self, update_ui: bool = True) -> None:
        """
        Sync tasks without blocking the UI
        """
        if GSettings.get("sync-provider") == 0:
            UserData.clean_deleted()
            if update_ui:
                GLib.idle_add(State.sidebar.update_ui)
            return
        if not self.provider:
            GLib.idle_add(State.sidebar.sync_indicator.set_visible, True)
            self.init(self.window)
            GLib.idle_add(State.sidebar.sync_indicator.set_visible, False)
        if self.provider and self.provider.can_sync:
            if self.syncing:
                self.sync_again = True
                return
            self.syncing = True
            GLib.idle_add(State.sidebar.sync_indicator.set_visible, True)
            self.provider.sync()
            UserData.clean_deleted()
            if update_ui:
                GLib.idle_add(State.sidebar.update_ui)
            if self.sync_again:
                self.sync()
                self.sync_again = False
            GLib.idle_add(State.sidebar.sync_indicator.set_visible, False)
            self.syncing = False

    @classmethod
    def test_connection(self) -> bool:
        self.init(testing=True, window=self.window)
        return self.provider.can_sync
