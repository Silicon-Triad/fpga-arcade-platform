<!-- Rules: AGENTS.md. Fill in every section. Title format: <type>(<area>): <summary> [<SRS IDs>] -->

## Issue
Closes #

## SRS requirements
| ID | Verification method (from SRS) | Evidence |
|---|---|---|
| | | |

## What changed
-

## How it was tested
<!-- Commands you ran and the output summary. Say plainly what you could NOT run (board, QuestaSim, Quartus). -->
```
tools/ci_local.sh
```

## Hardware evidence (hw/ PRs only)
- QuestaSim logs updated: `hw/sim/logs/<module>.log` (last line `PASS`)
- Milestone PR (M2/M4): `hw/reports/` fit and timing summaries committed, timing met: yes / no / n.a.

## Frozen interfaces
- [ ] No frozen interface changed (AGENTS.md section 6)
- [ ] Or: approved `frozen-interface` issue: #

## AI assistance
<!-- Required (R-10.17). "None" if no AI tool was used. -->
- Tool and model (name and version):
- Parts written by AI:
- Parts checked by a human:

## Done checklist (AGENTS.md section 9)
- [ ] Links its issue and stays within its scope
- [ ] SRS IDs cited in code comments, commits and this PR
- [ ] Every acceptance check passes, with evidence above
- [ ] Tests added or updated
- [ ] `tools/ci_local.sh` passes locally; CI is green
- [ ] No hardware value without an SRS citation
- [ ] Nothing in `docs/srs/` changed
- [ ] Docs updated if behaviour, commands or set-up changed
