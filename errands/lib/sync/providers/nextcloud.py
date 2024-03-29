from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.lib.sync.providers.caldav import SyncProviderCalDAV


class SyncProviderNextcloud(SyncProviderCalDAV):
    def __init__(self, *args, **kwargs) -> None:
        return super().__init__(name="Nextcloud", *args, **kwargs)

    def _check_url(self) -> None:
        Log.debug("Sync: Checking URL")

        # Add prefix if needed
        if not self.url.startswith("http"):
            self.url = "https://" + self.url
            GSettings.set("sync-url", "s", self.url)
        # Add suffix if needed
        if not "remote.php/dav" in GSettings.get("sync-url"):
            self.url = f"{self.url}/remote.php/dav/"
            GSettings.set("sync-url", "s", self.url)
        else:
            self.url = GSettings.get("sync-url")

        Log.debug(f"Sync: URL is set to {self.url}")
