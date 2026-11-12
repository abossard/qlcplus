import json
import random

import pytest

from edm_analysis.confidence import LABELS

from edm_analysis.rounds import current_round, load_answers, public_round, record_answer, validate_answer

YESNO = {"id": "r01-p001", "track": "abababababababab", "title": "song", "theory": "t1", "bucket": "unsure",
         "question": "yesno", "prompt": "Does the drop start at the marker?",
         "claim": {"kind": "drop", "bar": 8, "time": 16.0, "confidence": 41},
         "plays": [{"id": "main", "start": 12.0, "end": 20.0, "mark": 16.0}]}
AB = {**YESNO, "id": "r01-p002", "question": "ab", "prompt": "Which is the drop?",
      "plays": [{"id": "a", "start": 12.0, "end": 20.0, "mark": 16.0}, {"id": "b", "start": 20.0, "end": 28.0, "mark": 24.0}],
      "alternatives": {"a": {"bar": 8, "time": 16.0, "source": "ours"}, "b": {"bar": 12, "time": 24.0, "source": "vdj"}}}
LABEL = {**YESNO, "id": "r01-p003", "question": "label", "prompt": "Is this a drop?"}
MISSED = {**YESNO, "id": "r01-p005", "claim": {"kind": "missed", "bar": 8, "time": 16.0, "confidence": 50}}
LOCATE = {**YESNO, "id": "r01-p004", "question": "locate", "prompt": "Tap the bar where the drop starts",
          "plays": [{"id": "main", "start": 8.0, "end": 28.0, "mark": None}],
          "alternatives": {"ours": {"bar": 8}, "edmformer": {"bar": 12}}}
ROUND = {"round": 1, "created": "2026-11-05T10:00:00", "theories": [{"id": "t1", "text": "Drops are late"}],
         "probes": [YESNO, AB, LABEL]}


def write_round(root, rnd):
    (root / "rounds").mkdir(exist_ok=True)
    (root / "rounds" / f"round-{rnd['round']:02d}.json").write_text(json.dumps(rnd))


@pytest.mark.parametrize("probe,answer,ok", [
    (YESNO, "yes", True), (YESNO, "earlier", True), (YESNO, "later", True), (YESNO, "skip", True),
    (YESNO, "none", True), (YESNO, "no", True), (MISSED, "no", True), (MISSED, "none", False),
    (YESNO, "a", False), (YESNO, "", False), (YESNO, None, False), (YESNO, 1, False),
    (AB, "a", True), (AB, "b", True), (AB, "neither", True), (AB, "yes", False),
    (LABEL, "yes", True), (LABEL, "no", True), (LABEL, "earlier", False),
    (YESNO, "bar:0", True), (YESNO, "bar:12", True), (YESNO, "bar:24", True), (YESNO, "bar:25", False),
    (YESNO, "bar:-1", False), (YESNO, "bar:08", False), (YESNO, "bar:", False), (YESNO, "bar:x", False), (YESNO, "bar:1.5", False),
    (LABEL, "is:drop", True), (LABEL, "is:outro", True), (LABEL, "is:chorus", False), (LABEL, "is:", False), (LABEL, "bar:9", False),
    (AB, "bar:9", False), (AB, "is:drop", False), (YESNO, "is:drop", False),
    (LOCATE, "bar:9", True), (LOCATE, "bar:24", True), (LOCATE, "bar:25", False), (LOCATE, "none", True), (LOCATE, "skip", True),
    (LOCATE, "yes", False), (LOCATE, "earlier", False), (LOCATE, "no", False),
])
def test_validate_answer(probe, answer, ok):
    assert (validate_answer(probe, answer) is None) == ok


def test_the_latest_round_is_current(tmp_path):
    assert current_round(tmp_path) is None
    write_round(tmp_path, ROUND)
    write_round(tmp_path, {**ROUND, "round": 2})
    assert current_round(tmp_path)["round"] == 2


def test_answers_are_stored_with_the_claim_they_judge(tmp_path):
    write_round(tmp_path, ROUND)

    assert record_answer(tmp_path, "r01-p001", "earlier", now="2026-11-05T10:05:00") == (200, None)
    assert record_answer(tmp_path, "r01-p001", "yes", now="2026-11-05T10:06:00") == (200, None)

    stored = load_answers(tmp_path)["r01-p001"]
    assert stored["answer"] == "yes" and stored["at"] == "2026-11-05T10:06:00"
    assert stored["round"] == 1 and stored["track"] == "abababababababab" and stored["claim"] == YESNO["claim"]
    assert not list((tmp_path / "rounds").glob("*.tmp"))


@pytest.mark.parametrize("probe_id,answer,status", [("r09-p001", "yes", 404), ("r01-p001", "maybe", 400), ("r01-p002", "yes", 400)])
def test_bad_answers_change_nothing(tmp_path, probe_id, answer, status):
    write_round(tmp_path, ROUND)
    assert record_answer(tmp_path, probe_id, answer)[0] == status
    assert load_answers(tmp_path) == {}


def test_the_public_round_hides_what_could_bias_the_reviewer(tmp_path):
    write_round(tmp_path, ROUND)
    record_answer(tmp_path, "r01-p002", "b")

    pub = public_round(current_round(tmp_path), load_answers(tmp_path))

    assert pub["round"] == 1 and pub["progress"] == {"answered": 1, "total": 3}
    assert pub["theories"] == [{"id": "t1", "text": "Drops are late"}]
    assert [p["answer"] for p in pub["probes"]] == [None, "b", None]
    assert [o["value"] for o in pub["probes"][0]["options"]] == ["yes", "none", "skip"]
    text = json.dumps(pub)
    for hidden in ("confidence", "bucket", "source", "claim", "path"):
        assert f'"{hidden}"' not in text


@pytest.mark.parametrize("probe,options,tap,labels", [
    (YESNO, ["yes", "none", "skip"], True, None),
    (MISSED, ["yes", "no", "skip"], True, None),
    (AB, ["a", "b", "neither", "skip"], False, None),
    (LABEL, ["yes", "no", "skip"], False, list(LABELS)),
    (LOCATE, ["none", "skip"], True, None),
])
def test_the_public_probe_says_what_the_page_may_offer(probe, options, tap, labels):
    pub = public_round({**ROUND, "probes": [probe]}, {})["probes"][0]
    assert [o["value"] for o in pub["options"]] == options
    assert pub["tap"] is tap and pub.get("labels") == labels
    assert ("mark" in pub["plays"][0] and pub["plays"][0]["mark"] is None) == (probe is LOCATE)
    for hidden in ("confidence", "bucket", "source", "claim", "alternatives", "ours", "edmformer"):
        assert f'"{hidden}"' not in json.dumps(pub)


# --- making and scoring rounds ---

from edm_analysis.rounds import BUCKETS, CONFIRMS, THEORIES, make_round, report, theory_candidates  # noqa: E402

N = 32


def doc(i, drop_bar=16, drop_conf=60, grid_conf=90):
    tid = f"{i:016x}"
    lift = 0.01 * i
    return tid, {
        "track": {"path": f"/m/{tid}.mp3", "title": f"song {i}", "sha256": tid * 4},
        "grid": {"bpm": 120.0, "period": 0.5, "first_downbeat": 0.0, "confidence": 0.9, "bar1_confidence": grid_conf},
        "beats": [0.5 * k for k in range(4 * N)],
        "bars": {"start": [2.0 * k for k in range(N)],
                 "levels": {"kick": [0.0] * drop_bar + [1.0] * (N - drop_bar), "bass": [lift] * N, "hihat": [0.5] * N,
                            "instruments": [0.5 + lift] * N, "vocal": [0.0] * N, "loud": [0.3] * drop_bar + [0.9] * (N - drop_bar)}},
        "sections": [{"label": "intro", "start_bar": 0, "end_bar": drop_bar, "confidence": 100, "label_confidence": 50},
                     {"label": "drop", "start_bar": drop_bar, "end_bar": N, "confidence": drop_conf, "label_confidence": 50}],
        "events": [{"kind": "drop", "bar": drop_bar, "time": 2.0 * drop_bar, "confidence": drop_conf}]}


DOCS = dict(doc(i, drop_conf=10 * (i % 10)) for i in range(1, 25))


def test_a_round_asks_once_per_track_within_its_size():
    rnd = make_round(1, DOCS, {}, {}, [], size=10, created="2026-11-05T10:00:00")
    tracks = [p["track"] for p in rnd["probes"]]
    assert len(rnd["probes"]) == 10 and len(set(tracks)) == 10
    assert [p["id"] for p in rnd["probes"]][:2] == ["r01-p001", "r01-p002"]
    assert {p["bucket"] for p in rnd["probes"]} <= set(BUCKETS)
    assert {t["id"] for t in rnd["theories"]} == {p["theory"] for p in rnd["probes"]}
    assert all(0 <= p["claim"]["confidence"] <= 100 for p in rnd["probes"])


def test_later_rounds_use_fresh_tracks_and_follow_up_once():
    first = make_round(1, DOCS, {}, {}, [], size=10)
    yesno = next(p for p in first["probes"] if p["question"] == "yesno" and p["claim"]["kind"] == "drop")
    answers = {yesno["id"]: {"answer": "earlier", "round": 1, "track": yesno["track"], "claim": yesno["claim"]}}

    second = make_round(2, DOCS, {}, answers, [first], size=10)

    follow = [p for p in second["probes"] if p["theory"] == "followup"]
    assert [(p["track"], p["claim"]["bar"], p["claim"]["followup_of"]) for p in follow] == [
        (yesno["track"], yesno["claim"]["bar"] - 4, yesno["id"])]
    others = {p["track"] for p in second["probes"] if p["theory"] != "followup"}
    assert not others & {yesno["track"]}
    third = make_round(3, DOCS, {}, answers, [first, second], size=10)
    assert not [p for p in third["probes"] if p["theory"] == "followup"]


@pytest.mark.parametrize("answered", [0, 2, 5])
def test_only_tracks_of_answered_probes_count_as_used(answered):
    first = make_round(1, DOCS, {}, {}, [], size=len(DOCS))
    assert len({p["track"] for p in first["probes"]}) == len(DOCS)
    done = first["probes"][:answered]
    answers = {p["id"]: {"answer": "yes", "round": 1, "track": p["track"], "claim": p["claim"]} for p in done}

    second = make_round(2, DOCS, {}, answers, [first], size=len(DOCS))

    assert {p["track"] for p in second["probes"] if "repeat_of" not in p} == set(DOCS) - {p["track"] for p in done}


@pytest.mark.parametrize("answer,follow_ups", [("earlier", 1), ("later", 1), ("bar:12", 0), ("bar:20", 0), ("yes", 0), ("no", 0)])
def test_exact_bar_answers_need_no_followup(answer, follow_ups):
    first = make_round(1, DOCS, {}, {}, [], size=10)
    yesno = next(p for p in first["probes"] if p["question"] == "yesno" and p["claim"]["kind"] == "drop")
    answers = {yesno["id"]: {"answer": answer, "round": 1, "track": yesno["track"], "claim": yesno["claim"]}}
    second = make_round(2, DOCS, {}, answers, [first], size=10)
    assert len([p for p in second["probes"] if p["theory"] == "followup"]) == follow_ups


def test_disputed_drops_become_a_or_b_questions_in_either_order():
    vdj = {d["track"]["path"]: {"period": 0.5, "phase": 0.0, "cues": [], "breaks": [(8.0, 40.0)]} for d in DOCS.values()}
    rnd = make_round(1, DOCS, vdj, {}, [], size=20)
    ab = [p for p in rnd["probes"] if p["theory"] == "drop"]
    assert ab
    for p in ab:
        assert {a["source"] for a in p["alternatives"].values()} == {"ours", "vdj"}
        assert [pl["mark"] for pl in p["plays"]] == [p["alternatives"]["a"]["time"], p["alternatives"]["b"]["time"]]
    assert {p["alternatives"]["a"]["source"] for p in ab} == {"ours", "vdj"}


def vdj_ends(*bars):
    return {"period": 0.5, "phase": 0.0, "cues": [], "breaks": [(0.0, 2.0 * b) for b in bars]}


@pytest.mark.parametrize("ends,drops,alternatives,play_bars", [
    ([], {"edmformer": [18]}, {"ours": 16, "edmformer": 18}, (12, 20)),
    ([], {"edmformer": [14]}, {"ours": 16, "edmformer": 14}, (10, 18)),
    ([], {"edmformer": [24]}, {"ours": 16, "edmformer": 24}, (12, 26)),
    ([], {"edmformer": [4, 19]}, {"ours": 16, "edmformer": 19}, (12, 21)),          # the nearest drop of a source counts
    ([14], {"dropdetector": [20]}, {"ours": 16, "vdj": 14, "dropdetector": 20}, (10, 22)),
    ([16], {"edmformer": [18], "dropdetector": [16]}, {"ours": 16, "vdj": 16, "edmformer": 18, "dropdetector": 16}, (12, 20)),
    ([19], {}, {"ours": 16, "vdj": 19}, (12, 21)),
    ([], {"edmformer": [16]}, None, None),               # all agree
    ([], {"edmformer": [25]}, None, None),               # too far to be the same drop
    ([], {"edmformer": [8], "dropdetector": [24]}, None, None),   # clip would be longer than 16 bars
    ([], {}, None, None),
])
def test_where_sources_disagree_about_a_drop_the_reviewer_taps_the_bar(ends, drops, alternatives, play_bars):
    tid, d = doc(1)
    probes = theory_candidates(tid, d, vdj_ends(*ends) if ends else None, random.Random(0), drops)["detectors"]
    if alternatives is None:
        assert probes == []
        return
    (p,) = probes
    assert (p["theory"], p["question"], p["prompt"]) == ("detectors", "locate", "Tap the bar where the drop starts")
    assert p["alternatives"] == {k: {"bar": v} for k, v in alternatives.items()}
    assert p["claim"] == {"kind": "drop", "bar": 16, "time": 32.0, "confidence": 60}
    assert p["plays"] == [{"id": "main", "start": 2.0 * play_bars[0], "end": 2.0 * play_bars[1], "mark": None}]


@pytest.mark.parametrize("ends,drops,sources", [
    ([], {"edmformer": [18]}, {"edmformer": 18}),
    ([], {"edmformer": [16]}, {"edmformer": 16}),
    ([14], {"edmformer": [18], "dropdetector": [30]}, {"vdj": 14, "edmformer": 18}),   # 14 bars away is another drop
    ([19], None, {"vdj": 19}),
    ([], {"edmformer": [4, 20]}, {"edmformer": 20}),
    ([], {"edmformer": [40]}, None),
    ([], {"edmformer": []}, None),
    ([], None, None),
])
def test_calibration_questions_about_a_drop_carry_what_the_other_sources_say(ends, drops, sources):
    tid, d = doc(1)
    out = theory_candidates(tid, d, vdj_ends(*ends) if ends else None, random.Random(0), drops)["calibration"]
    (p,) = [p for p in out if p["question"] == "yesno"]
    assert p.get("sources") == sources


def test_only_questions_about_a_drop_carry_sources():
    tid, d = doc(1)
    d["sections"] = [d["sections"][0], {"label": "buildup", "start_bar": 12, "end_bar": 16, "confidence": 50},
                     {**d["sections"][1], "start_bar": 16}]
    out = theory_candidates(tid, d, None, random.Random(0), {"edmformer": [13, 17]})["calibration"]
    assert {p["claim"]["kind"]: p.get("sources") for p in out if p["question"] == "yesno"} == {"buildup": None, "drop": {"edmformer": 17}}


def test_the_play_does_not_start_before_the_track():
    tid, d = doc(1, drop_bar=2)
    (p,) = theory_candidates(tid, d, None, random.Random(0), {"edmformer": [4]})["detectors"]
    assert p["plays"][0]["start"] == 0.0 and p["plays"][0]["end"] == 12.0


def test_tracks_with_candidate_output_get_locate_questions_and_the_others_keep_a_or_b():
    vdj = {d["track"]["path"]: vdj_ends(24) for d in DOCS.values()}
    with_candidates = list(DOCS)[:12]
    rnd = make_round(1, DOCS, vdj, {}, [], size=24, candidate_drops={t: {"edmformer": [18]} for t in with_candidates})
    locate = [p for p in rnd["probes"] if p["theory"] == "detectors"]
    ab = [p for p in rnd["probes"] if p["theory"] == "drop"]
    assert locate and ab
    assert {p["track"] for p in locate} <= set(with_candidates) and not {p["track"] for p in ab} & set(with_candidates)
    assert {p["theory"] for p in locate} == {"detectors"} and {p["bucket"] for p in locate} == {"unsure"}
    text = next(t["text"] for t in rnd["theories"] if t["id"] == "detectors")
    assert not any(name in text.lower() for name in ("vdj", "virtualdj", "edmformer", "drop detector", "allin1"))
    assert len({p["track"] for p in rnd["probes"]}) == len(rnd["probes"])


def test_the_tracks_where_the_sources_disagree_most_come_first():
    special = list(DOCS)[8]    # a high confidence drop: confidence order alone would put it last
    drops = {t: {"edmformer": [17]} for t in list(DOCS)[:12]}
    drops[special] = {"edmformer": [18], "dropdetector": [20]}
    rnd = make_round(1, DOCS, {}, {}, [], size=10, candidate_drops=drops)
    assert next(p for p in rnd["probes"] if p["theory"] == "detectors")["track"] == special


def judged(n, confidence, answer):
    probes = [{"id": f"r01-p{i + 1:03d}", "track": f"{i:016x}", "theory": "calibration", "question": "yesno",
               "claim": {"kind": "drop", "bar": 16, "time": 32.0, "confidence": confidence}} for i in range(n)]
    answers = {p["id"]: {"answer": answer(i), "round": 1, "track": p["track"], "claim": p["claim"]} for i, p in enumerate(probes)}
    return [{"round": 1, "theories": [], "probes": probes}], answers


@pytest.mark.parametrize("n,confidence,wrong_every,library_conf,met", [
    (40, 97, 0, 97, True),
    (40, 97, 10, 97, False),     # 90% exact when sure
    (10, 97, 0, 97, False),      # too few sure answers to tell
    (40, 97, 0, 60, False),      # too little of the library is sure
])
def test_the_target_needs_honest_and_broad_confidence(n, confidence, wrong_every, library_conf, met):
    rounds, answers = judged(n, confidence, lambda i: "no" if wrong_every and i % wrong_every == 0 else "yes")
    docs = dict(doc(i, drop_conf=library_conf, grid_conf=library_conf) for i in range(1, 5))
    for d in docs.values():
        d["sections"] = [{**s, "label_confidence": library_conf} for s in d["sections"]]
    out = report(rounds, answers, docs)
    assert out["target"]["met"] is met
    assert out["rounds"][0]["answered"] == n


def test_the_report_scores_fresh_answers_and_theories():
    rounds, answers = judged(4, 40, lambda i: "yes" if i < 3 else "later")
    out = report(rounds, answers, DOCS)
    assert out["rounds"][0]["fresh_exact"] == {"grid": None, "start": 0.75, "label": None}
    assert out["calibration"][0] == {"band": "0-50", "n": 4, "exact": 0.75}


def answer_of(probe, answer, claim_bar=16, alternatives=None, sources=None):
    claim = {"kind": "drop", "bar": claim_bar, "time": 2.0 * claim_bar, "confidence": 60}
    return {"answer": answer, "round": 1, "track": probe, "claim": claim, **({"alternatives": alternatives} if alternatives else {}),
            **({"sources": sources} if sources else {})}


def test_the_report_scores_every_source_on_the_bars_the_reviewer_gave():
    sources = {"ours": {"bar": 16}, "edmformer": {"bar": 18}, "dropdetector": {"bar": 18}}
    answers = {
        "r01-p001": answer_of("t1", "yes"),
        "r01-p002": answer_of("t2", "bar:18"),
        "r01-p003": answer_of("t3", "bar:18", alternatives=sources),
        "r01-p004": answer_of("t4", "none", alternatives=sources),
        "r01-p005": answer_of("t5", "b", alternatives={"a": {"source": "ours", "bar": 16}, "b": {"source": "vdj", "bar": 18}}),
        "r01-p006": answer_of("t6", "later"),
        "r01-p007": answer_of("t7", "skip", alternatives=sources),
    }
    theories = {"r01-p003": "detectors", "r01-p004": "detectors"}
    probes = [{"id": pid, "track": a["track"], "theory": theories.get(pid, "calibration"), "claim": a["claim"],
               "question": "locate" if pid in theories else "yesno"} for pid, a in answers.items()]
    out = report([{"round": 1, "theories": [], "probes": probes}], answers, DOCS)
    assert out["sources"] == {"ours": {"n": 4, "exact": 0.25}, "edmformer": {"n": 1, "exact": 1.0},
                              "dropdetector": {"n": 1, "exact": 1.0}, "vdj": {"n": 1, "exact": 1.0}}
    assert out["theories"]["detectors"] == {"n": 2, "confirmed": 0.5, "text": THEORIES["detectors"]}
    assert set(out) == {"rounds", "theories", "calibration", "coverage", "target", "sources", "reviewer"}


def test_the_report_has_no_sources_without_exact_bars():
    rounds, answers = judged(3, 40, lambda i: "later")
    assert report(rounds, answers, DOCS)["sources"] == {}


def test_the_sources_of_a_yes_or_no_question_are_stored_with_the_answer_and_stay_secret(tmp_path):
    probe = {**YESNO, "id": "r01-p005", "sources": {"edmformer": 12, "vdj": 8}}
    write_round(tmp_path, {**ROUND, "probes": [probe]})

    assert record_answer(tmp_path, "r01-p005", "yes") == (200, None)

    assert load_answers(tmp_path)["r01-p005"]["sources"] == {"edmformer": 12, "vdj": 8}
    text = json.dumps(public_round(current_round(tmp_path), load_answers(tmp_path)))
    for hidden in ("sources", "edmformer", "vdj"):
        assert hidden not in text


def test_yes_and_bar_answers_score_every_source_that_was_asked_about():
    sources = {"edmformer": 18, "vdj": 16}
    answers = {
        "r01-p001": answer_of("t1", "yes", sources={**sources}),
        "r01-p002": answer_of("t2", "bar:18", sources={**sources}),
        "r01-p003": answer_of("t3", "yes"),
    }
    probes = [{"id": pid, "track": a["track"], "theory": "calibration", "claim": a["claim"], "question": "yesno"} for pid, a in answers.items()]
    out = report([{"round": 1, "theories": [], "probes": probes}], answers, DOCS)
    assert out["sources"] == {"ours": {"n": 3, "exact": 0.667}, "edmformer": {"n": 2, "exact": 0.5}, "vdj": {"n": 2, "exact": 0.5}}


@pytest.mark.parametrize("theory", sorted(CONFIRMS))
def test_a_theory_in_the_report_carries_its_text(theory):
    answers = {"r01-p001": answer_of("t1", "yes")}
    probes = [{"id": "r01-p001", "track": "t1", "theory": theory, "claim": answers["r01-p001"]["claim"], "question": "yesno"}]
    assert report([{"round": 1, "theories": [], "probes": probes}], answers, DOCS)["theories"][theory]["text"] == THEORIES[theory]


def test_the_detectors_pool_gets_two_turns_per_cycle():
    drops = {t: {"edmformer": [18]} for t in DOCS}
    rnd = make_round(1, DOCS, {}, {}, [], size=24, candidate_drops=drops)
    counts = {t: sum(p["theory"] == t and p["bucket"] == "unsure" for p in rnd["probes"]) for t in ("detectors", "grid", "labels")}
    counts["grid"] = sum(p["claim"]["kind"] == "grid" and p["bucket"] == "unsure" for p in rnd["probes"])
    assert counts["detectors"] >= 2 * counts["grid"] - 1 and counts["detectors"] > counts["grid"]


def test_every_answer_ever_given_stays_in_the_log(tmp_path):
    write_round(tmp_path, ROUND)
    (tmp_path / "rounds" / "answers.json").write_text(json.dumps({"r01-p003": {"answer": "no", "round": 1}}))

    record_answer(tmp_path, "r01-p001", "earlier", now="2026-11-05T10:05:00")
    record_answer(tmp_path, "r01-p001", "yes", now="2026-11-05T10:06:00")

    log = [json.loads(line) for line in (tmp_path / "rounds" / "answers-log.jsonl").read_text().splitlines()]
    assert [(e["probe"], e["answer"]) for e in log] == [("r01-p003", "no"), ("r01-p001", "earlier"), ("r01-p001", "yes")]
    assert load_answers(tmp_path)["r01-p001"]["answer"] == "yes"


# --- buildup questions ---

from edm_analysis.confidence import _exact, gold_bar, gold_rows  # noqa: E402
from edm_analysis.rounds import gold_answers  # noqa: E402


def with_buildup(d, start, end=16):
    d["sections"] = [{**d["sections"][0], "end_bar": start}, {"label": "buildup", "start_bar": start, "end_bar": end, "confidence": 70, "label_confidence": 50},
                     d["sections"][1]]
    return d


def buildup_probes(ours, others=None, drop_bar=16, kind="buildup"):
    tid, d = doc(1, drop_bar=drop_bar)
    if ours:
        with_buildup(d, ours, drop_bar)
    return tid, d, theory_candidates(tid, d, None, random.Random(0), None, others)[kind]


NONE_OURS = {"none": {"source": "ours"}}


@pytest.mark.parametrize("ours,others,drop_bar,claim,alternatives,play_bars", [
    (None, None, 16, {"kind": "missed", "bar": 16, "time": 32.0, "confidence": 50}, NONE_OURS, (2, 18)),
    (8, None, 16, {"kind": "buildup", "bar": 8, "time": 16.0, "confidence": 70}, {"ours": {"bar": 8}}, (2, 18)),
    (2, None, 16, {"kind": "buildup", "bar": 2, "time": 4.0, "confidence": 70}, {"ours": {"bar": 2}}, (2, 18)),     # the first bar of the clip
    (8, {"edmformer": [10]}, 16, {"kind": "buildup", "bar": 8, "time": 16.0, "confidence": 70}, {"ours": {"bar": 8}, "edmformer": {"bar": 10}}, (2, 18)),
    (None, {"edmformer": [12]}, 16, {"kind": "missed", "bar": 16, "time": 32.0, "confidence": 50}, {**NONE_OURS, "edmformer": {"bar": 12}}, (2, 18)),
    (8, {"edmformer": [4, 11]}, 16, {"kind": "buildup", "bar": 8, "time": 16.0, "confidence": 70}, {"ours": {"bar": 8}, "edmformer": {"bar": 11}}, (2, 18)),
    (8, {"edmformer": [1, 16, 20]}, 16, {"kind": "buildup", "bar": 8, "time": 16.0, "confidence": 70}, {"ours": {"bar": 8}}, (2, 18)),   # outside the clip or not before the drop
    (None, None, 4, {"kind": "missed", "bar": 4, "time": 8.0, "confidence": 50}, NONE_OURS, (0, 6)),
])
def test_the_reviewer_taps_where_the_buildup_before_a_drop_starts(ours, others, drop_bar, claim, alternatives, play_bars):
    tid, d, probes = buildup_probes(ours, others, drop_bar)
    (p,) = probes
    assert (p["theory"], p["question"], p["prompt"]) == ("buildup", "locate", "Tap the bar where the buildup starts")
    assert p["claim"] == claim and p["alternatives"] == alternatives
    assert p["plays"] == [{"id": "main", "start": 2.0 * play_bars[0], "end": 2.0 * play_bars[1], "mark": None}]
    assert "sources" not in p


def test_a_buildup_longer_than_the_clip_can_hold_is_not_asked_about():
    assert buildup_probes(1)[2] == []


def test_the_buildup_theory_names_no_source():
    assert not any(name in THEORIES["buildup"].lower() for name in ("vdj", "virtualdj", "edmformer", "drop detector", "allin1", "rules"))


@pytest.mark.parametrize("ours,others,answer,exact,gold", [
    (8, None, "bar:8", True, 8), (8, None, "bar:10", False, 10), (8, None, "none", False, None), (8, None, "skip", None, None),
    (None, None, "none", True, None), (None, None, "bar:10", False, 10), (None, None, "skip", None, None),
    (8, {"edmformer": [10]}, "none", False, None), (None, {"edmformer": [10]}, "none", True, None),
    (None, {"edmformer": [10]}, "bar:10", False, 10), (8, {"edmformer": [10]}, "bar:10", False, 10),
])
def test_a_buildup_answer_is_exact_and_names_a_gold_bar_both_with_and_without_our_buildup(tmp_path, ours, others, answer, exact, gold):
    tid, d, (p,) = buildup_probes(ours, others)
    write_round(tmp_path, {**ROUND, "probes": [{**p, "id": "r01-p001"}]})

    assert record_answer(tmp_path, "r01-p001", answer) == (200, None)

    record = load_answers(tmp_path)["r01-p001"]
    assert (_exact(record), gold_bar(record)) == (exact, gold)
    assert bool(gold_rows({"r01-p001": record}, {tid: d})) == bool(ours and answer != "skip")   # no row for a Claim we did not make


@pytest.mark.parametrize("ours,answer,ok", [
    (None, "bar:2", True), (None, "bar:15", True), (None, "bar:16", False), (None, "bar:17", False), (None, "bar:1", True), (None, "bar:0", True),
    (8, "bar:2", True), (8, "bar:15", True), (8, "bar:16", False), (8, "bar:33", False), (8, "yes", False), (8, "no", False), (8, "none", True),
])
def test_the_tap_on_a_buildup_question_must_be_a_bar_before_the_drop(ours, answer, ok):
    p = buildup_probes(ours)[2][0]
    assert (validate_answer(p, answer) is None) == ok


def test_the_buildup_question_says_what_none_means():
    p = buildup_probes(8)[2][0]
    pub = public_round({**ROUND, "probes": [{**p, "id": "r01-p001"}]}, {})["probes"][0]
    assert [(o["value"], o["label"]) for o in pub["options"]] == [("none", "No buildup"), ("skip", "Can't tell")]
    assert pub["tap"] is True
    for hidden in ("confidence", "bucket", "source", "alternatives", "ours", "claim", "before", "none_label"):
        assert f'"{hidden}"' not in json.dumps(pub)


def test_calibration_may_draw_a_buildup_question_with_the_calibration_theory():
    (p,) = [q for q in buildup_probes(8, kind="calibration")[2] if q["question"] == "locate"]
    assert p["theory"] == "calibration" and p["prompt"] == "Tap the bar where the buildup starts" and p["claim"]["kind"] == "buildup"


# --- start questions with room to tap ---

@pytest.mark.parametrize("drops,play_bars", [
    ({}, (14, 18)),
    ({"edmformer": [18]}, (14, 20)),
    ({"edmformer": [14]}, (13, 18)),
    ({"edmformer": [24]}, (14, 26)),
    ({"edmformer": [8]}, (7, 18)),
    ({"edmformer": [16]}, (14, 18)),
    ({"edmformer": [10], "dropdetector": [24]}, (9, 18)),          # the farther bar would make the clip longer than 16 bars
    ({"edmformer": [18], "dropdetector": [12]}, (11, 20)),
])
def test_a_start_question_holds_every_other_bar_it_knows_with_a_bar_to_spare(drops, play_bars):
    tid, d = doc(1)
    out = theory_candidates(tid, d, None, random.Random(0), drops or None)["calibration"]
    (p,) = [p for p in out if p["question"] == "yesno"]
    assert (p["plays"][0]["start"], p["plays"][0]["end"], p["plays"][0]["mark"]) == (2.0 * play_bars[0], 2.0 * play_bars[1], 32.0)


def test_the_clip_of_a_start_question_does_not_start_before_the_track():
    tid, d = doc(1, drop_bar=2)
    out = theory_candidates(tid, d, None, random.Random(0), {"edmformer": [4]})["calibration"]
    assert [p["plays"][0]["start"] for p in out if p["question"] == "yesno"] == [0.0]


@pytest.mark.parametrize("kind,answer,exact,gold", [(k, "none", False, None) for k in ("drop", "buildup", "breakdown")])
def test_not_in_this_clip_is_not_exact_and_names_no_bar(kind, answer, exact, gold):
    record = {"answer": answer, "claim": {"kind": kind, "bar": 16, "time": 32.0, "confidence": 60}}
    assert (_exact(record), gold_bar(record)) == (exact, gold)


# --- repeats ---

from edm_analysis.rounds import REPEAT_SHARE  # noqa: E402


def answer_all(rnd, choose):
    return {p["id"]: {"answer": choose(p), "round": rnd["round"], "track": p["track"], "claim": p["claim"],
                      **{k: p[k] for k in ("alternatives", "sources", "repeat_of") if k in p}} for p in rnd["probes"]}


def yes_or_a(p):
    return {"ab": "a"}.get(p["question"], "yes")


def repeats_of(rnd):
    return [p for p in rnd["probes"] if "repeat_of" in p]


def stripped(p):
    return {k: v for k, v in p.items() if k not in ("id", "bucket", "repeat_of")}


def test_a_round_repeats_four_earlier_answers_outside_its_other_shares():
    first = make_round(1, DOCS, {}, {}, [], size=12)
    answers = answer_all(first, yes_or_a)

    second = make_round(2, DOCS, {}, answers, [first], size=40)

    roots = {p["id"]: p for p in first["probes"]}
    repeats = repeats_of(second)
    assert REPEAT_SHARE == 0.1 and len(repeats) == 4 and len(second["probes"]) == 12 + 4
    assert {p["bucket"] for p in repeats} == {"repeat"} and len({p["track"] for p in repeats}) == 4
    for p in repeats:
        assert p["repeat_of"] in roots and stripped(p) == stripped(roots[p["repeat_of"]])
    assert [p["id"] for p in second["probes"]] == [f"r02-p{i + 1:03d}" for i in range(16)]
    assert len({p["track"] for p in second["probes"] if "repeat_of" not in p}) == 12


def test_a_repeat_looks_like_a_new_question_to_the_page():
    first = make_round(1, DOCS, {}, {}, [], size=12)
    second = make_round(2, DOCS, {}, answer_all(first, yes_or_a), [first], size=40)
    roots = {p["id"]: p for p in first["probes"]}
    pub_first = {p["id"]: p for p in public_round(first, {})["probes"]}
    for p in repeats_of(second):
        pub = public_round({**second, "probes": [p]}, {})["probes"][0]
        assert pub["answer"] is None
        assert {k: v for k, v in pub.items() if k != "id"} == {k: v for k, v in pub_first[p["repeat_of"]].items() if k not in ("id", "answer")} | {"answer": None}
        assert roots[p["repeat_of"]]["theory"] == pub["theory"]
    text = json.dumps(public_round(second, {}))
    for hidden in ("repeat", "bucket", "confidence", "source"):
        assert f'"{hidden}' not in text


def asked(pid, track, sources=None, kind="drop"):
    return {"id": pid, "track": track, "title": "t", "theory": "calibration", "bucket": "style", "question": "yesno", "prompt": "p",
            "claim": {"kind": kind, "bar": 16, "time": 32.0, "confidence": 60}, "plays": [{"id": "main", "start": 0.0, "end": 8.0, "mark": 4.0}],
            **({"sources": sources} if sources else {})}


def given(probe, answer, round_no=1):
    return {"answer": answer, "round": round_no, "track": probe["track"], "claim": probe["claim"],
            **{k: probe[k] for k in ("alternatives", "sources", "repeat_of") if k in probe}}


def earlier_round(probes, number=1):
    return {"round": number, "theories": [], "probes": probes}


TRACKS = list(DOCS)


@pytest.mark.parametrize("answer,repeated", [("yes", True), ("no", True), ("bar:18", True), ("skip", False), ("earlier", False), ("later", False),
                                             ("none", False), ("is:groove", False), ("a", False)])
def test_only_answers_that_named_a_bar_or_said_yes_or_no_are_asked_again(answer, repeated):
    p = asked("r01-p001", TRACKS[0])
    rnd = make_round(2, DOCS, {}, {p["id"]: given(p, answer)}, [earlier_round([p])], size=10)
    assert [r["repeat_of"] for r in repeats_of(rnd)] == (["r01-p001"] if repeated else [])


def test_a_repeat_prefers_answers_where_two_sources_agree_on_a_bar_the_reviewer_did_not_name():
    sources = {"edmformer": 18, "dropdetector": 18}
    disputed = [asked(f"r01-p{i:03d}", TRACKS[i], sources=sources) for i in range(5)]
    plain = [asked(f"r01-p{i:03d}", TRACKS[i]) for i in range(5, 10)]
    named = [asked(f"r01-p{i:03d}", TRACKS[i], sources=sources) for i in range(10, 13)]   # the answer named the bar the sources agree on
    probes = disputed + plain + named
    answers = {p["id"]: given(p, "bar:18" if p in named else "yes") for p in probes}
    for seed in range(5):
        rnd = make_round(2, DOCS, {}, answers, [earlier_round(probes)], size=40, seed=seed)
        chosen = {p["repeat_of"] for p in repeats_of(rnd)}
        assert len(chosen) == 4 and chosen <= {p["id"] for p in disputed}


def test_a_repeat_that_disagrees_is_asked_a_third_time_before_anything_else():
    root = asked("r01-p001", TRACKS[0])
    again = {**asked("r02-p001", TRACKS[0]), "repeat_of": "r01-p001"}
    others = [asked(f"r01-p{i:03d}", TRACKS[i], sources={"edmformer": 18, "dropdetector": 18}) for i in range(2, 12)]
    answers = {root["id"]: given(root, "yes"), again["id"]: given(again, "no"), **{p["id"]: given(p, "yes") for p in others}}
    rnd = make_round(3, DOCS, {}, answers, [earlier_round([root, *others]), earlier_round([again], 2)], size=40)
    assert "r01-p001" in {p["repeat_of"] for p in repeats_of(rnd)}
    assert [p["repeat_of"] for p in repeats_of(rnd)].count("r01-p001") == 1


@pytest.mark.parametrize("second,pending,again", [("yes", False, False), ("bar:16", False, False), ("no", False, True), ("skip", False, True),
                                                  ("yes", True, False)])
def test_a_probe_is_not_asked_again_once_its_repeats_agree_or_while_one_waits(second, pending, again):
    root = asked("r01-p001", TRACKS[0])
    repeat = {**asked("r02-p001", TRACKS[0]), "repeat_of": "r01-p001"}
    answers = {root["id"]: given(root, "yes"), **({} if pending else {repeat["id"]: given(repeat, second)})}
    rnd = make_round(3, DOCS, {}, answers, [earlier_round([root]), earlier_round([repeat], 2)], size=10)
    assert ("r01-p001" in {p["repeat_of"] for p in repeats_of(rnd)}) is again


def test_a_track_outside_the_library_is_not_asked_again():
    p = asked("r01-p001", "f" * 16)
    assert repeats_of(make_round(2, DOCS, {}, {p["id"]: given(p, "yes")}, [earlier_round([p])], size=10)) == []


def test_a_round_asks_a_repeat_once_even_when_the_track_has_two_answers():
    a, b = asked("r01-p001", TRACKS[0]), asked("r01-p002", TRACKS[0])
    rnd = make_round(2, DOCS, {}, {p["id"]: given(p, "yes") for p in (a, b)}, [earlier_round([a, b])], size=10)
    assert len(repeats_of(rnd)) == 1


@pytest.mark.parametrize("group,kept", [
    (["yes"], "yes"), (["yes", "yes"], "yes"), (["yes", "no"], None), (["yes", "no", "yes"], "yes"), (["yes", "no", "no"], "no"),
    (["bar:18", "bar:18"], "bar:18"), (["yes", "bar:16"], "yes"), (["yes", "bar:18"], None), (["yes", "skip"], "yes"), (["skip"], "skip"),
    (["yes", "no", "bar:18"], None), (["no", "skip", "yes"], None),
])
def test_repeats_count_once_when_they_agree_and_not_at_all_until_a_third_answer_settles_them(group, kept):
    root = asked("r01-p001", TRACKS[0])
    records = {root["id"]: given(root, group[0])}
    for i, answer in enumerate(group[1:], start=2):
        records[f"r0{i}-p001"] = {**given(root, answer, i), "repeat_of": "r01-p001"}
    other = asked("r01-p002", TRACKS[1])
    out = gold_answers({**records, other["id"]: given(other, "no")})
    assert [a["answer"] for pid, a in out.items() if pid != "r01-p002"] == ([kept] if kept else [])
    assert out["r01-p002"]["answer"] == "no"


def test_repeats_stay_out_of_fresh_accuracy_and_theory_results_and_disagreements_out_of_gold():
    theory = "detectors"
    probes = [{**asked("r01-p001", TRACKS[0]), "theory": theory},
              {**asked("r01-p002", TRACKS[1]), "theory": theory}]
    again = [{**asked("r02-p001", TRACKS[0]), "theory": theory, "repeat_of": "r01-p001"},
             {**asked("r02-p002", TRACKS[1]), "theory": theory, "repeat_of": "r01-p002"}]
    rounds = [earlier_round(probes), earlier_round(again, 2)]
    answers = {p["id"]: given(p, a, 1) for p, a in zip(probes, ("yes", "yes"))}
    answers |= {p["id"]: given(p, a, 2) for p, a in zip(again, ("yes", "no"))}
    out = report(rounds, answers, DOCS)
    assert out["rounds"][0]["fresh_exact"]["start"] == 1.0 and out["rounds"][1]["fresh_exact"]["start"] is None
    assert out["theories"]["detectors"]["n"] == 1
    assert sum(b["n"] for b in out["calibration"]) == 1       # the track whose repeat disagreed counts nowhere yet


@pytest.mark.parametrize("pairs,repeats,same", [
    ([("yes", "yes")], 1, 1.0), ([("yes", "no")], 1, 0.0), ([("yes", "yes"), ("no", "yes"), ("bar:18", "bar:18")], 3, 0.667),
    ([("yes", "skip"), ("skip", "yes")], 0, None), ([("yes", "bar:16")], 1, 1.0), ([], 0, None),
])
def test_the_report_says_how_often_the_reviewer_repeats_an_answer(pairs, repeats, same):
    probes, again, answers = [], [], {}
    for i, (first, second) in enumerate(pairs):
        p = asked(f"r01-p{i:03d}", TRACKS[i])
        q = {**asked(f"r02-p{i:03d}", TRACKS[i]), "repeat_of": p["id"]}
        probes.append(p)
        again.append(q)
        answers |= {p["id"]: given(p, first), q["id"]: given(q, second, 2)}
    out = report([earlier_round(probes), earlier_round(again, 2)], answers, DOCS)
    assert out["reviewer"] == {"repeats": repeats, "same": same}


# --- the unsure bucket and calibration ---

def test_the_detectors_and_buildup_pools_get_two_turns_per_cycle_and_the_others_one():
    drops = {t: {"edmformer": [18]} for t in DOCS}
    vdj = {d["track"]["path"]: vdj_ends(14) for d in DOCS.values()}
    rnd = make_round(1, DOCS, vdj, {}, [], size=40, candidate_drops=drops)
    unsure = [p for p in rnd["probes"] if p["bucket"] == "unsure"]
    counts = {t: sum(p["theory"] == t for p in unsure) for t in ("detectors", "buildup", "grid", "labels")}
    counts["grid"] = sum(p["claim"]["kind"] == "grid" for p in unsure)
    assert counts["detectors"] >= 2 * counts["grid"] - 1 and counts["buildup"] >= 2 * counts["grid"] - 1
    assert counts["detectors"] > counts["grid"] and counts["buildup"] > counts["grid"]
    assert abs(counts["detectors"] - counts["buildup"]) <= 1


def test_calibration_draws_buildup_questions_for_drops_that_were_not_picked_for_doubt():
    drops = {t: {"edmformer": [18]} for t in DOCS}
    rnd = make_round(1, DOCS, {}, {}, [], size=24, candidate_drops=drops)
    drawn = [p for p in rnd["probes"] if p["bucket"] in ("style", "random") and p["question"] == "locate"]
    assert drawn and {p["theory"] for p in drawn} == {"calibration"}
    assert {p["prompt"] for p in drawn} == {"Tap the bar where the buildup starts"}
    assert len({p["track"] for p in rnd["probes"]}) == len(rnd["probes"])


# --- answers that mean the same ---

@pytest.mark.parametrize("kind,first,second,same", [
    ("drop", "no", "none", True), ("drop", "none", "no", True), ("buildup", "no", "none", True), ("breakdown", "no", "none", True),
    ("drop", "yes", "bar:16", True), ("drop", "no", "yes", False), ("drop", "none", "yes", False), ("drop", "no", "bar:18", False),
    ("missed", "no", "none", False), ("label", "no", "none", False),
])
def test_two_answers_to_one_probe_are_the_same_when_they_mean_the_same(kind, first, second, same):
    root = asked("r01-p001", TRACKS[0], kind=kind)
    again = {**asked("r02-p001", TRACKS[0], kind=kind), "repeat_of": "r01-p001"}
    answers = {root["id"]: given(root, first), again["id"]: given(again, second, 2)}
    assert report([earlier_round([root]), earlier_round([again], 2)], answers, DOCS)["reviewer"] == {"repeats": 1, "same": 1.0 if same else 0.0}
    assert len(gold_answers(answers)) == (1 if same else 0)


# --- the first time the beat comes in ---

def with_sections(d, *sections):
    d["sections"] = [{"label": label, "start_bar": a, "end_bar": b, "confidence": 60, "label_confidence": 40} for label, a, b in sections]
    return d


def entry_probes(*sections):
    tid, d = doc(1)
    with_sections(d, *sections)
    return tid, d, theory_candidates(tid, d, None, random.Random(0))["entry"]


@pytest.mark.parametrize("sections,drop_bars", [
    ([("intro", 0, 16), ("drop", 16, 32)], [16]),
    ([("intro", 0, 8), ("groove", 8, 16), ("drop", 16, 32)], [16]),
    ([("intro", 0, 16), ("breakdown", 16, 24), ("drop", 24, 32)], []),
    ([("intro", 0, 8), ("buildup", 8, 16), ("drop", 16, 32)], []),
    ([("intro", 0, 8), ("drop", 8, 16), ("groove", 16, 24), ("drop", 24, 32)], [8, 24]),       # no breakdown or buildup yet before either
    ([("intro", 0, 8), ("drop", 8, 16), ("breakdown", 16, 24), ("drop", 24, 32)], [8]),
    ([("breakdown", 0, 8), ("drop", 8, 16)], []),
    ([("intro", 0, 32)], []),
])
def test_a_drop_before_any_breakdown_or_buildup_is_asked_about_as_the_first_beat_entry(sections, drop_bars):
    tid, d, probes = entry_probes(*sections)
    assert [p["claim"]["bar"] for p in probes] == drop_bars
    for p in probes:
        assert (p["theory"], p["question"], p["prompt"]) == ("entry", "label", "Is this part a drop?")
        assert p["claim"] == {"kind": "label", "label": "drop", "bar": p["claim"]["bar"], "end_bar": p["claim"]["end_bar"], "time": 2.0 * p["claim"]["bar"],
                              "confidence": 40}
        assert p["plays"] == [{"id": "main", "start": max(0.0, 2.0 * (p["claim"]["bar"] - 4)), "end": 2.0 * (p["claim"]["bar"] + 4), "mark": 2.0 * p["claim"]["bar"]}]


@pytest.mark.parametrize("answer,ok,confirmed", [("yes", True, False), ("no", True, True), ("is:intro", True, True), ("is:buildup", True, True), ("skip", True, None),
                                                 ("is:chorus", False, None), ("bar:16", False, None)])
def test_the_first_beat_entry_theory_is_confirmed_by_anything_but_yes(tmp_path, answer, ok, confirmed):
    tid, d, (p,) = entry_probes(("intro", 0, 16), ("drop", 16, 32))
    write_round(tmp_path, {**ROUND, "probes": [{**p, "id": "r01-p001"}]})
    assert (record_answer(tmp_path, "r01-p001", answer)[0] == 200) == ok
    if confirmed is not None:
        assert CONFIRMS["entry"](load_answers(tmp_path)["r01-p001"]) is confirmed


def test_the_first_beat_entry_theory_names_no_source():
    assert not any(name in THEORIES["entry"].lower() for name in ("vdj", "virtualdj", "edmformer", "drop detector", "allin1", "rules"))


# --- a second hit inside a drop ---

def with_lift(*bars, sections=(("intro", 0, 8), ("drop", 8, 40))):
    tid, d = doc(1)
    with_sections(d, *sections)
    d["events"] = [*d["events"], *({"kind": "lift", "bar": b, "time": 2.0 * b, "confidence": 50} for b in bars)]
    return tid, d, theory_candidates(tid, d, None, random.Random(0))["lift"]


@pytest.mark.parametrize("lifts,sections,expected", [
    ([], (("intro", 0, 8), ("drop", 8, 40)), []),
    ([16], (("intro", 0, 8), ("drop", 8, 40)), [(8, 16, 4, 18)]),
    ([12, 18], (("intro", 0, 8), ("drop", 8, 40)), [(8, 12, 4, 14), (8, 18, 4, 20)]),
    ([10], (("intro", 0, 2), ("drop", 2, 40)), [(2, 10, 0, 12)]),                 # the clip does not start before the track
    ([19], (("intro", 0, 8), ("drop", 8, 40)), []),                               # the clip would be longer than 16 bars
    ([30], (("intro", 0, 8), ("drop", 8, 24), ("breakdown", 24, 40)), []),        # not inside a drop
    ([28], (("intro", 0, 8), ("drop", 8, 24), ("drop", 24, 40)), [(24, 28, 20, 30)]),
])
def test_a_merged_drop_is_located_in_a_clip_that_holds_its_start_and_the_lift(lifts, sections, expected):
    tid, d, probes = with_lift(*lifts, sections=sections)
    assert len(probes) == len(expected)
    for p, (start, lift, first, last) in zip(probes, expected):
        assert (p["theory"], p["question"], p["prompt"]) == ("lift", "locate", "Tap the bar where the drop starts")
        assert p["claim"] == {"kind": "drop", "bar": start, "time": 2.0 * start, "confidence": 60}
        assert p["alternatives"] == {"ours": {"bar": start}, "lift": {"bar": lift}}
        assert p["plays"] == [{"id": "main", "start": 2.0 * first, "end": 2.0 * last, "mark": None}]


@pytest.mark.parametrize("answer,exact,gold,confirmed", [
    ("bar:8", True, 8, True), ("bar:16", False, 16, False), ("bar:12", False, 12, False), ("none", False, None, False), ("skip", None, None, None),
])
def test_the_tapped_bar_says_whether_the_merge_was_right(tmp_path, answer, exact, gold, confirmed):
    tid, d, (p,) = with_lift(16)
    write_round(tmp_path, {**ROUND, "probes": [{**p, "id": "r01-p001"}]})
    assert record_answer(tmp_path, "r01-p001", answer) == (200, None)
    record = load_answers(tmp_path)["r01-p001"]
    assert (_exact(record), gold_bar(record)) == (exact, gold)
    if confirmed is not None:
        assert CONFIRMS["lift"](record) is confirmed


def test_the_lift_question_looks_like_any_locate_question_and_its_theory_gives_no_hint():
    p = {**with_lift(16)[2][0], "id": "r01-p001"}
    pub = public_round({**ROUND, "probes": [p]}, {})["probes"][0]
    assert [(o["value"], o["label"]) for o in pub["options"]] == [("none", "Not in this clip"), ("skip", "Can't tell")] and pub["tap"] is True
    for hidden in ("confidence", "bucket", "source", "alternatives", "claim", "ours", "lift"):
        assert f'"{hidden}"' not in json.dumps({**pub, "theory": ""})
    assert not any(word in THEORIES["lift"].lower() for word in ("second", "twice", "hit", "lift", "merge", "new drop", "vdj", "edmformer", "detector", "allin1", "rules"))


OLD_LIFT = {**YESNO, "id": "r05-p006", "theory": "lift", "prompt": "Does a new drop start at the marker?",
            "claim": {"kind": "missed", "bar": 8, "time": 48.0, "confidence": 50}, "none_label": "Not in this clip", "near": 24,
            "alternatives": {"yes": {"source": "lift", "bar": 24}, "none": {"source": "ours"}}}


@pytest.mark.parametrize("answer,ok", [("bar:24", True), ("bar:40", True), ("bar:41", False), ("bar:7", False), ("none", True), ("yes", True), ("skip", True)])
def test_the_lift_questions_of_round_5_keep_their_buttons_and_answers(answer, ok):
    assert (validate_answer(OLD_LIFT, answer) is None) == ok
    assert [o["value"] for o in public_round({**ROUND, "probes": [OLD_LIFT]}, {})["probes"][0]["options"]] == ["yes", "none", "skip"]


# --- labels ---

FIVE = ("intro", "buildup", "drop", "breakdown", "outro")


def test_the_label_list_and_new_answers_follow_the_labels_of_the_analysis(monkeypatch):
    monkeypatch.setattr("edm_analysis.rounds.LABELS", FIVE)
    assert public_round({**ROUND, "probes": [LABEL]}, {})["probes"][0]["labels"] == list(FIVE)
    assert validate_answer(LABEL, "is:groove") is not None and validate_answer(LABEL, "is:outro") is None


def test_a_stored_answer_with_a_label_that_is_gone_still_counts(monkeypatch):
    monkeypatch.setattr("edm_analysis.rounds.LABELS", FIVE)
    claim = {"kind": "label", "label": "drop", "bar": 16, "end_bar": 32, "time": 32.0, "confidence": 40}
    probes = [{"id": f"r01-p00{i}", "track": TRACKS[i], "theory": "labels", "question": "label", "claim": claim} for i in (1, 2)]
    answers = {p["id"]: {"answer": a, "round": 1, "track": p["track"], "claim": claim} for p, a in zip(probes, ("is:groove", "yes"))}
    out = report([earlier_round(probes)], answers, DOCS)
    assert out["rounds"][0]["fresh_exact"]["label"] == 0.5 and out["theories"]["labels"]["n"] == 2
    assert len(gold_answers(answers)) == 2


def five_part_docs(n=24):
    out = {}
    for i in range(1, n + 1):
        tid, d = doc(i)
        out[tid] = with_sections(d, ("intro", 0, 8), ("buildup", 8, 16), ("drop", 16, 24), ("breakdown", 24, 28), ("outro", 28, 32))
    return out


def label_of(p):
    return p["claim"]["label"]


def test_the_labels_pool_gets_two_turns_per_cycle():
    rnd = make_round(1, five_part_docs(), {}, {}, [], size=30)
    unsure = [p for p in rnd["probes"] if p["bucket"] == "unsure"]
    counts = {t: sum(p["theory"] == t for p in unsure) for t in ("labels", "buildup", "grid")}
    assert abs(counts["labels"] - counts["buildup"]) <= 1 and counts["labels"] >= 2 * counts["grid"] - 1 and counts["labels"] > counts["grid"]


@pytest.mark.parametrize("asked_before,first", [
    ({}, set(FIVE)),
    ({"drop": 5, "intro": 5, "breakdown": 5}, {"buildup", "outro"}),
    ({"drop": 3, "intro": 3, "breakdown": 3, "buildup": 1, "outro": 3}, {"buildup"}),
])
def test_label_questions_go_to_the_labels_asked_least(asked_before, first):
    docs = five_part_docs()
    claim = lambda label: {"kind": "label", "label": label, "bar": 16, "end_bar": 24, "time": 32.0, "confidence": 50}
    answers = {f"r00-{label}-{i}": {"answer": "yes", "round": 0, "track": "f" * 16, "claim": claim(label)} for label, n in asked_before.items() for i in range(n)}
    rnd = make_round(1, docs, {}, answers, [], size=30)
    labels = [label_of(p) for p in rnd["probes"] if p["theory"] == "labels" and p["bucket"] == "unsure"]
    assert set(labels[:len(first)]) == first
    total = {l: asked_before.get(l, 0) + labels.count(l) for l in FIVE}
    assert max(total.values()) - min(total.values()) <= max(1, max(asked_before.values(), default=0) - min(total.values()))


def test_the_random_bucket_may_draw_a_label_question_of_a_label_asked_little():
    docs = five_part_docs()
    rounds = [make_round(1, docs, {}, {}, [], size=24, seed=seed) for seed in range(8)]
    drawn = [p for r in rounds for p in r["probes"] if p["bucket"] == "random" and p["theory"] == "labels"]
    assert drawn and len({label_of(p) for p in drawn}) > 1
    for r in rounds:
        assert len({p["track"] for p in r["probes"]}) == len(r["probes"])


# --- composition with the new pools ---

def with_lifts(docs, tracks):
    out = {}
    for tid, d in docs.items():
        d = {**d, "events": [*d["events"], {"kind": "lift", "bar": 24, "time": 48.0, "confidence": 50}]} if tid in tracks else d
        out[tid] = d
    return out


def test_the_entry_pool_gets_two_turns_per_cycle_and_the_lift_pool_one():
    docs = with_lifts(DOCS, set(DOCS))
    drops = {t: {"edmformer": [18]} for t in DOCS}
    rnd = make_round(1, docs, {}, {}, [], size=33, candidate_drops=drops)
    unsure = [p for p in rnd["probes"] if p["bucket"] == "unsure"]
    counts = {t: sum(p["theory"] == t for p in unsure) for t in ("detectors", "buildup", "entry", "lift", "grid", "labels")}
    counts["grid"] = sum(p["claim"]["kind"] == "grid" for p in unsure)
    assert counts["entry"] >= 2 * counts["lift"] and counts["entry"] > counts["grid"]
    assert abs(counts["lift"] - counts["grid"]) <= 1 and counts["lift"] > 0
    assert abs(counts["labels"] - counts["entry"]) <= 1
    assert abs(counts["entry"] - counts["detectors"]) <= 1


def test_a_lift_question_waits_for_lift_events():
    rnd = make_round(1, DOCS, {}, {}, [], size=40)
    assert not [p for p in rnd["probes"] if p["theory"] == "lift"]


def test_the_random_bucket_may_draw_a_first_beat_entry_question():
    rounds = [make_round(1, DOCS, {}, {}, [], size=24, seed=seed) for seed in range(8)]
    assert [p for r in rounds for p in r["probes"] if p["bucket"] == "random" and p["theory"] == "entry"]
