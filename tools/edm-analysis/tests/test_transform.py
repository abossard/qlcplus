import copy

import pytest

from edm_analysis.transform import (answer_rows, bar_rows, candidate_drop_bars, claims_candidate, claims_ours, claims_silver,
                                    snap, track_row)

TID = "abababababababab"
N = 32
DOC = {"track": {"path": "/m/a.mp3", "title": "a", "sha256": TID * 4, "duration": 70.0}, "analyzer": {"version": "2.5"},
       "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 1.0, "confidence": 0.9, "bar1_confidence": 88},
       "beats": [1.0 + 0.5 * k for k in range(4 * N)],
       "bars": {"start": [1.0 + 2.0 * k for k in range(N)],
                "levels": {"kick": [0.0] * 16 + [1.0] * 16, "bass": [0.0] * 16 + [0.9] * 16, "hihat": [0.5] * N,
                           "instruments": [0.5] * N, "vocal": [0.0] * N, "loud": [0.3] * 16 + [0.9] * 16},
                "descriptors": {"centroid": [0.4] * N, "onset": [0.2] * N, "mfcc": [[0.1] * 13] * N}},
       "sections": [{"label": "intro", "start_bar": 0, "end_bar": 12, "confidence": 100},
                    {"label": "buildup", "start_bar": 12, "end_bar": 16, "confidence": 40},
                    {"label": "drop", "start_bar": 16, "end_bar": 32, "confidence": 70}],
       "events": []}


@pytest.mark.parametrize("t,bar,offset", [(1.0, 0, 0.0), (33.0, 16, 0.0), (33.5, 16, 1.0), (31.5, 15, 1.0), (32.5, 16, -1.0), (0.0, 0, -2.0)])
def test_snap_places_any_time_on_our_grid(t, bar, offset):
    assert snap(DOC, t) == (bar, offset)


def test_our_claims_carry_our_confidence():
    rows = claims_ours(TID, DOC)
    assert [(r["kind"], r["label"], r["bar"], r["score"]) for r in rows] == [
        ("bar1", None, 0, 88), ("section", "intro", 0, 100), ("section", "buildup", 12, 40), ("buildup", "buildup", 12, 40),
        ("section", "drop", 16, 70), ("drop", "drop", 16, 70)]


@pytest.mark.parametrize("name,loaded,expected", [
    ("allin1", {"downbeats": [1.5, 3.5], "sections": [{"start": 0.0, "label": "intro"}, {"start": 33.2, "label": "chorus"}], "drops": []},
     [("bar1", None, 0, 1.0), ("section", "intro", 0, -2.0), ("section", "chorus", 16, 0.4)]),
    ("edmformer", {"downbeats": [], "sections": [{"start": 0.0, "label": "intro"}, {"start": 32.9, "label": "drop"}],
                   "drops": [{"time": 32.9, "confidence": None}]},
     [("section", "intro", 0, -2.0), ("section", "drop", 16, -0.2), ("drop", "drop", 16, -0.2)]),
    ("dropdetector", {"downbeats": [], "sections": [], "drops": [{"time": 33.1, "confidence": 0.97}, {"time": 50.0, "confidence": 0.1}]},
     [("drop", "drop", 16, 0.2), ("drop", "drop", 24, 2.0)]),
])
def test_candidate_claims_land_on_our_bars(name, loaded, expected):
    rows = claims_candidate(TID, DOC, name, loaded)
    assert [(r["kind"], r["label"], r["bar"], r["offset_beats"]) for r in rows] == expected
    assert {r["source"] for r in rows} == {name}


def test_silver_claims_from_virtualdj_and_mixed_in_key():
    vdj = {"period": 0.5, "phase": 1.5, "breaks": [(23.0, 33.0)], "cues": [{"name": "Energy 6", "pos": 33.049}, {"name": "Cue 1", "pos": 5.0}]}
    rows = claims_silver(TID, DOC, vdj)
    assert [(r["source"], r["kind"], r["label"], r["bar"], r["offset_beats"]) for r in rows] == [
        ("vdj", "bar1", None, 0, 1.0), ("vdj", "break_start", None, 11, 1.0), ("vdj", "break_end", None, 16, 0.0),
        ("mik", "energy", "E6", 16, 0.0)]


def test_no_silver_without_a_virtualdj_grid():
    assert claims_silver(TID, DOC, {"period": None}) == [] and claims_silver(TID, DOC, None) == []


ANSWERS = {"r01-p002": {"answer": "b", "round": 1, "track": TID, "claim": {"kind": "drop", "bar": 16, "time": 33.0, "confidence": 70},
                        "alternatives": {"a": {"source": "ours"}, "b": {"source": "vdj"}}, "at": "2026-11-05T10:00:00"},
           "r01-p001": {"answer": "yes", "round": 1, "track": TID, "claim": {"kind": "buildup", "bar": 12, "time": 25.0, "confidence": 40}},
           "r01-p003": {"answer": "skip", "round": 1, "track": TID, "claim": {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 33.0}}}


def test_answers_become_rows_with_their_judgement():
    rows = answer_rows(ANSWERS, {TID: DOC})
    assert [(r["probe"], r["kind"], r["bar"], r["exact"]) for r in rows] == [
        ("r01-p001", "buildup", 12, True), ("r01-p002", "drop", 16, False), ("r01-p003", "label", 16, None)]


def test_answer_rows_carry_the_gold_bar():
    located = {"answer": "bar:18", "round": 2, "track": TID, "claim": {"kind": "drop", "bar": 16, "time": 33.0, "confidence": 70},
               "alternatives": {"ours": {"bar": 16}, "edmformer": {"bar": 18}}}
    rows = answer_rows({**ANSWERS, "r02-p001": located}, {TID: DOC})
    assert [(r["probe"], r["gold_bar"]) for r in rows] == [("r01-p001", 12), ("r01-p002", None), ("r01-p003", None), ("r02-p001", 18)]


@pytest.mark.parametrize("name,loaded,bars", [
    ("dropdetector", {"downbeats": [], "sections": [], "drops": [{"time": 33.1, "confidence": 0.97}, {"time": 50.0, "confidence": 0.1}]}, [16]),
    ("dropdetector", {"downbeats": [], "sections": [], "drops": [{"time": 33.1, "confidence": 0.5}, {"time": 41.0, "confidence": 0.49}]}, [16]),
    ("dropdetector", {"downbeats": [], "sections": [], "drops": [{"time": 41.0, "confidence": 0.9}, {"time": 33.1, "confidence": 0.9}, {"time": 33.9, "confidence": 0.9}]}, [16, 20]),
    ("edmformer", {"downbeats": [], "sections": [{"start": 0.0, "label": "intro"}, {"start": 32.9, "label": "drop"}], "drops": [{"time": 32.9, "confidence": None}]}, [16]),
    ("allin1", {"downbeats": [1.5], "sections": [{"start": 0.0, "label": "intro"}, {"start": 33.2, "label": "chorus"}], "drops": []}, []),
])
def test_candidate_drops_are_bars_on_our_grid_and_weak_ones_do_not_count(name, loaded, bars):
    assert candidate_drop_bars(TID, DOC, name, loaded) == bars


def test_the_round_asks_about_the_drops_of_every_candidate_that_has_output(tmp_path):
    import json

    from edm_analysis.cli import _candidate_drops

    for name, raw in (("dropdetector", {"drops": [{"time": 33.1, "confidence": 0.97}]}), ("allin1", {"segments": [], "downbeats": []})):
        (tmp_path / "candidates" / name).mkdir(parents=True)
        (tmp_path / "candidates" / name / f"{TID}.json").write_text(json.dumps(raw))
    assert _candidate_drops(tmp_path, {TID: DOC, "cdcdcdcdcdcdcdcd": DOC}) == {TID: {"dropdetector": [16], "allin1": []}}


def test_bar_rows_line_up_levels_sources_and_gold():
    claims = claims_ours(TID, DOC) + claims_candidate(TID, DOC, "dropdetector", {"downbeats": [], "sections": [], "drops": [{"time": 33.1, "confidence": 0.97}]})
    rows = bar_rows(TID, DOC, claims, answer_rows(ANSWERS, {TID: DOC}))
    assert len(rows) == N
    r = rows[16]
    assert (r["kick"], r["loud"], r["centroid"], r["mfcc_12"]) == (1.0, 0.9, 0.4, 0.1)
    assert (r["rules_start"], r["rules_label"], r["rules_drop"], r["dropdetector_drop"]) == (True, "drop", 70, 0.97)
    assert r["gold_drop"] is False and rows[12]["gold_buildup"] is True
    assert rows[13]["rules_label"] == "buildup" and "rules_start" not in rows[13]
    assert "gold_label" not in r            # 'can't tell' is no Gold


def test_transformers_do_not_touch_the_raw_documents():
    raw = copy.deepcopy(DOC)
    claims = claims_ours(TID, DOC)
    bar_rows(TID, DOC, claims)
    track_row(TID, DOC, {"rules"})
    assert DOC == raw


def test_export_rebuilds_the_tables_from_raw_outputs_only(tmp_path):
    import json

    import pyarrow.parquet as pq

    from edm_analysis.export import export

    doc = {**DOC, "sections": [{**s, "end": 0.0, "start": 0.0} for s in DOC["sections"]]}
    (tmp_path / f"{TID}.json").write_text(json.dumps(doc))
    (tmp_path / "candidates" / "dropdetector").mkdir(parents=True)
    (tmp_path / "candidates" / "dropdetector" / f"{TID}.json").write_text(json.dumps({"drops": [{"time": 33.1, "confidence": 0.97}]}))
    (tmp_path / "rounds").mkdir()
    (tmp_path / "rounds" / "answers.json").write_text(json.dumps({k: v for k, v in ANSWERS.items() if k == "r01-p001"}))
    before = {p: p.read_bytes() for p in tmp_path.rglob("*.json")}

    folder = export(tmp_path, {}, log=lambda m: None)

    assert {p.name for p in folder.glob("*.parquet")} == {"claims.parquet", "bars.parquet", "answers.parquet", "tracks.parquet"}
    claims = pq.read_table(folder / "claims.parquet").to_pylist()
    assert {(c["source"], c["kind"]) for c in claims} >= {("rules", "drop"), ("fused", "drop"), ("dropdetector", "drop")}
    bars = pq.read_table(folder / "bars.parquet").to_pylist()
    assert len(bars) == N and bars[12]["gold_buildup"] is True and bars[16]["dropdetector_drop"] == 0.97
    assert pq.read_table(folder / "tracks.parquet").to_pylist()[0]["sources"] == ["dropdetector", "fused", "rules"]
    assert {p: p.read_bytes() for p in tmp_path.rglob("*.json")} == before


FUSED = {**DOC, "rules": {"sections": DOC["sections"], "events": []},
         "sections": [{"label": "intro", "start_bar": 0, "end_bar": 10, "confidence": 100},
                      {"label": "buildup", "start_bar": 10, "end_bar": 18, "confidence": 60},
                      {"label": "drop", "start_bar": 18, "end_bar": 32, "confidence": 90}],
         "analyzer": {"version": "2.5.1"}}


def test_a_fused_analysis_gives_claims_for_the_rules_and_for_the_fusion():
    rows = claims_ours(TID, FUSED)
    pick = lambda source: [(r["kind"], r["bar"], r["score"]) for r in rows if r["source"] == source]
    assert pick("rules") == [("bar1", 0, 88), ("section", 0, 100), ("section", 12, 40), ("buildup", 12, 40), ("section", 16, 70), ("drop", 16, 70)]
    assert pick("fused") == [("section", 0, 100), ("section", 10, 60), ("buildup", 10, 60), ("section", 18, 90), ("drop", 18, 90)]


def test_answers_say_which_analysis_they_are_read_against():
    answers = {"r01-p001": {"round": 1, "track": TID, "answer": "yes", "claim": {"kind": "drop", "bar": 16, "time": 33.0}}}
    assert answer_rows(answers, {TID: FUSED})[0]["analyzer"] == "2.5.1"
    assert answer_rows(answers, {})[0]["analyzer"] is None


def _group(answers, root="r01-p001"):
    """The answers to one probe and its repeats, one per round."""
    claim = {"kind": "drop", "bar": 16, "time": 33.0, "confidence": 50}
    return {f"r0{i}-p001": {"answer": a, "round": i, "track": TID, "claim": claim, **({"repeat_of": root} if i > 1 else {})}
            for i, a in enumerate(answers, 1)}


@pytest.mark.parametrize("answers,kept", [
    (["yes"], ["yes"]), (["yes", "yes"], ["yes"]),           # a repeat that agrees counts once
    (["yes", "no"], []),                                      # one that disagrees removes both
    (["yes", "no", "no"], ["no"]),                            # until a third answer settles it
])
def test_answer_rows_count_a_probe_and_its_repeats_as_one_gold_answer(answers, kept):
    assert [r["answer"] for r in answer_rows(_group(answers), {TID: DOC})] == kept


def test_a_buildup_located_where_we_had_none_is_a_buildup_answer():
    miss = {"answer": "bar:8", "round": 4, "track": TID, "claim": {"kind": "missed", "bar": 16, "time": 33.0, "confidence": 50},
            "alternatives": {"none": {"source": "ours"}, "edmformer": {"bar": 9}}}
    cue = {"answer": "yes", "round": 4, "track": TID, "claim": {"kind": "missed", "bar": 12, "time": 25.0, "confidence": 50}}
    rows = {r["probe"]: r for r in answer_rows({"r04-p001": miss, "r04-p002": cue}, {TID: DOC})}
    assert (rows["r04-p001"]["kind"], rows["r04-p001"]["gold_bar"], rows["r04-p001"]["exact"]) == ("buildup", 8, False)
    assert (rows["r04-p002"]["kind"], rows["r04-p002"]["gold_bar"]) == ("missed", 12)   # a new part at a cue is no buildup


def test_fused_claims_name_their_sources_and_lifts_are_claims_of_their_own():
    doc = {**FUSED, "sections": [{**s, "sources": src} for s, src in zip(FUSED["sections"], ([], ["rules"], ["dropdetector", "edmformer"]))],
           "events": [{"kind": "lift", "bar": 25, "time": 51.0, "sources": ["dropdetector"]}, {"kind": "vocal_in", "bar": 3, "time": 7.0}]}
    rows = [r for r in claims_ours(TID, doc) if r["source"] == "fused"]
    assert [(r["kind"], r["bar"], r["sources"]) for r in rows if r["kind"] != "section"] == [
        ("buildup", 10, "rules"), ("drop", 18, "dropdetector+edmformer"), ("lift", 25, "dropdetector")]
    assert [r["sources"] for r in claims_ours(TID, doc) if r["source"] == "rules"] == [None] * 6
