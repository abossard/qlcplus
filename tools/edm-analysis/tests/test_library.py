import json
import os
from pathlib import Path

import pytest

from edm_analysis.library import clear_scope, in_library, load_scope, make_scope, resolve_pattern, set_scope

SAME = lambda p: p
LINKS = lambda p: p.replace("/Users/me/OneDrive", "/Users/me/Library/CloudStorage/OneDrive-Personal")


@pytest.mark.parametrize("include,path,inside", [
    (["/m/ZZ/2026*"], "/m/ZZ/2026-11 mix/a.mp3", True),
    (["/m/ZZ/2026*"], "/m/ZZ/2026-11 mix/disc 1/a.mp3", True),          # any depth below a matching folder
    (["/m/ZZ/2026*"], "/m/ZZ/2025-11 mix/a.mp3", False),
    (["/m/ZZ/2026*"], "/m/ZZ/a.mp3", False),
    (["/m/ZZ/2026*"], "/m/Other/2026-11/a.mp3", False),
    (["/m/ZZ/2026*"], "/m/zz/2026-11/a.mp3", False),                      # paths are case sensitive
    (["/m/ZZ/2026*"], "/m/ZZ/2026-11", True),                             # a folder itself
    (["/m/x/a.mp3"], "/m/x/a.mp3", True),                                 # a file pattern
    (["/m/x/a.mp3"], "/m/x/b.mp3", False),
    (["/m/x"], "/m/x/y/a.mp3", True),                                     # a folder pattern
    (["/m/x/"], "/m/x/a.mp3", True),
    (["/m/x"], "/m/xy/a.mp3", False),
    (["/m/x", "/m/ZZ/2026*"], "/m/ZZ/2026-1/a.mp3", True),                # patterns add up
    (["/m/x", "/m/ZZ/2026*"], "/m/y/a.mp3", False),
    (["/m/*/2026*"], "/m/ZZ/2026-1/a.mp3", True),
    (["/m/ZZ/2026-0?"], "/m/ZZ/2026-01/a.mp3", True),
    (["/m/ZZ/2026-0?"], "/m/ZZ/2026-11/a.mp3", False),
])
def test_a_track_is_in_the_library_when_a_pattern_matches_its_file_or_a_parent_folder(include, path, inside):
    assert in_library(path, make_scope(include, SAME)) is inside


@pytest.mark.parametrize("scope", [None, make_scope([], SAME)])
def test_without_a_scope_every_track_is_in(scope):
    assert in_library("/anywhere/a.mp3", scope) is True


@pytest.mark.parametrize("pattern,resolved", [
    ("/Users/me/OneDrive/Music/ZZ/2026*", "/Users/me/Library/CloudStorage/OneDrive-Personal/Music/ZZ/2026*"),
    ("/Users/me/OneDrive/Music/ZZ/", "/Users/me/Library/CloudStorage/OneDrive-Personal/Music/ZZ"),
    ("/Users/me/OneDrive/Music/a.mp3", "/Users/me/Library/CloudStorage/OneDrive-Personal/Music/a.mp3"),
    ("/Users/me/OneDrive/*/2026*", "/Users/me/Library/CloudStorage/OneDrive-Personal/*/2026*"),
    ("*/2026*", "*/2026*"),
    ("/*/2026*", "/*/2026*"),
])
def test_the_folders_before_the_first_wildcard_are_resolved_through_symlinks(pattern, resolved):
    assert resolve_pattern(pattern, LINKS) == resolved


@pytest.mark.parametrize("path,inside", [
    ("/Users/me/Library/CloudStorage/OneDrive-Personal/Music/ZZ/2026-11/a.mp3", True),   # the stored, resolved path
    ("/Users/me/OneDrive/Music/ZZ/2026-11/a.mp3", True),                                 # as the pattern was typed
    ("/Users/me/Library/CloudStorage/OneDrive-Personal/Music/ZZ/2025-11/a.mp3", False),
])
def test_a_pattern_matches_paths_through_the_link_and_through_its_target(path, inside):
    assert in_library(path, make_scope(["/Users/me/OneDrive/Music/ZZ/2026*"], LINKS)) is inside


def test_resolution_follows_real_symlinks(tmp_path):
    real = tmp_path.resolve() / "cloud" / "Music" / "ZZ" / "2026-a"
    real.mkdir(parents=True)
    os.symlink(tmp_path.resolve() / "cloud", tmp_path.resolve() / "link")
    scope = make_scope([f"{tmp_path.resolve()}/link/Music/ZZ/2026*"], os.path.realpath)
    assert in_library(str(real / "a.mp3"), scope) and not in_library(str(real.parent / "2025-b" / "a.mp3"), scope)


def test_the_scope_is_stored_in_the_library_folder_and_can_be_cleared(tmp_path):
    assert load_scope(tmp_path) is None
    set_scope(tmp_path, ["/m/ZZ/2026*", "/m/x"])
    assert json.loads((tmp_path / "library.json").read_text()) == {"include": ["/m/ZZ/2026*", "/m/x"]}
    scope = load_scope(tmp_path)
    assert scope["include"] == ["/m/ZZ/2026*", "/m/x"] and in_library("/m/x/a.mp3", scope) and not in_library("/m/y/a.mp3", scope)
    clear_scope(tmp_path)
    clear_scope(tmp_path)
    assert load_scope(tmp_path) is None and not (tmp_path / "library.json").exists()


# --- where the Library is loaded ---

A, B = "aaaaaaaaaaaaaaaa", "bbbbbbbbbbbbbbbb"
PATHS = {A: "/m/ZZ/2026-11/a.mp3", B: "/m/ZZ/2025-01/b.mp3"}
N = 32


def doc(tid):
    return {"track": {"path": PATHS[tid], "title": f"song {tid[0]}", "sha256": tid * 4}, "analyzer": {"version": "2.5"},
            "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0, "confidence": 0.9},
            "beats": [0.5 * k for k in range(4 * N)],
            "bars": {"start": [2.0 * k for k in range(N)],
                     "levels": {"kick": [0.0] * 16 + [1.0] * 16, "bass": [0.0] * 16 + [0.9] * 16, "hihat": [0.5] * N,
                                "instruments": [0.5] * N, "vocal": [0.0] * N, "loud": [0.3] * 16 + [0.9] * 16}},
            "raw": {"downbeats": [2.0 * k for k in range(N)]},
            "sections": [{"label": "intro", "start_bar": 0, "end_bar": 16, "start": 0.0, "end": 32.0},
                         {"label": "drop", "start_bar": 16, "end_bar": 32, "start": 32.0, "end": 64.0}],
            "events": [{"kind": "drop", "bar": 16, "time": 32.0}]}


@pytest.fixture
def library(tmp_path, monkeypatch):
    monkeypatch.setattr("edm_analysis.vdj.DEFAULT_DB", tmp_path / "no-database.xml")
    for tid in PATHS:
        (tmp_path / f"{tid}.json").write_text(json.dumps(doc(tid)))
    (tmp_path / "index.json").write_text(json.dumps({tid: {"path": p} for tid, p in PATHS.items()}))
    set_scope(tmp_path, ["/m/ZZ/2026*"])
    return tmp_path


@pytest.mark.parametrize("scoped,tids", [(True, [A]), (False, [A, B])])
def test_the_library_docs_are_the_analyses_inside_the_scope(library, scoped, tids):
    from edm_analysis.evaluate import library_docs

    assert sorted(library_docs(library, scoped=scoped)) == tids


def test_without_a_scope_file_the_library_is_every_analysis(library):
    from edm_analysis.evaluate import library_docs

    clear_scope(library)
    assert sorted(library_docs(library)) == [A, B]


def test_evaluation_scores_only_the_library(library, monkeypatch):
    from edm_analysis import evaluate

    monkeypatch.setattr(evaluate, "score_track", lambda d, vdj, golds: d["track"]["title"])
    monkeypatch.setattr(evaluate, "summarize_scores", lambda rows: None)
    assert evaluate.evaluate_library(library, {})[0] == ["song a"]


def test_a_round_asks_only_about_tracks_in_the_library(library):
    from edm_analysis.cli import main

    assert main(["round", "new", "--out", str(library), "--size", "10"]) == 0
    probes = json.loads((library / "rounds" / "round-01.json").read_text())["probes"]
    assert probes and {p["track"] for p in probes} == {A}


def test_the_export_covers_only_the_library(library):
    import pyarrow.parquet as pq

    from edm_analysis.export import export

    folder = export(library, {}, log=lambda m: None)
    assert {t["track"] for t in pq.read_table(folder / "tracks.parquet").to_pylist()} == {A}


def test_answers_on_tracks_outside_the_library_still_train_the_confidence(library, monkeypatch):
    from edm_analysis import confidence, final
    from edm_analysis.cli import main

    claim = {"kind": "drop", "bar": 16, "time": 32.0, "confidence": 50}
    (library / "rounds").mkdir()
    (library / "rounds" / "answers.json").write_text(json.dumps(
        {f"r01-p00{i}": {"answer": "yes", "round": 1, "track": t, "claim": claim} for i, t in enumerate((A, B), 1)}))
    fitted, written = [], []
    monkeypatch.setattr(confidence, "fit", lambda silver, gold: fitted.append(gold) or {"fitted": "2026-11-12", "groups": {}})
    monkeypatch.setattr(final, "write_final", lambda out, d: written.append(d["track"]["title"]))

    assert main(["confidence", "--out", str(library)]) == 0

    assert len(fitted[0]) == 2           # the model is fitted on the Gold of both tracks
    assert written == ["song a"]         # final files of tracks outside the Library stay as they are


@pytest.mark.parametrize("action,patterns,said", [
    ("show", [], ["/m/ZZ/2026*", "2 analysed tracks: 1 in the Library, 1 out"]),
    ("set", ["/m/ZZ/2025*"], ["/m/ZZ/2025*", "2 analysed tracks: 1 in the Library, 1 out"]),
    ("set", ["/m/ZZ/*"], ["2 analysed tracks: 2 in the Library, 0 out"]),
    ("clear", [], ["every analysed track is in the Library", "2 analysed tracks: 2 in the Library, 0 out"]),
])
def test_the_library_command_shows_and_changes_the_scope(library, capsys, action, patterns, said):
    from edm_analysis.cli import main

    assert main(["library", action, *patterns, "--out", str(library)]) == 0
    out = capsys.readouterr().out
    assert all(s in out for s in said)
    assert (action != "clear") or not (library / "library.json").exists()


def test_the_library_command_needs_a_pattern_to_set(library, capsys):
    from edm_analysis.cli import main

    assert main(["library", "set", "--out", str(library)]) == 1
    assert load_scope(library)["include"] == ["/m/ZZ/2026*"]


def test_candidates_run_on_the_tracks_of_the_library_first(tmp_path, monkeypatch):
    import subprocess
    from types import SimpleNamespace

    from edm_analysis import candidates

    music = tmp_path / "music"
    music.mkdir()
    tracks = {f"{i:016x}": music / f"{'ZZ' if i == 2 else 'XX'}-song{i}.mp3" for i in range(3)}
    for p in tracks.values():
        p.write_bytes(b"")
    (tmp_path / "index.json").write_text(json.dumps({tid: {"path": str(p)} for tid, p in tracks.items()}))
    set_scope(tmp_path, [f"{music}/ZZ*"])
    monkeypatch.setattr(candidates, "CANDIDATES_HOME", tmp_path / "home")
    called = []
    monkeypatch.setattr(subprocess, "run", lambda cmd, **kw: called.append([Path(c).name for c in cmd if str(c).endswith(".mp3")])
                        or SimpleNamespace(returncode=0))

    candidates.run(tmp_path, "edmformer", log=lambda m: None)

    assert called[0][0] == f"{list(tracks)[2]}.mp3" and len(called[0]) == 3
