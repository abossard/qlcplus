# EDM analysis

Offline analysis of the reviewer's own EDM library, tuned to that library and its style, producing the structure QLC+ reads for lighting.

## Language

**Library**:
The reviewer's own collection of tracks the analysis is tuned for: the analysed tracks inside the scope the reviewer chose (`library.json`), or all of them when there is none. Tracks outside stay analysed on disk, and answers already given on them stay Gold, but Review rounds, coverage and the Target look only at the Library.
_Avoid_: dataset, corpus

**Analysis**:
The automatic description of one track (grid, sections, drops, buildups, breakdowns) that QLC+ reads. It never contains Gold.
_Avoid_: final, metadata

**Claim**:
One thing the Analysis asserts about a track: its beat grid and bar 1, a drop, buildup or breakdown start, or a section label at a bar.
_Avoid_: prediction, marker

**Confidence**:
The Analysis's own estimate, in percent, that a Claim is on the exact bar (or, for a label, right). It is part of every Claim. Calibrated: Claims at 95% are exact about 95% of the time.
_Avoid_: score, certainty, sure/unsure

**Gold**:
The reviewer's answers to Probes. It is training and scoring data only, never runtime data.
_Avoid_: correction, rating, ground truth

**Silver reference**:
A hint from another tool (VirtualDJ break markers, Mixed In Key energy cues) used to estimate accuracy without the reviewer. Not truth.
_Avoid_: reference, label

**Theory**:
A stated guess about where the analysis is right or wrong for some kind of track, e.g. "drops in this Style are found one bar late". Each Theory is tested by a set of Probes.
_Avoid_: hypothesis, finding

**Probe**:
One short, targeted question to the reviewer about a single Claim, played as an excerpt around it. Never a full-song mapping.
_Avoid_: rating, review task

**Review round**:
One cycle of answering a set of Probes the system chose to test its Theories, then retuning on all Gold so far and re-running the Library.
_Avoid_: training round, batch

**Fresh accuracy**:
The share of Claims the reviewer accepts on a Review round's new tracks, judged against the Analysis from before that round's tuning. The honest measure of how good the analysis is.
_Avoid_: test score

**Gold accuracy**:
The share of Claims the current Analysis gets right across all Gold so far. Flattered by tuning on the same tracks; used to catch regressions.
_Avoid_: training score

**Style**:
A group of tracks that is shown to need its own settings or Candidate. Styles emerge from Gold; they are not taken from genre tags.
_Avoid_: genre

**Candidate**:
One competing way to produce a kind of Claim: a rule set, a model trained on Gold, or a pretrained model. Candidates are compared on Gold.
_Avoid_: algorithm, model

**Fusion**:
How the Analysis's sections and events are made from the rules and the Candidates' outputs: drops from the best evidence, a buildup before each drop, labels from what the evidence supports. Each fused Claim lists the Candidates that put it on its exact bar (`sources`). Without Candidate output the Fusion falls back to the rules. The rules' own output stays available as source "rules", the Fusion as source "fused".
_Avoid_: ensemble, merge

**Lift**:
A step inside a drop: a drop that directly follows a drop is the same drop, so the later detector or EDMFormer hit is an event of kind `lift` (bar, time, sources), not a section start and not a drop Claim.
_Avoid_: second drop

**First beat entry**:
The first drop of a track when no breakdown and no buildup comes before it. It is intro, and an event `entry` (bar, time) says where the beat comes in so QLC+ can react. It stays a drop only where the track would otherwise have none, or the intro would pass 55 % of the track.
_Avoid_: intro drop

**Buildup**:
An existence Claim: "a buildup exists before this drop". Its start is nominal (8 bars before the drop, marked `nominal`), not an exact-bar Claim, because the reviewer's named starts range from 1 to 14 bars before the drop. The final file carries `bars.ramp`, 0 outside buildups and rising to 1 at the drop.
_Avoid_: buildup start

**Label**:
One of intro, buildup, drop, breakdown, outro. There is no groove: a steady beat part is named by where it sits (intro before the first breakdown, buildup or drop; part of the drop it follows; buildup or breakdown before a drop; outro as the last section). Intro only opens a track, outro only closes it, and the last section is outro only where the beat is still running or the song fades out (else breakdown). An old answer "is:groove" still counts: it means "not the asked label, a steady beat part".
_Avoid_: groove, part type

**Energy**:
The level of a section, 1 to 10: its mean loudness against the loudest section of the track (10). The final file also carries the loudness per bar (`bars.energy`, 0 to 1) so the level inside a long section can be followed.
_Avoid_: intensity, loudness rank

**Reviewer error rate**:
The assumed share (10 %) of the reviewer's answers that are wrong, so even a perfect source reads about 0.9 on Gold. Exact rates on Gold are reported as observed and as read under this rate; the Target itself is not changed.
_Avoid_: noise, tolerance

**Target**:
The point where Review rounds stop: on fresh Probes, Claims at 95% Confidence or more are exact at least 95% of the time, and at least 70% of all Claims reach 95% Confidence.
_Avoid_: goal, threshold

## Relationships

- A **Review round** tests one or more **Theories** through **Probes**, produces **Gold**, and a new **Analysis** for every track in the **Library**
- The **Analysis** is the **Fusion** of the rules and the **Candidates**' outputs
- Each **Probe** asks about one **Claim**
- **Gold** judges **Claims**; it never flows into an **Analysis**
