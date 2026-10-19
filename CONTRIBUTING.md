# Contributing

Welcome! This is the CS-326 FPGA Arcade Platform (group G2, Silicon Triad). This page gets you started. **The full rules are in [`AGENTS.md`](AGENTS.md)**: read it once before your first PR. It applies to humans and AI tools alike.

## 1. Set up (once)

1. Dev PC (C++, SDL2, Verilator, Verible): [`docs/setup/dev-pc.md`](docs/setup/dev-pc.md)
2. FPGA tools (Quartus Prime Lite 25.1+, Questa–Intel FPGA Starter Edition): [`docs/setup/fpga-toolchain.md`](docs/setup/fpga-toolchain.md)
3. DE1-SoC board: [`docs/setup/board.md`](docs/setup/board.md)
4. Git and GitHub: [`docs/setup/github.md`](docs/setup/github.md)

## 2. Know where things are

- Requirements: [`docs/srs/`](docs/srs/), the source of truth. **Never edit it.**
- Plan and issue list: [`docs/plan.md`](docs/plan.md). Milestones M0–M5 end on Sun 15 Nov 2026.
- Repo map and owners: `AGENTS.md` sections 2 and 8.

## 3. Daily workflow

1. Take an issue assigned to you. Read the SRS IDs it lists.
2. Branch from `main`: `git switch -c <issue>-<slug>`, for example `18-snake-rules`.
3. Code and test. Cite SRS IDs in comments and commits: `feat(snake): speed up every 5 food [SR-GAM-03]`.
4. Run `tools/ci_local.sh`. For hardware, also run QuestaSim and commit the log (`make -C hw/sim questa TB=tb_<module>`).
5. Open a PR, fill in the template (including **AI assistance**), get CI green, and request review from the area owner.
6. Once approved, squash-merge.

## 4. Things that need everyone's approval

The register map, the Arcade API, the game interface and the table schemas are **frozen** (`AGENTS.md` section 6). To change one, open an *Interface change* issue first.

## 5. Help

Ask the area owner: Muhammad (engine, games, integration), Mujtaba (bridge, renderer, HAL) or Huzaifa (VGA, input, audio, board tests).
