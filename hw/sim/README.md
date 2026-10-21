# `hw/sim/`

**Owner:** Mujtaba and Huzaifa  
**Issues:** #2

`Makefile` targets `lint`, `format`, `verilator` and `questa`. They call `run.sh`, which does the work:

| Command (from the repo root) | What it does |
|---|---|
| `make -C hw/sim lint` | Verilator `--lint-only -Wall` on each module, then Verible lint and format check |
| `make -C hw/sim format` | Verible format in place |
| `make -C hw/sim verilator [TB=tb_<mod>]` | Builds and runs testbenches in Verilator (CI); skips `// questa-only` ones |
| `make -C hw/sim questa TB=tb_<mod>` | Runs one testbench in QuestaSim and writes `logs/<mod>.log` |

Build output goes to `build/sim/` (ignored by git). `logs/` is committed. Rules: see [`AGENTS.md`](/AGENTS.md) sections 3.2, 3.4 and 4.2.
