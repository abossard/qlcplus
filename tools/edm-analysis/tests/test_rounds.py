import json
import random

import pytest

from edm_analysis.rounds import current_round, load_answers, public_round, record_answer, validate_answer

YESNO = {"id": "r01-p001", "track": "abababababababab", "title": "song", "theory": "t1", "bucket": "unsure",
         "question": "yesno", "prompt": "Does the drop start at the marker?",
         "claim": {"kind": "drop", "bar": 8, "time": 16.0, "confidence": 41},
         "plays": [{"id": "main", "start": 12.0, "end": 20.0, "mark": 16.0}]}
AB = {**YESNO, "id": "r01-p002", "question": "ab", "prompt": "Which is the drop?",
      "plays": [{"id": "a", "start": 12.0, "end": 20.0, "mark": 16.0}, {"id": "b", "start": 20.0, "end": 28.0, "mark": 24.0}],
      "alternatives": {"a": {"bar": 8, "time": 16.0, "source": "ours"}, "b": {"bar": 12, "time": 24.0, "source": "vdj"}}}
LABEL = {**YESNO, "id": "r01-p003", "question": "label", "prompt": "Is this a drop?"}
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
    (YESNO, "a", False), (YESNO, "", False), (YESNO, None, False), (YESNO, 1, False),
    (AB, "a", True), (AB, "b", True), (AB, "neither", True), (AB, "yes", False),
    (LABEL, "yes", True), (LABEL, "no", True), (LABEL, "earlier", False),
    (YESNO, "bar:0", True), (YESNO, "bar:12", True), (YESNO, "bar:24", True), (YESNO, "bar:25", False),
    (YESNO, "bar:-1", False), (YESNO, "bar:08", False), (YESNO, "bar:", False), (YESNO, "bar:x", False), (YESNO, "bar:1.5", False),
    (LABEL, "is:drop", True), (LABEL, "is:groove", True), (LABEL, "is:chorus", False), (LABEL, "is:", False), (LABEL, "bar:9", False),
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
    assert [o["value"] for o in pub["probes"][0]["options"]] == ["yes", "no", "skip"]
    text = json.dumps(pub)
    for hidden in ("confidence", "bucket", "source", "claim", "path"):
        assert f'"{hidden}"' not in text


@pytest.mark.parametrize("probe,options,tap,labels", [
    (YESNO, ["yes", "no", "skip"], True, None),
    (AB, ["a", "b", "neither", "skip"], False, None),
    (LABEL, ["yes", "no", "skip"], False, ["intro", "buildup", "drop", "breakdown", "groove", "outro"]),
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

    assert {p["track"] for p in second["probes"]} == set(DOCS) - {p["track"] for p in done}


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
    (p,) = out
    assert p.get("sources") == sources


def test_only_questions_about_a_drop_carry_sources():
    tid, d = doc(1)
    d["sections"] = [d["sections"][0], {"label": "buildup", "start_bar": 12, "end_bar": 16, "confidence": 50},
                     {**d["sections"][1], "start_bar": 16}]
    out = theory_candidates(tid, d, None, random.Random(0), {"edmformer": [13, 17]})["calibration"]
    assert {p["claim"]["kind"]: p.get("sources") for p in out} == {"buildup": None, "drop": {"edmformer": 17}}


def test_the_play_does_not_start_before_the_track():
    tid, d = doc(1, drop_bar=2)
    (p,) = theory_candidates(tid, d, None, random.Random(0), {"edmformer": [4]})["detectors"]
    assert p["plays"][0]["start"] == 0.0 and p["plays"][0]["end"] == 12.0


def test_tracks_with_candidate_output_get_locate_questions_and_the_others_keep_a_or_b():
    vdj = {d["track"]["path"]: vdj_ends(24) for d in DOCS.values()}
    with_candidates = list(DOCS)[:12]
    rnd = make_round(1, DOCS, vdj, {}, [], size=24, candidate_drops={t: {"edmformer": [18]} for t in with_candidates})
    locate = [p for p in rnd["probes"] if p["question"] == "locate"]
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
    assert next(p for p in rnd["probes"] if p["question"] == "locate")["track"] == special


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
    assert set(out) == {"rounds", "theories", "calibration", "coverage", "target", "sources"}


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
    assert counts["detectors"] >= 2 * counts["labels"] and counts["detectors"] > counts["grid"]


def test_every_answer_ever_given_stays_in_the_log(tmp_path):
    write_round(tmp_path, ROUND)
    (tmp_path / "rounds" / "answers.json").write_text(json.dumps({"r01-p003": {"answer": "no", "round": 1}}))

    record_answer(tmp_path, "r01-p001", "earlier", now="2026-11-05T10:05:00")
    record_answer(tmp_path, "r01-p001", "yes", now="2026-11-05T10:06:00")

    log = [json.loads(line) for line in (tmp_path / "rounds" / "answers-log.jsonl").read_text().splitlines()]
    assert [(e["probe"], e["answer"]) for e in log] == [("r01-p003", "no"), ("r01-p001", "earlier"), ("r01-p001", "yes")]
    assert load_answers(tmp_path)["r01-p001"]["answer"] == "yes"
