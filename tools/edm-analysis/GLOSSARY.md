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
A drop with no breakdown and no buildup before it, so the track enters on a full beat (`fusion.entry_drops`). Not relabelled until the reviewer says whether it counts as a drop.
_Avoid_: intro drop

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
