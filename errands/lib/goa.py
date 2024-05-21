# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import TypedDict
from errands.lib.logging import Log


class GoaCredentials(TypedDict):
    url: str
    username: str
    password: str


def get_goa_credentials(acc_name: str) -> GoaCredentials | None:
    """
    If Gnome Online Accounts is installed, try to get account info.
    Only for password based account yet.
    """

    try:
        import gi  # type:ignore

        gi.require_version("Goa", "1.0")
        from gi.repository import Goa  # type:ignore
    except (ValueError, ModuleNotFoundError):
        Log.debug("Gnome Online Accounts is not installed. Skipping...")
        return None

    try:
        client: Goa.Client = Goa.Client.new_sync(None)
        accounts: list[Goa.ObjectProxy] = client.get_accounts()
        for account in accounts:
            acc: Goa.AccountProxy = account.get_account()
            name: str = acc.get_cached_property("ProviderName").get_string()
            if name == acc_name:
                Log.debug(f"GOA: Getting data for {acc_name}")
                username: str = (
                    acc.get_cached_property("PresentationIdentity")
                    .get_string()
                    .rsplit("@", 1)[0]
                )
                password = account.get_password_based().call_get_password_sync(
                    arg_id=acc.get_cached_property("Id").get_string()
                )
                # Get url
                try:
                    url: str = (
                        account.get_calendar()
                        .get_cached_property("Uri")
                        .get_string()
                        .replace(username + "@", "")
                    )
                except:
                    url: str = (
                        "https://"
                        + (
                            acc.get_cached_property("PresentationIdentity")
                            .get_string()
                            .split("@")[-1]
                        )
                        + "/remote.php/dav/"
                    )

                return GoaCredentials(url=url, username=username, password=password)
    except Exception as e:
        Log.error(f"GOA: Can't get credentials. {e}")
        return None

    return None
