#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
PHOTOS = ROOT / "apps/crazypod/crazypod_photos.c"
CACHE = ROOT / "apps/crazypod/photos/crazypod_photo_cache.c"
SYSTEM_PROMPTS = (
    ROOT / "apps/crazypod/ui/shell/crazypod_system_prompts.c"
)


def function_body(source: str, name: str) -> str:
    match = re.search(
        rf"\b{name}\s*\([^)]*\)\s*\{{(?P<body>.*?)^\}}",
        source,
        re.MULTILINE | re.DOTALL,
    )
    if match is None:
        raise SystemExit(f"missing function: {name}")
    return match.group("body")


def media_library_build(source: str) -> str:
    """The half of crazypod_photos.c that has a photo library in it.

    The file opens with an empty implementation for the panels that do not
    build one -- the Mini has no Media app -- and the real one follows. A
    search from the top of the file finds the empty version and concludes
    the policy below has been dropped, which is the opposite of true.
    """
    marker = "#elif defined(HAVE_CRAZYPOD_UI)"
    index = source.find(marker)
    if index < 0:
        raise SystemExit(
            f"{PHOTOS} no longer has a media-library build to check")
    return source[index:]


photos_source = media_library_build(PHOTOS.read_text(encoding="utf-8"))
cache_source = CACHE.read_text(encoding="utf-8")
prompts_source = SYSTEM_PROMPTS.read_text(encoding="utf-8")

invalidate_body = function_body(
    photos_source, "crazypod_photos_invalidate_catalog"
)
if "crazypod_photo_catalog_invalidate();" not in invalidate_body:
    raise SystemExit("photo catalog invalidation no longer refreshes the catalog")
if "crazypod_photo_cache_invalidate" in invalidate_body:
    raise SystemExit("USB catalog invalidation must preserve decoded photo caches")
if "crazypod_photos_invalidate_catalog();" not in prompts_source:
    raise SystemExit("USB connection no longer invalidates the photo catalog")

load_body = function_body(cache_source, "crazypod_photo_cache_load")
for identity_check in (
    "entry->key != key",
    "entry->source_size != source_size",
    "entry->source_mtime != source_mtime",
):
    if identity_check not in load_body:
        raise SystemExit(
            "retained photo caches require source identity check: "
            f"{identity_check}"
        )

print("Photo USB refresh preserves source-validated decoded caches")
