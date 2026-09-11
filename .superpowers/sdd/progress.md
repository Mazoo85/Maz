# Score to picture — progress ledger

Plan: docs/superpowers/plans/2026-09-10-score-to-picture.md
Spec: docs/superpowers/specs/2026-09-10-score-to-picture-design.md
Branch: claude/text-to-film-script-bjelmd

Tasks listed complete here are DONE — do not re-dispatch them.

Task 1: complete (commits e23bb67..f5b3487, review clean; Minor: film-score.js doc comment could note the Score/Conductor binding collision)
Task 2: complete (commits ccdc171..261302c, review clean; Minors: dead `cuts.length ||` disjunct in cutTimes guard; chooseBpm range defaults use a falsy check so a literal 0 bound would fall through)
Task 3: complete (commits 3d9757f..7929881, review clean; no findings)
Task 4: complete (commits 0e6e503..eaba99d, review clean after one Important fix — boundary assertions now pin all three thresholds)
Task 5: complete (commits 257c61b..8aed127, review clean after one Important fix — the genre fallback is now covered by a negative test)
Task 6: complete (commits 599ace5..d3f49f2, review clean; Minor: a line at t=0 emits two coincident envelope points)
Task 7: complete (commits 28abb44..f8c3e97, review clean after THREE Important fixes — brief's patch crashed on supplied sections lacking startBar; the 112-bar cap made exact-duration requests come back short on fast genres; removing that cap let seconds:Infinity hang the composer forever. Default preset path proven byte-identical across 120 songs against the pre-change commit.)
Task 8: complete (commits 9835bb1..825745b, review clean; SONG FORGE's browser suite re-run by the controller — plays, exports WAV and MIDI, no page errors)
Task 9: complete (commits 5ecebf3..d8338db, review clean; two real bugs fixed — window.FilmScore collision silently clobbered the conductor, and enterShot() still referenced the deleted bed and would have thrown on the first scene cut)
