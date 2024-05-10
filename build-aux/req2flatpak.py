#!/usr/bin/env python3

# req2flatpak is MIT-licensed.
#
# Copyright 2022 johannesjh
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""
req2flatpak converts python package requirements to a flatpak build module.

The req2flatpak script takes python package requirements as input, e.g., as
``requirements.txt`` file. It allows to specify the target platform’s
python version and architecture. The script outputs an automatically
generated ``flatpak-builder`` build module. The build module, if included
into a flatpak-builder build manifest, will install the python packages
using pip.
"""

import argparse
import json
import logging
import pathlib
import re
import shelve
import sys
import urllib.request
from contextlib import nullcontext, suppress
from dataclasses import asdict, dataclass, field
from itertools import product
from typing import (
    Any,
    Dict,
    FrozenSet,
    Generator,
    Hashable,
    Iterable,
    Iterator,
    List,
    Optional,
    Set,
    Tuple,
    Union,
)
from urllib.parse import urlparse

import pkg_resources

logger = logging.getLogger(__name__)

try:
    import yaml
except ImportError:
    yaml = None  # type: ignore

# =============================================================================
# Helper functions / semi vendored code
# =============================================================================

try:
    # added with py 3.8
    from functools import cached_property  # type: ignore[attr-defined]
except ImportError:
    # Inspired by the implementation in the standard library
    # pylint: disable=invalid-name,too-few-public-methods
    class cached_property:  # type: ignore[no-redef]
        """A property-like wrapper that caches the value."""

        def __init__(self, func):
            """Init."""
            self.func = func
            self.attrname = None
            self.__doc__ = func.__doc__

        def __set_name__(self, owner, name):
            """Set name of the attribute this instance is assigned to."""
            self.attrname = name

        def __get__(self, instance, owner=None):
            """Return value if this is set as an attribute."""
            if not self.attrname:
                raise TypeError("cached_property must be used as an attribute.")
            _None = object()
            cache = instance.__dict__
            val = cache.get(self.attrname, _None)
            if val is _None:
                val = self.func(instance)
                cache[self.attrname] = val
            return val


class InvalidWheelFilename(Exception):
    """An invalid wheel filename was found, users should refer to PEP 427."""


try:
    # use packaging.tags functionality if available
    from packaging.utils import parse_wheel_filename

    def tags_from_wheel_filename(filename: str) -> Set[str]:
        """
        Parses a wheel filename into a list of compatible platform tags.

        Implemented using functionality from ``packaging.utils.parse_wheel_filename``.
        """
        _, _, _, tags = parse_wheel_filename(filename)
        return {str(tag) for tag in tags}

except ModuleNotFoundError:
    # fall back to a local implementation
    # that is heavily inspired by / almost vendored from the `packaging` package:
    def tags_from_wheel_filename(filename: str) -> Set[str]:
        """
        Parses a wheel filename into a list of compatible platform tags.

        Implemented as (semi-)vendored functionality in req2flatpak.
        """
        Tag = Tuple[str, str, str]

        # the following code is based on packaging.tags.parse_tag,
        # it is needed for the parse_wheel_filename function:
        def parse_tag(tag: str) -> FrozenSet[Tag]:
            tags: Set[Tag] = set()
            interpreters, abis, platforms = tag.split("-")
            for interpreter in interpreters.split("."):
                for abi in abis.split("."):
                    for platform_ in platforms.split("."):
                        tags.add((interpreter, abi, platform_))
            return frozenset(tags)

        # the following code is based on packaging.utils.parse_wheel_filename:
        # pylint: disable=redefined-outer-name
        def parse_wheel_filename(
            wheel_filename: str,
        ) -> Iterable[Tag]:
            if not wheel_filename.endswith(".whl"):
                raise InvalidWheelFilename(
                    "Error parsing wheel filename: "
                    "Invalid wheel filename (extension must be '.whl'): "
                    f"{wheel_filename}"
                )
            wheel_filename = wheel_filename[:-4]
            dashes = wheel_filename.count("-")
            if dashes not in (4, 5):
                raise InvalidWheelFilename(
                    "Error parsing wheel filename: "
                    "Invalid wheel filename (wrong number of parts): "
                    f"{wheel_filename}"
                )
            parts = wheel_filename.split("-", dashes - 2)
            return parse_tag(parts[-1])

        return {"-".join(tag_tuple) for tag_tuple in parse_wheel_filename(filename)}


# =============================================================================
# Data Structures
# =============================================================================


@dataclass(frozen=True)
class Platform:
    """Represents a target platform for python package installations."""

    python_version: List[str]
    """A list of python version numbers, similar to ``platform.python_version_tuple()``."""

    python_tags: List[str]
    """A list of platform tags, similar to ``packaging.tags.sys_tags()``."""


@dataclass(frozen=True)
class Requirement:
    """Represents a python package requirement."""

    package: str
    """A python package name."""

    version: str
    """The exact version of the package."""


@dataclass(frozen=True)
class Download(Requirement):
    """Represents a python package download."""

    filename: str
    url: str
    sha256: str

    @cached_property
    def is_wheel(self):
        """True if this download is a wheel."""
        return self.filename.endswith(".whl")

    @cached_property
    def is_sdist(self):
        """True if this download is a source distribution."""
        return not self.is_wheel and not self.filename.endswith(".egg")

    @cached_property
    def tags(self) -> Optional[Set[str]]:
        """Returns a list of tags that this download is compatible for."""
        # https://packaging.pypa.io/en/latest/utils.html#packaging.utils.parse_wheel_filename
        # https://packaging.pypa.io/en/latest/utils.html#packaging.utils.parse_sdist_filename
        if not self.is_sdist and self.filename.endswith(".whl"):
            return tags_from_wheel_filename(self.filename)
        return None

    @cached_property
    def arch(self) -> Optional[str]:
        """Returns a wheel's target architecture, and None for sdists."""
        if not self.is_sdist and self.is_wheel and self.tags:
            if any(tag.endswith("x86_64") for tag in self.tags):
                return "x86_64"
            if any(tag.endswith("aarch64") for tag in self.tags):
                return "aarch64"
        return None

    def __lt__(self, other):
        """Makes this class sortable."""
        # Note: Implementing __lt__ is sufficient to make a class sortable,
        # see, e.g., https://stackoverflow.com/a/7152796

        def sort_keys(download: Download) -> Tuple[str, str, str]:
            """A tuple of package, version and architecture is used as key for sorting."""
            return download.package, download.version, download.arch or ""

        return sort_keys(self) < sort_keys(other)


@dataclass(frozen=True)
class Release(Requirement):
    """Represents a package release as name, version, and downloads."""

    package: str
    version: str
    downloads: List[Download] = field(default_factory=list)


# =============================================================================
# Operations
# =============================================================================


class PlatformFactory:
    """Provides methods for creating platform objects."""

    @staticmethod
    def _get_current_python_version() -> List[str]:
        # pylint: disable=import-outside-toplevel
        import platform

        return list(platform.python_version_tuple())

    @staticmethod
    def _get_current_python_tags() -> List[str]:
        try:
            # pylint: disable=import-outside-toplevel
            import packaging.tags

            tags = [str(tag) for tag in packaging.tags.sys_tags()]
            return tags
        except ModuleNotFoundError as e:
            logger.warning(
                'Error trying to import the "packaging" package.', exc_info=e
            )
            return []

    @classmethod
    def from_current_interpreter(cls) -> Platform:
        """
        Returns a platform object that describes the current interpreter and system.

        This method requires the ``packaging`` package to be installed
        because functionality from this package is used to generate the list of
        tags that are supported by the current platform.

        The platform object returned by this method obviously depends on the
        specific python interpreter and system architecture used to run the
        req2flatpak script. The reason why is that this method reads platform
        properties from the current interpreter and system.
        """
        return Platform(
            python_version=cls._get_current_python_version(),
            python_tags=cls._get_current_python_tags(),
        )

    @classmethod
    def from_string(cls, platform_string: str) -> Optional[Platform]:
        """
        Returns a platform object by parsing a platform string.

        :param platform_string: A string specifying python version and system
          architecture. The string format is
          "{python_version}-{system_architecture}".
          For example: "cp39-x86_64" or "cp310-aarch64".
          Acceptable values are the same as in
          :py:meth:`~req2flatpak.PlatformFactory.from_python_version_and_arch`.
        """
        try:
            _, minor, arch = re.match(  # type: ignore[union-attr]
                r"^(?:py|cp)?(\d)(\d+)-(.*)$", platform_string
            ).groups()
            return cls.from_python_version_and_arch(minor_version=int(minor), arch=arch)
        except AttributeError:
            logger.warning("Could not parse platform string %s", platform_string)
            return None

    @classmethod
    def from_python_version_and_arch(
        cls, minor_version: Optional[int] = None, arch="x86_64"
    ) -> Platform:
        """
        Returns a platform object that roughly describes a cpython installation on linux.

        The tags in the platform object are a rough approximation, trying to match what
        `packaging.tags.sys_tags` would return if invoked on a linux system with cpython.
        No guarantees are made about how closely this approximation matches a real system.

        :param minor_version: the python 3 minor version, specified as int.
          Defaults to the current python version.
        :param arch: either "x86_64" or "aarch64".
        """
        if not minor_version:
            minor_version = int(cls._get_current_python_version()[1])
        assert arch in ["x86_64", "aarch64"]
        return Platform(
            python_version=["3", str(minor_version)],
            python_tags=list(
                cls._cp3_linux_tags(minor_version=minor_version, arch=arch)
            ),
        )

    @classmethod
    def _cp3_linux_tags(
        cls, minor_version: Optional[int] = None, arch="x86_64"
    ) -> Generator[str, None, None]:
        """Yields python platform tags for cpython3 on linux."""
        # pylint: disable=too-many-branches

        assert minor_version is not None
        assert arch in ["x86_64", "aarch64"]

        def seq(start: int, end: int) -> Iterable[int]:
            """Returns a range of numbers, from start to end, in steps of +/- 1."""
            step = 1 if start < end else -1
            return range(start, end + step, step)

        cache = set()

        def dedup(obj: Hashable):
            if obj in cache:
                return None
            cache.add(obj)
            return obj

        platforms = [f"manylinux_2_{v}" for v in seq(35, 17)] + ["manylinux2014"]
        if arch == "x86_64":
            platforms += (
                [f"manylinux_2_{v}" for v in seq(16, 12)]
                + ["manylinux2010"]
                + [f"manylinux_2_{v}" for v in seq(11, 5)]
                + ["manylinux1"]
            )
        platforms += ["linux"]
        platform_tags = [f"{platform}_{arch}" for platform in platforms]

        # current cpython version, all abis, all platforms:
        for py in [f"cp3{minor_version}"]:
            for abi in [f"cp3{minor_version}", "abi3", "none"]:
                for platform in platform_tags:
                    yield dedup(f"{py}-{abi}-{platform}")

        # older cpython versions, abi3, all platforms:
        for py in [f"cp3{v}" for v in seq(minor_version - 1, 2)]:
            for abi in ["abi3"]:
                for platform in platform_tags:
                    yield dedup(f"{py}-{abi}-{platform}")

        # current python version, abi=none, all platforms:
        for py in [f"py3{minor_version}"]:
            for abi in ["none"]:
                for platform in platform_tags:
                    yield dedup(f"{py}-{abi}-{platform}")

        # current major python version (py3), abi=none, all platforms:
        for py in ["py3"]:
            for abi in ["none"]:
                for platform in platform_tags:
                    yield dedup(f"{py}-{abi}-{platform}")

        # older python versions, abi=none, all platforms:
        for py in [f"py3{v}" for v in seq(minor_version - 1, 0)]:
            for abi in ["none"]:
                for platform in platform_tags:
                    yield dedup(f"{py}-{abi}-{platform}")

        # current python version, abi=none, platform=any
        yield f"py3{minor_version}-none-any"

        # current major python version, abi=none, platform=any
        yield "py3-none-any"

        # older python versions, abi=none, platform=any
        for py in [f"py3{v}" for v in seq(minor_version - 1, 0)]:
            yield f"{py}-none-any"


class RequirementsParser:
    """
    Parses requirements.txt files in a very simple way.

    This methods expects all versions to be pinned, and it does not
    resolve dependencies.
    """

    # based on: https://stackoverflow.com/a/59971236
    # using functionality from pkg_resources.parse_requirements

    @classmethod
    def parse_string(cls, requirements_txt: str) -> List[Requirement]:
        """Parses requirements.txt string content into a list of Requirement objects."""

        def validate_requirement(req: pkg_resources.Requirement) -> None:
            assert (
                len(req.specs) == 1
            ), "Error parsing requirements: A single version number must be specified."
            assert (
                req.specs[0][0] == "=="
            ), "Error parsing requirements: The exact version must specified as 'package==version'."

        def make_requirement(req: pkg_resources.Requirement) -> Requirement:
            validate_requirement(req)
            return Requirement(package=req.project_name, version=req.specs[0][1])

        reqs = pkg_resources.parse_requirements(requirements_txt)
        return [make_requirement(req) for req in reqs]

    @classmethod
    def parse_file(cls, file) -> List[Requirement]:
        """Parses a requirements.txt file into a list of Requirement objects."""
        if hasattr(file, "read"):
            req_txt = file.read()
        else:
            req_txt = pathlib.Path(file).read_text(encoding="utf-8")
        return cls.parse_string(req_txt)


# Cache typealias
# This is meant for caching responses when querying package information.
# A cache can either be a dict for in-memory caching, or a shelve.Shelf
Cache = Union[dict, shelve.Shelf]


class PypiClient:
    """Queries package information from the PyPi package index."""

    cache: Cache = {}
    """A dict-like object for caching responses from PyPi."""

    @classmethod
    def _query_from_cache(cls, url) -> Optional[str]:
        try:
            return cls.cache[url]
        except KeyError:
            return None

    @classmethod
    def _query_from_pypi(cls, url) -> str:
        # url scheme might be `file:/`
        if not urlparse(url).scheme == "https":
            raise ValueError("URL scheme not `https`.")
        with urllib.request.urlopen(url) as response:  # nosec: B310
            json_string = response.read().decode("utf-8")
        cls.cache[url] = json_string
        return json_string

    @classmethod
    def _query(cls, url) -> str:
        return cls._query_from_cache(url) or cls._query_from_pypi(url)

    @classmethod
    def get_release(cls, req: Requirement) -> Release:
        """Queries pypi regarding available releases for this requirement."""
        url = f"https://pypi.org/pypi/{req.package}/{req.version}/json"
        json_string = cls._query(url)
        json_dict = json.loads(json_string)
        return Release(
            package=req.package,
            version=req.version,
            downloads=[
                Download(
                    package=req.package,
                    version=req.version,
                    filename=url["filename"],
                    url=url["url"],
                    sha256=url["digests"]["sha256"],
                )
                for url in json_dict["urls"]
            ],
        )

    @classmethod
    def get_releases(cls, reqs: Iterable[Requirement]) -> List[Release]:
        """Queries pypi regarding available releases for these requirements."""
        return [cls.get_release(req) for req in reqs]


class DownloadChooser:
    """
    Provides methods for choosing package downloads.

    This class implements logic for filtering wheel and sdist downloads
    that are compatible with a given target platform.
    """

    @classmethod
    def matches(cls, download: Download, platform_tag: str) -> bool:
        """Returns whether a download is compatible with a target platform tag."""
        if download.is_sdist:
            return True

        return platform_tag in (download.tags or [])

    @classmethod
    def downloads(
        cls,
        release: Release,
        platform: Platform,
        wheels_only=False,
        sdists_only=False,
    ) -> Iterator[Download]:
        """
        Yields suitable downloads for a specific platform.

        The order of downloads matches the order of platform tags, i.e.,
        preferred downloads are returned first.
        """
        cache = set()
        for platform_tag, download in product(platform.python_tags, release.downloads):
            if download in cache:
                continue
            if wheels_only and not download.is_wheel:
                continue
            if sdists_only and not download.is_sdist:
                continue
            if cls.matches(download, platform_tag):
                cache.add(download)
                yield download

    @classmethod
    def wheel(
        cls,
        release: Release,
        platform: Platform,
    ) -> Optional[Download]:
        """Returns the preferred wheel download for this release."""
        try:
            return next(cls.downloads(release, platform, wheels_only=True))
        except StopIteration:
            return None

    @classmethod
    def sdist(cls, release: Release) -> Optional[Download]:
        """Returns the source package download for this release."""
        try:
            return next(filter(lambda d: d.is_sdist, release.downloads))
        except StopIteration:
            return None

    @classmethod
    def wheel_or_sdist(cls, release: Release, platform: Platform) -> Optional[Download]:
        """Returns a wheel or an sdist for this release, in this order of preference."""
        return cls.wheel(release, platform) or cls.sdist(release)

    @classmethod
    def sdist_or_wheel(cls, release: Release, platform: Platform) -> Optional[Download]:
        """Returns an sdist or a wheel for this release, in this order of preference."""
        return cls.sdist(release) or cls.wheel(release, platform)


class FlatpakGenerator:
    """Provides methods for generating a flatpak-builder build module."""

    @staticmethod
    def build_module(
        requirements: Iterable[Requirement],
        downloads: Iterable[Download],
        module_name="python3-package-installation",
        pip_install_template: str = "pip3 install --verbose --exists-action=i "
        '--no-index --find-links="file://${PWD}" '
        "--prefix=${FLATPAK_DEST} --no-build-isolation ",
    ) -> dict:
        """Generates a build module for inclusion in a flatpak-builder build manifest."""

        def source(download: Download) -> dict:
            source: Dict[str, Any] = {
                "type": "file",
                "url": download.url,
                "sha256": download.sha256,
            }
            if download.arch:
                source["only-arches"] = [download.arch]
            return source

        def sources(downloads: Iterable[Download]) -> List[dict]:
            return [source(download) for download in sorted(downloads)]

        return {
            "name": module_name,
            "buildsystem": "simple",
            "build-commands": [
                pip_install_template + " ".join([req.package for req in requirements])
            ],
            "sources": sources(downloads),
        }

    @classmethod
    def build_module_as_str(cls, *args, **kwargs) -> str:
        """
        Generate JSON build module for inclusion in a flatpak-builder build manifest.

        The args and kwargs are the same as in
        :py:meth:`~req2flatpak.FlatpakGenerator.build_module`
        """
        return json.dumps(cls.build_module(*args, **kwargs), indent=4)

    @classmethod
    def build_module_as_yaml_str(cls, *args, **kwargs) -> str:
        """
        Generate YAML build module for inclusion in a flatpak-builder build manifest.

        The args and kwargs are the same as in
        :py:meth:`~req2flatpak.FlatpakGenerator.build_module`
        """
        # optional dependency, not imported at top
        if not yaml:
            raise ImportError(
                "Package `pyyaml` has to be installed for the yaml format."
            )

        return yaml.dump(
            cls.build_module(*args, **kwargs), default_flow_style=False, sort_keys=False
        )


# =============================================================================
# CLI commandline interface
# =============================================================================


def cli_parser() -> argparse.ArgumentParser:
    """Returns the req2flatpak commandline interface parser."""
    parser = argparse.ArgumentParser(
        description="req2flatpak generates a flatpak-builder build module for installing required python packages."
    )
    parser.add_argument(
        "--requirements",
        nargs="*",
        help="One or more requirements can be specified as commandline arguments, e.g., 'pandas==1.4.4'.",
    )
    parser.add_argument(
        "--requirements-file",
        "-r",
        nargs="?",
        type=argparse.FileType("r"),
        help="Requirements can be read from a specified requirements.txt file.",
    )
    parser.add_argument(
        "--target-platforms",
        "-t",
        nargs="+",
        help="Target platforms can be specified as, e.g., '39-x86_64' or '310-aarch64'.",
    )
    parser.add_argument(
        "--cache",
        action="store_true",
        default=False,
        help="Uses a persistent cache when querying pypi.",
    )
    parser.add_argument(
        "--yaml",
        action="store_true",
        help="Write YAML instead of the default JSON.  Needs the 'pyyaml' package.",
    )

    parser.add_argument(
        "--outfile",
        "-o",
        nargs="?",
        type=argparse.FileType("w"),
        default=sys.stdout,
        help="""
            By default, writes JSON but specify a '.yaml' extension and YAML
            will be written instead, provided you have the 'pyyaml' package.
        """,
    )
    parser.add_argument(
        "--platform-info",
        action="store_true",
        default=False,
        help="Prints information about the current platform.",
    )
    parser.add_argument(
        "--installed-packages",
        action="store_true",
        default=False,
        help="Prints installed packages in requirements.txt format.",
    )
    return parser


def main():  # pylint: disable=too-many-branches
    """Main function that provides req2flatpak's commandline interface."""

    # process commandline arguments
    parser = cli_parser()
    options = parser.parse_args()

    # stream output to a file or to stdout
    if hasattr(options.outfile, "write"):
        output_stream = options.outfile
        if pathlib.Path(output_stream.name).suffix.casefold() in (".yaml", ".yml"):
            options.yaml = True
    else:
        output_stream = sys.stdout

    if options.yaml and not yaml:
        parser.error(
            "Outputing YAML requires 'pyyaml' package: try 'pip install pyyaml'"
        )

    # print platform info if requested, and exit
    if options.platform_info:
        info = asdict(PlatformFactory.from_current_interpreter())
        if options.yaml:
            yaml.dump(info, output_stream, default_flow_style=False, sort_keys=False)
        else:
            json.dump(info, output_stream, indent=4)
        parser.exit()

    # print installed packages if requested, and exit
    if options.installed_packages:
        # pylint: disable=not-an-iterable
        pkgs = {p.key: p.version for p in pkg_resources.working_set}
        for pkg, version in pkgs.items():
            print(f"{pkg}=={version}", file=output_stream)
        parser.exit()

    # parse requirements
    requirements = []
    with suppress(AttributeError):
        if options.requirements:
            requirements += RequirementsParser.parse_string(
                "\n".join(options.requirements)
            )
        if options.requirements_file:
            requirements += RequirementsParser.parse_file(options.requirements_file)
    if not requirements:
        parser.error(
            "Error parsing requirements: At least one requirement must be specified."
        )

    # parse target platforms
    if not options.target_platforms:
        parser.error(
            "Error parsing target platforms. "
            "Missing commandline argument, at least one target platform must "
            "be specified as, e.g., '39-x86_64' or '310-aarch64'."
        )
    platforms = [
        PlatformFactory.from_string(platform) for platform in options.target_platforms
    ]
    if not platforms:
        parser.error(
            "Error parsing target platforms. "
            "At least one target platform must be specified "
            "as, e.g., '39-x86_64' or '310-aarch64'."
        )

    # query released downloads from PyPi, optionally using a shelve.Shelf to cache responses:
    with (
        shelve.open("pypi_cache") if options.cache else nullcontext()  # nosec: B301
    ) as cache:
        PypiClient.cache = cache or {}
        releases = PypiClient.get_releases(requirements)

    # choose suitable downloads for the target platforms
    downloads = {
        DownloadChooser.wheel_or_sdist(release, platform)
        for release in releases
        for platform in platforms
        if platform
    }

    # generate flatpak-builder build module
    if options.yaml:
        try:
            name = pathlib.Path(__file__).name
        except NameError:
            name = "req2flatpak"
        output_stream.write(f"# Generated by {name} {' '.join(sys.argv[1:])}\n")
        output_stream.write(
            FlatpakGenerator.build_module_as_yaml_str(requirements, downloads)
        )
    else:
        # write json
        output_stream.write(
            FlatpakGenerator.build_module_as_str(requirements, downloads)
        )


if __name__ == "__main__":
    main()
