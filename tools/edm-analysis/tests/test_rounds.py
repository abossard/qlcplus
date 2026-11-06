import json

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
    assert [o["value"] for o in pub["probes"][0]["options"]] == ["yes", "no", "earlier", "later", "skip"]
    text = json.dumps(pub)
    for hidden in ("confidence", "bucket", "source", "claim", "path"):
        assert f'"{hidden}"' not in text


# --- making and scoring rounds ---

from edm_analysis.rounds import BUCKETS, make_round, report  # noqa: E402

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
    assert not others & {p["track"] for p in first["probes"]}
    third = make_round(3, DOCS, {}, answers, [first, second], size=10)
    assert not [p for p in third["probes"] if p["theory"] == "followup"]


def test_disputed_drops_become_a_or_b_questions_in_either_order():
    vdj = {d["track"]["path"]: {"period": 0.5, "phase": 0.0, "cues": [], "breaks": [(8.0, 40.0)]} for d in DOCS.values()}
    rnd = make_round(1, DOCS, vdj, {}, [], size=20)
    ab = [p for p in rnd["probes"] if p["theory"] == "drop"]
    assert ab
    for p in ab:
        assert {a["source"] for a in p["alternatives"].values()} == {"ours", "vdj"}
        assert [pl["mark"] for pl in p["plays"]] == [p["alternatives"]["a"]["time"], p["alternatives"]["b"]["time"]]
    assert {p["alternatives"]["a"]["source"] for p in ab} == {"ours", "vdj"}


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


def test_every_answer_ever_given_stays_in_the_log(tmp_path):
    write_round(tmp_path, ROUND)
    (tmp_path / "rounds" / "answers.json").write_text(json.dumps({"r01-p003": {"answer": "no", "round": 1}}))

    record_answer(tmp_path, "r01-p001", "earlier", now="2026-11-05T10:05:00")
    record_answer(tmp_path, "r01-p001", "yes", now="2026-11-05T10:06:00")

    log = [json.loads(line) for line in (tmp_path / "rounds" / "answers-log.jsonl").read_text().splitlines()]
    assert [(e["probe"], e["answer"]) for e in log] == [("r01-p003", "no"), ("r01-p001", "earlier"), ("r01-p001", "yes")]
    assert load_answers(tmp_path)["r01-p001"]["answer"] == "yes"
