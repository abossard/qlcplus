"""The Library: the analysed tracks inside the scope the reviewer chose, or all of them when there is none.

<out>/library.json is {"include": [glob, ...]}. A track is in the Library when a pattern matches its file or
one of its parent folders. Analyses store the resolved path of a track, so the folders of a pattern before its
first wildcard are resolved through symlinks once, when the scope is made; matching is then plain string work.
A scope is None (everything) or {"include": the patterns as given, "match": the patterns to match against}.
"""

import json
import os
import re
from fnmatch import fnmatchcase
from pathlib import PurePosixPath

SCOPE_FILE = "library.json"
WILDCARD = re.compile(r"[*?\[]")


def resolve_pattern(pattern, resolve=os.path.realpath):
    """The pattern with the folders before its first wildcard resolved through symlinks."""
    pattern = pattern.rstrip("/") or "/"
    wild = WILDCARD.search(pattern)
    if not wild:
        return resolve(pattern)
    folder, _, rest = pattern[:wild.start()].rpartition("/")
    return f"{resolve(folder)}/{rest}{pattern[wild.start():]}" if folder else pattern


def make_scope(include, resolve=os.path.realpath):
    """None for no patterns: everything is in the Library."""
    if not include:
        return None
    return {"include": list(include), "match": list(dict.fromkeys(p for i in include for p in (resolve_pattern(i, resolve), i.rstrip("/"))))}


def in_library(path, scope):
    """Is this track file (as stored in its analysis) inside the scope?"""
    if scope is None:
        return True
    places = (path, *map(str, PurePosixPath(path).parents))
    return any(fnmatchcase(place, pattern) for pattern in scope["match"] for place in places)


def load_scope(out_dir):
    path = os.path.join(out_dir, SCOPE_FILE)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return make_scope(json.load(f).get("include", []))


def set_scope(out_dir, include):
    with open(os.path.join(out_dir, SCOPE_FILE), "w") as f:
        json.dump({"include": list(include)}, f, indent=1)


def clear_scope(out_dir):
    try:
        os.remove(os.path.join(out_dir, SCOPE_FILE))
    except FileNotFoundError:
        pass
