# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import GLib, Gio, Gtk, Secret  # type:ignore
from __main__ import APP_ID
from errands.lib.logging import Log

SECRETS_SCHEMA: Secret.Schema = Secret.Schema.new(
    APP_ID,
    Secret.SchemaFlags.NONE,
    {
        "account": Secret.SchemaAttributeType.STRING,
    },
)


class GSettings:
    """Class for accessing gsettings"""

    gsettings: Gio.Settings = None
    initialized: bool = False

    def _check_init(self):
        if not self.initialized:
            self.init()

    @classmethod
    def bind(
        self, setting: str, obj: Gtk.Widget, prop: str, invert: bool = False
    ) -> None:
        self._check_init(self)
        self.gsettings.bind(
            setting,
            obj,
            prop,
            (
                Gio.SettingsBindFlags.INVERT_BOOLEAN
                if invert
                else Gio.SettingsBindFlags.DEFAULT
            ),
        )

    @classmethod
    def get(self, setting: str):
        self._check_init(self)
        return self.gsettings.get_value(setting).unpack()

    @classmethod
    def set(self, setting: str, gvariant: str, value) -> None:
        self._check_init(self)
        self.gsettings.set_value(setting, GLib.Variant(gvariant, value))

    @classmethod
    def get_secret(self, account: str):
        self._check_init(self)
        return Secret.password_lookup_sync(SECRETS_SCHEMA, {"account": account}, None)

    @classmethod
    def set_secret(self, account: str, secret: str) -> None:
        self._check_init(self)

        return Secret.password_store_sync(
            SECRETS_SCHEMA,
            {
                "account": account,
            },
            Secret.COLLECTION_DEFAULT,
            f"Errands account credentials for {account}",
            secret,
            None,
        )

    @classmethod
    def delete_secret(self, account: str) -> bool:
        return Secret.password_clear_sync(SECRETS_SCHEMA, {"account": account}, None)

    @classmethod
    def init(self) -> None:
        Log.debug("Initialize GSettings")
        self.initialized = True
        self.gsettings = Gio.Settings.new(APP_ID)

        # Migrate old password
        try:
            account: int = self.gsettings.get_int("sync-provider")
            password: str = self.gsettings.get_string("sync-password")
            if 0 < account < 3 and password:
                account = "Nextcloud" if account == 1 else "CalDAV"
                self.set_secret(account, password)
                self.gsettings.set_string("sync-password", "")  # Clean pass
        except:
            pass
