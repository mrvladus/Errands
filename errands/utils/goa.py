# Copyright 2023-2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.utils.logging import Log


def get_goa_credentials(acc_name: str) -> dict[str, str] | None:
    """
    If Gnome Online Accounts is installed, try to get account info.
    Only for password based account yet.
    """

    try:
        import gi

        gi.require_version("Goa", "1.0")
        from gi.repository import Goa
    except (ValueError, ModuleNotFoundError):
        Log.debug("Gnome Online Accounts is not installed. Skipping...")
        return None

    client: Goa.Client = Goa.Client.new_sync(None)
    accounts: list[Goa.ObjectProxy] = client.get_accounts()
    for account in accounts:
        acc: Goa.AccountProxy = account.get_account()
        name: str = acc.get_cached_property("ProviderName").get_string()
        if name == acc_name:
            Log.debug(f"GOA: Getting data for {acc_name}")
            username, url = (
                acc.get_cached_property("PresentationIdentity").get_string().split("@")
            )
            password = account.get_password_based().call_get_password_sync(
                arg_id=acc.get_cached_property("Id").get_string()
            )
            result: dict[str, str] = {
                "url": url,
                "username": username,
                "password": password,
            }
            return result
    return None
