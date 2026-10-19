# Hardware lint and simulation (AGENTS.md section 3.2). Run from the repo root:
#   make -C hw/sim lint                       Verilator --lint-only + Verible lint/format check
#   make -C hw/sim format                     Verible format in place
#   make -C hw/sim verilator [TB=tb_<mod>]    testbenches in Verilator (CI), skips questa-only
#   make -C hw/sim questa TB=tb_<mod>         one testbench in QuestaSim, writes logs/<mod>.log
# Everything is done by run.sh; this file only names the targets.

RUN := ./run.sh

.PHONY: lint format verilator questa clean

lint:
	$(RUN) lint

format:
	$(RUN) format

verilator:
	$(RUN) verilator $(TB)

questa:
	$(RUN) questa $(TB)

clean:
	rm -rf ../../build/sim
