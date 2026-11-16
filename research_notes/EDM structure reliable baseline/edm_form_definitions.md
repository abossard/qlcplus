# EDM form: operational definitions of section labels and their audio cues

Scope note: findings come only from peer-reviewed journals, ISMIR/TISMIR papers, an academic-press book (cited second-hand where marked) and one master's thesis (marked, medium trust). Page numbers are journal page numbers verified against the PDFs unless marked "second-hand". Practitioner blogs, DJ glossaries and Wikipedia were seen in searches and deliberately excluded.

Source key (full citations):
- [Solberg 2014] R. T. Solberg, "'Waiting for the Bass to Drop': Correlations Between Intense Emotional Experiences and Production Techniques in Build-up and Drop Sections of Electronic Dance Music", *Dancecult* 6(1): 61–82, 2014. DOI 10.12801/1947-5403.2014.06.01.04. https://dj.dancecult.net/index.php/dancecult/article/view/451
- [Solberg & Dibben 2019] R. T. Solberg, N. Dibben, "Peak Experiences with Electronic Dance Music: Subjective Experiences, Physiological Responses, and Musical Characteristics of the Break Routine", *Music Perception* 36(4): 371–389, 2019. DOI 10.1525/mp.2019.36.4.371. Open copy: https://eprints.whiterose.ac.uk/145911/
- [Solberg & Jensenius 2016/17] R. T. Solberg, A. R. Jensenius, "Pleasurable and Intersubjectively Embodied Experiences of Electronic Dance Music", *Empirical Musicology Review* 11(3–4): 301–318. https://duo.uio.no/handle/10852/55312 ; commentary: A. Greasley, *EMR* 11(3–4): 319–323.
- [Smith 2024] J. W. Smith, "Breakdowns and the Aesthetic of Disorientation in Festival-House Music", *Twentieth-Century Music* 21(3): 421–447, 2024. DOI 10.1017/S1478572223000087. https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader
- [Butler 2006] M. J. Butler, *Unlocking the Groove: Rhythm, Meter, and Musical Design in Electronic Dance Music*, Indiana University Press, 2006 (foundational, pre-2010). Full text not accessible to me; page-level claims are second-hand via Iler 2011, Vande Veire & De Bie 2018 and Solberg 2014. https://search.worldcat.org/title/61162077
- [Iler 2011] D. Iler, *Formal Devices of Trance and House Music: Breakdowns, Buildups and Anthems*, M.M. thesis, University of North Texas, 2011 (academic but not peer-reviewed). http://www.davidbardschwarz.com/pdf/Iler.final.pdf
- [Yadati 2014] K. Yadati, M. Larson, C. C. S. Liem, A. Hanjalic, "Detecting Drops in Electronic Dance Music: Content Based Approaches to a Socially Significant Music Event", *Proc. ISMIR 2014*: 143–148. https://archives.ismir.net/ismir2014/paper/000297.pdf
- [Raveform 2026] T. Kim, J. Kim, H. Kim, J. Nam, "Raveform: A Dataset of Metrical and Functional Structure Annotations for EDM Tracks in DJ Mixes", *Transactions of ISMIR* 9(1): 131–143, 2026. DOI 10.5334/tismir.288. https://transactions.ismir.net/articles/288 ; dataset https://mir-aidj.github.io/raveform/
- [EDM-CUE 2024] "Cue Point Estimation using Object Detection" (ETH Zurich; Argüello, Lanzendörfer, Wattenhofer), *Proc. ISMIR 2024*. https://zenodo.org/record/14877359/files/000043.pdf ; preprint https://arxiv.org/html/2407.06823v1 ; dataset https://huggingface.co/datasets/disco-eth/edm-cue
- [Kim 2020] T. Kim, M. Choi, E. Sacks, Y.-H. Yang, J. Nam, "A Computational Analysis of Real-World DJ Mixes using Mix-To-Track Subsequence Alignment", *Proc. ISMIR 2020*. https://program.ismir2020.net/static/final_papers/352.pdf
- [Vande Veire 2018] L. Vande Veire, T. De Bie, "From raw audio to a seamless mix: creating an automated DJ system for Drum and Bass", *EURASIP J. Audio, Speech, and Music Processing* 2018:13. DOI 10.1186/s13636-018-0134-8. https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8

## 1. How does peer-reviewed musicology define buildup, drop, breakdown (and intro, groove/core, bridge, outro)?

### Takeaway
Musicology treats breakdown, build-up and drop as one functional unit (the "break routine"): breakdown = sudden thinning, above all removal of bass and bass drum; build-up = layer-by-layer re-accumulation with rising pitch (risers/uplifters), accelerating snare subdivision and filter opening; drop = the instant bass and kick return, which starts the full-texture "core" (main groove). Intro and outro are percussion-led DJ-mixing zones. Two definitional conflicts matter for a labeler: "drop" is a point for musicologists (Yadati, Smith, Solberg) but a whole section in Raveform; and the kick is not always absent in breakdowns (house, trance).

### Cited Findings
**Break routine as a unit**
- The break routine "consists of three musical passages; breakdown, build-up, and drop". It is usually situated towards the last third of the track and creates "a sudden and large decrease and increase in the track's intensity" (p. 373). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- In the breakdown, dynamics, texture and structure are reduced until few features remain, and "Often the rhythmic framework of the groove is temporarily removed completely"; the track is then rebuilt by adding layers until the peak (the drop), with the groove reintroduced "in its layered entirety" (p. 373). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- EDM tracks "normally contain two or more break routines" (citing Butler 2006 and Snoman 2009), and the later one is meant to be the most intense (p. 373). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/); same claim (two build-up/drop pairs, second more intense) at p. 66 of [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Butler's prototypical form (second-hand): intro, buildup, anthem, breakdown, buildup, anthem, outro (Butler 2006: 221–223; chart of "DJ Stanley's formal prototype" on p. 222). [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf) p. 2; the p. 222 chart is reproduced as Fig. 1 of [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451) p. 67.

**Breakdown**
- "A breakdown is characterized by the track's texture becoming considerably thinner or even being entirely changed. Several instrument layers are removed, most importantly the bass and the bass drum" (p. 67). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- "In EDM, breakdowns are defined by their thin texture and relatively quiet volume." In festival house, "the presence of drums in house breakdowns is more variable": the kick is often removed or withheld but is sometimes one of the only elements left. [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- Raveform: breakdown is "a crucial transitional segment characterized by a sharp reduction in energy", often the lowest-energy segment, where producers commonly remove kick drums and basslines. [Raveform 2026](https://transactions.ismir.net/articles/288)
- A breakdown can be "contrasting" (new material) rather than reductive: Madeon's breakdown has sustained harmonies, pad, whole-note bass and lacks "rhythmic and percussive patterns" (p. 73). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Thesis-level (medium trust): the bass drum "is almost always present (in some manner)" in trance/house breakdowns, often as steady quarter notes, and "Often the breakdown is not a 'section' of music, but rather a point from which the buildup begins immediately" (p. 3). The breakdown removes "most of the percussion layers (and often the bass drum), rhythmic activity, and some melodic layers" (p. 4, fn. 15). [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf)

**Build-up**
- "Buildup sections are characterized by an increase in sonic intensity typically created through the gradual addition of sound layers, filter sweeps, crescendos, and ascending pitch gestures known as risers." [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- In the build-up "different instrument layers are built up one after one", with "the rhythmic structures being constantly compressed" (p. 67). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Raveform: the buildup's "primary function is to progressively increase both energy and tension"; it typically occurs once per track. [Raveform 2026](https://transactions.ismir.net/articles/288)
- Butler (second-hand, 2006: 226): trance and progressive house "typically feature especially climactic builds" using snare drum rolls (with "regimented diminution") and crescendo; this obviousness is criticized by techno fans. [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf) p. xi

**Drop**
- The drop is "symbolised by dropping down the bass and bass drum"; then "the main groove returns with its regular rhythmic and melodic structures" (pp. 67–68). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- The name stems from "the beat, bass or bass drum being 'dropped' back into the groove" (p. 374). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- "a drop occurs after a build, a building up of tension, and is followed by the re-introduction of the full bassline" (p. 143). Annotated drop = "the point where the buildup ends and the bassline is re-introduced" (p. 146). [Yadati 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Smith uses "drop" for "the instantaneous and climactic moment that low frequencies (such as the kick drum) return", usually the start of a core (fn. 39). [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- Contrast: Raveform labels Drop as a section, "The main section of the track, conveying its core musical idea", "the most energetic and danceable part, usually featuring all instruments together"; Drop and Breakdown can occur several times per track. [Raveform 2026](https://transactions.ismir.net/articles/288)

**Core / main groove**
- Core = "the formal sections that have high intensity and contain most or all of the sound elements in a track". [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- "Core" is "the track's main groove" (p. 70). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451); D&B layout (citing Butler 2006: 289): the drop is "the beginning of the main section or core", comparable to a pop chorus (p. 6, Fig. 2). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)

**Intro, outro, bridge, cooldown (MIR dataset definitions)**
- Intro: percussive, with "straightforward rhythmic patterns and minimal harmonic content", typically four-on-the-floor, "specifically designed by producers to facilitate seamless mixing by DJs". Outro "shares many sonic characteristics with an intro, typically featuring percussive sounds"; percussion dominance separates Outro from Cooldown. Ambient-Intro/Ambient-Outro = intro/outro "without percussive elements". [Raveform 2026](https://transactions.ismir.net/articles/288)
- Cooldown: "the inverse of a buildup, gradually reducing energy and tension". Bridge: "analogous to that of a bridge in popular music", near the middle, after a breakdown and before another breakdown or drop, "generally appears only once per track", rare in the data. [Raveform 2026](https://transactions.ismir.net/articles/288)
- Terminology clash: Vande Veire & De Bie call the post-core low section "a musical bridge, also called a breakdown" (Fig. 2, p. 6), whereas Raveform keeps Bridge and Breakdown separate. [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8); [Raveform 2026](https://transactions.ismir.net/articles/288)
- Raveform defines Energy as "a combination of loudness, rhythmic density, frequency density, and instrumentation", and Tension separately (rhythmic unpredictability, high-frequency timbre). [Raveform 2026](https://transactions.ismir.net/articles/288)

### Inferences
- Operational label set that reconciles the sources: Intro (percussive, low harmonic content, kick usually present) / Ambient-Intro (no percussion); Breakdown (sharp energy drop; bass and usually kick removed; house/trance may keep kick); Buildup (monotonic rise in energy and tension; risers, snare diminution, filter opening); Drop as an event (timestamp at bass+kick return, on a downbeat) plus Core/Drop-section (high energy, full texture) that follows it; Cooldown (gradual energy decrease without percussion dominating); Bridge (rare, once, mid-track contrast); Outro (percussion-dominant) / Ambient-Outro. A pipeline should emit both a drop-point and a drop-section to match both the musicological and the Raveform conventions.
- "Groove/main section" maps to "core" in musicology and to "Drop" in Raveform; "anthem" (Butler, Iler) is an older synonym for the post-drop core in trance/house.

### Gaps
- Butler 2006 could not be read directly; his definitions and page numbers above are second-hand. A pipeline citing Butler should verify pp. 179–201 (hypermeter) and 221–226 (form) in the book.
- No peer-reviewed definition specific to big room, hard dance (hardstyle) or dubstep drop structure was found. Yadati 2014 includes dubstep/electro/house but does not define genre-specific drops.
- "Kirill Siebert" on EDM form: not located in any indexed peer-reviewed venue; left out.

## 2. What measurable signal changes accompany each section transition?

### Takeaway
Peer-reviewed evidence converges on: a U-shaped loudness profile across the break routine; disappearance and return of low-frequency energy (bass + kick) as the strongest single marker of breakdown start and drop; rising pitch (risers/uplifters, often white noise) and low-pass filter opening across the build-up; snare/drum subdivision that accelerates into the drop; a rhythm-feature switch at the drop; and higher spectral flux/RMS in cores. Chroma- or MFCC/RMS-novelty segmentation finds candidate boundaries within a few seconds, and RMS above the track mean is a workable "core vs non-core" heuristic.

### Cited Findings
**Loudness / energy**
- The four break-routine tracks share "A characteristic U-shaped amplitude profile with a sudden decrease at the break down, gradual build up during the build-up, and maximum amplitude at the drop" (p. 379). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- Features computed per segment (pre-break, breakdown, build-up, post-drop): amplitude, RMS, spectral flux, spectrum, brightness, roughness, mode; rhythm features (beat spectrum, pulse clarity) showed no specific relation to skin conductance in seated listeners (p. 378). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- Drop sections produced significantly larger skin conductance responses than other parts of the break routine (abstract, p. 371). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- D&B segment labelling: segments with mean RMS above the song-wide mean are "high" (core), others "low" (intro, buildup, breakdown); a low-to-high transition matched a real drop in 82.4% of structurally correct songs (pp. 11, 17–18). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- Raveform Fig. 12 reports mean loudness per frequency band per label (no numeric thresholds published in text). [Raveform 2026](https://transactions.ismir.net/articles/288)

**Bass and kick (low-frequency band)**
- "Sudden removal and gradual reintroduction of several of the musical features, especially the bass and bass drum", and "Large changes in the frequency register during the break routine, especially in the transition between build-up and drop" (p. 379). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- In a build-up the kick may persist but higher-pitched, "with lesser punch" and lower in the mix; in the final build-up part bass and kick are "completely absent" and spectral focus "lies in the higher frequency area" (p. 71). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Producers "replace the high-pitched uplifter with a deep-sounding bass to set in the core" (p. 75). One drop variant: a thinning "almost empty space in the lower frequency" just before bass and kick re-enter (p. 76). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Raveform tension cues: rhythmic unpredictability, sharp or distorted high frequencies, reduced low frequencies; "silence itself can also induce tension". [Raveform 2026](https://transactions.ismir.net/articles/288)

**Risers / uplifters / filter sweeps / white noise / pitch**
- "An uplifter is a sound being gradually pitched further and further in an upward direction"; build-ups combine pitched-up synths with "the typical white noise uplifter" (p. 70). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Uplifter = "upward glissandi and/or frequency filter shaped to increase pitch until the moment of the drop" (p. 374). Pitch direction: "ascending towards the drop (often referred to as uplifter, riser, or sweep), then descending to mark the drop" (p. 379). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- Ways the drop was announced in four house tracks: drum roll ending in syncopated snare hits; withholding the drop by "holding and repeating the last measures before the drop"; opening "the low-pass filter completely" (p. 380). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- Cores are "preceded by anacrustic filter sweeps and drum fills" (analysis of "Need2Freek"). [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- The build-up "can be clearly observed in the spectrogram" before a drop (Fig. 4, p. 145). [Yadati 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)

**Snare rolls / drum fills**
- "Drum roll effect": the prominent pattern, often the snare, "becomes increasingly divided until the return of the core, starting out with quarter notes and culminating in a drum roll right before the bass drops"; in "Body" it occupies "the four last bars of the build-up", with rising volume; it gives "the illusion and the effect of a tempo increase" without changing tempo (p. 70). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Variant: claps on beats 2 and 4, a roll that rises in volume and ends in "five syncopated snare drum hits" accenting off-beats (p. 73). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)

**Rhythm features and segmentation performance**
- "Due to a switch of rhythm at the drop moment", rhythm patterns/histograms are informative; classifier = SVM on MFCC + spectrogram statistics + rhythm features around candidate boundaries (p. 145). [Yadati 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- Chroma-based segmentation (12 pitch classes, 209 ms window, 139 ms hop) put a boundary on average 2.5 s from the true drop and missed under 8% of drops; mean segment length 16.5 s (pp. 144–145). Drop F1 = 0.61 at 3 s tolerance, 0.71 at 15 s (Table 3, p. 148). Dataset: 100 SoundCloud tracks (dubstep, electro, house), 190 drops, about 2–3 drops per track (Table 2, p. 146). [Yadati 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- D&B segmentation: self-similarity matrices from MFCC (texture) and RMS (energy), checkerboard-kernel novelty, peaks snapped to the nearest downbeat, peaks more than 0.4 bar from a downbeat discarded, then boundaries constrained to a multiple of 8 bars apart (pp. 10–11). Rationale: section boundaries reflect changes in texture and energy (citing Butler 2006: 290–292, 297–298). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)

**Drop timing at beat level**
- In Madeon's "Icarus" the drop is "a powerful declining bass slide being pitched down during the first beat of the next bar", with the rest of the groove withheld until the second beat ("micro vacuum moment") (p. 74). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)

**Listener-side validation (useful for lighting)**
- Dancers' quantity of motion dropped during the breakdown and rose at the drop; build-up and drop were rated most pleasurable/motivating. [Solberg & Jensenius 2016/17](https://duo.uio.no/handle/10852/55312); direction of change reported in Greasley's commentary, EMR 11(3–4): 319–323.

### Inferences
- Practical cue mapping: breakdown onset = step drop in sub-100 Hz energy and RMS on a phrase downbeat; build-up = positive slope in spectral centroid/brightness and RMS, rising onset density of snare-band transients, upward pitch glide in a noise-like band; drop = step return of low-band energy (kick + bass) on a downbeat, often preceded by up to one bar of near-silence or low-band gap; core = RMS and spectral flux above track mean.
- Because rhythm features did not track arousal in seated listeners but did help drop classification, onset/rhythm features are better used for boundary detection than for "intensity" estimation.

### Gaps
- No peer-reviewed numeric thresholds (dB drop, band edges, riser slope) for any label were found; Raveform gives only per-band means in a figure.
- No peer-reviewed measurement of how often white noise vs. pitched risers vs. filter sweeps appear per genre.
- Raveform's text does not discuss risers, snare rolls or filters explicitly.

## 3. Phrase structure: 8/16/32-bar evidence, downbeats, fills at phrase ends

### Takeaway
Strong convergent evidence: bars group in fours; layers enter/leave at 2/4/8/16/32-bar points; section lengths and DJ cue points cluster at multiples of 8 and 16 bars (16 most common); DJs treat 32 beats (8 bars) as the phrase unit for transitions. Boundaries sit on the downbeat that starts a new hypermeasure, and variations/fills sit just before it (anacrusis). Regularity is a strong prior, not a law: 2.5–5% of tracks violate it.

### Cited Findings
- EDM's basic unit is "often consisting of 2 or 4 bars", and elements "are most often introduced, changed and removed after 2, 4, 8, 16 or 32 bars" (p. 66). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)
- Stimulus selection criterion for house tracks: layers added, changed and removed "after 2, 4, 8, or 16 bars", tempo 118–128 BPM (p. 375). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- Raveform: changes tend "to occur at multiples of four or eight measures", applied as "a soft guideline rather than a hard constraint"; the most common segment length is 30 s, i.e. 16 measures at 128 BPM. Downbeats in Rekordbox grids were sometimes offset and were corrected manually. [Raveform 2026](https://transactions.ismir.net/articles/288)
- EDM-CUE: cue points "tend to strongly coincide with phrase boundaries"; 16-bar phrases most common, then 8-bar; cue points show "high occurrences ... on multiples of 8 and 16 bars"; sections with other lengths are "irregular" and DJs often cue at their start; some DJs cue on vocals or pickups rather than the phrase downbeat. 4,710 tracks, 21,461 cues from four professional DJs. [EDM-CUE 2024](https://arxiv.org/html/2407.06823v1)
- Real DJ mixes (1,557 mixes, 20,765 transitions): transition-length histogram "has peaks at every phrase" at multiples of 32 beats, which the authors call "usually considered as a phrase in the context of dance music" (Fig. 3, Fig. 6, Sec. 6.3). [Kim 2020](https://program.ismir2020.net/static/final_papers/352.pdf)
- Drum & Bass system assumptions: 4/4, "Phrases are a multiple of 8 measures long, and musical segments are an integer number of phrases long. Boundaries between segments are accentuated by large changes in musical texture" (p. 5). Only 4 of 160 songs violated the multiple-of-8 assumption; 95.3% got structurally correct annotations (p. 17). Beat-in-groups-of-four claim cites Butler 2006: 246–248. [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- "Breaks", short segments that "typically occur just before a structural boundary", produce false novelty peaks one or two measures before the true boundary and are filtered out (p. 10). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- Butler (second-hand, 2006: 189): EDM's "anacrustic orientation": variations "almost always occur just before a metrical or hypermetrical downbeat" and articulate hypermeter and form better than gradual instrumentation changes. [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf) p. 38
- Snare roll placement: in the analyzed build-up the roll fills the last four bars (p. 70). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451); Iler shows rolls that start off the hypermetric grid ("off-balance"), raising tension (p. 13). [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf)
- DJ mixing practice: a "double drop" cue is placed 16 measures before a drop so both tracks' drops align (p. 14). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- Butler (second-hand, 2006: 317–318): mixing tracks whose downbeats or segment boundaries are misaligned sounds structurally incoherent. [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8) p. 5

**Measured durations (seconds, house)**
- Break routine 32–97 s (mean 63.5 s, SD 26.6); build-ups 47–69 s in three tracks, shorter in the fourth (p. 380). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)
- "Body" (Cinnamon Chasers): Breakdown 2 02:56–03:12, Build-up 1 03:12–03:44, Build-up 2 03:44–04:00 (p. 70). [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)

**Extended mix vs radio edit**
- Intros are "specifically designed by producers to facilitate seamless mixing by DJs". Raveform excluded non-original versions from annotation. [Raveform 2026](https://transactions.ismir.net/articles/288)
- Dance tracks "commonly have a long intro and outro" so DJs can blend them (p. 1). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- Madeon's "Icarus (Extended Version)" is used as the analyzed form (p. 68), but the paper does not compare it to a radio edit. [Solberg 2014](https://dj.dancecult.net/index.php/dancecult/article/view/451)

### Inferences
- A beat/downbeat tracker plus a phrase grid of 8 bars (with 16 as the modal section length and 4 as a sub-grid) is a well-supported prior; snap candidate boundaries to the nearest downbeat and prefer multiples of 8 bars from the first strong boundary, but allow off-grid boundaries when the novelty is large (Raveform's soft rule) and expect 4-bar sub-phrases (EDM-CUE extra predictions at 4-bar intervals).
- Expect a fill/riser/silence in the last 1–4 bars before a section downbeat; detect it as an anacrusis feature that predicts, rather than marks, the boundary.
- At 128 BPM: 8 bars ≈ 15 s, 16 bars ≈ 30 s, 32 bars ≈ 60 s; at 174 BPM (D&B) 16 bars ≈ 22 s. (Arithmetic, not a source claim.)

### Gaps
- No peer-reviewed source quantifies the extended-mix vs radio-edit difference (intro/outro bar counts, number of break routines). The common "radio edit removes the DJ intro/outro" claim exists only in practitioner sources and Wikipedia, so it is left out of the findings.
- No peer-reviewed bar-count distribution per label (e.g. typical intro = 16 or 32 bars) was found; Raveform reports only overall segment-length mode.
- No source tested whether 32-bar phrasing dominates over 16 in trance/progressive vs techno.

## 4. Inter-annotator agreement and known ambiguity

### Takeaway
No EDM structure dataset reports a formal inter-annotator agreement score. Known ambiguity zones: where a breakdown turns into a build-up (often fluid), conflicting cues (no drums but a riser), gradual techno fades, whether a given moment counts as a drop at all, and cue/boundary placement in DJ practice (only 23.6% exact DJ agreement on cue points, 73.6% within 8 bars).

### Cited Findings
- Raveform: three experts in rotating annotator/verifier/judge roles over 7 months, one consolidated annotation per track, no agreement statistic; disagreements were common early and "Most disagreements were resolved through discussion"; some structures are intrinsically ambiguous, notably gradual fades in techno. [Raveform 2026](https://transactions.ismir.net/articles/288)
- Raveform: tension "can be developed within the buildup, the breakdown, or distributed across both", so tension alone does not separate the two labels. [Raveform 2026](https://transactions.ismir.net/articles/288)
- "Sometimes, however, the breakdown leads fluidly into a buildup, so that only one formal section is perceived" (Smith then uses "breakdown-buildup"); one track "subtly slips into a buildup section"; another breakdown sends "conflicting musical messages" because absent drums signal breakdown while a riser signals build-up. [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- Thesis-level rule of thumb: the breakdown "ends and the buildup begins when the music picks up from its least active point", e.g. a "bass hit" articulating a downbeat (p. 5). [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf)
- Yadati: drops labelled by a single author (about 6 hours for 100 tracks), assuming no second drop within 30 s; no agreement measured; the worst track reached only F1 0.20 at 3 s, partly because segment boundaries "do not capture the drops particularly well" (pp. 146–147). [Yadati 2014](https://archives.ismir.net/ismir2014/paper/000297.pdf)
- EDM-CUE: cue placement "is a subjective task with no clear definition", so annotations "may not contain all plausible cue points"; no agreement reported. [EDM-CUE 2024](https://arxiv.org/html/2407.06823v1)
- DJ cue-point agreement across mixes of the same track: 23.6% of pairs identical, 40.4% within 1 bar, 73.6% within 8 bars, 86.2% within 16 bars; human track-boundary disagreement in mixes has SD of about 9 s (citing prior work) (Sec. 6, incl. 6.4). [Kim 2020](https://program.ismir2020.net/static/final_papers/352.pdf)
- Structural segmentation is evaluated as "more difficult to assess because of its subjective nature" (p. 17). [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)

**Genre dependence (what varies)**
- Trance and progressive house: especially climactic builds with snare rolls and crescendo; techno fans criticize this obviousness (Butler 2006: 226, second-hand). [Iler 2011](http://www.davidbardschwarz.com/pdf/Iler.final.pdf)
- Festival house: drum presence in breakdowns is variable, enabling metric disorientation. [Smith 2024](https://www.cambridge.org/core/product/A0748DBC5425594BE0F95D9AE0676DDC/core-reader)
- Techno: gradual fades make boundaries ambiguous; techno is the largest genre in Raveform, followed by trance, drum & bass, house (plus tech house, progressive house, deep house). [Raveform 2026](https://transactions.ismir.net/articles/288)
- Drum & bass: 160–190 BPM, 8-bar phrase multiples holding for about 97% of 160 tested songs. [Vande Veire 2018](https://asmp-eurasipjournals.springeropen.com/articles/10.1186/s13636-018-0134-8)
- Musicology evidence for the break routine is mostly house and progressive/electro house (Solberg's stimuli are "contemporary house", 118–128 BPM). [Solberg & Dibben 2019](https://eprints.whiterose.ac.uk/145911/)

### Inferences
- A labeler should expose a merged "breakdown-buildup" option, or a continuous energy/tension curve, because experts themselves collapse the two; start-of-buildup is the least reliable boundary and drop onset the most reliable (the bass+kick return is a step change, and drop point detection at 3 s tolerance already reaches F1 0.61).
- Evaluate boundaries with phrase-scale tolerances (about 1 bar for drops, up to 8 bars for intro/outro or buildup onsets) rather than the 0.5 s/3 s windows common in pop-music segmentation.
- Kick-absence alone is a poor breakdown detector in house/trance; combine with bass-band energy and overall RMS.

### Gaps
- No inter-annotator agreement statistic exists for any EDM functional-structure dataset found (Raveform, EDM-CUE, Yadati).
- A MediaEval 2014 working-notes paper (CEUR-WS Vol-1263) reportedly says the presence of a drop is debatable in some tracks; seen only as a search snippet, not verified, so excluded.
- Schwarz & Fourer's DJ-mix reverse-engineering work (UnmixDB) estimates cue regions and gain/EQ curves on synthetic mixes; it does not define section labels, so it adds nothing to these definitions.
- No peer-reviewed material on big room or hard dance (hardstyle, hard trance) section conventions (e.g. hardstyle "anti-climax" drops) was found.
