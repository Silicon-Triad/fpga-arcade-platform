# `board/deploy/`

**Owner:** Muhammad  
**Issues:** #39

Board start-up script, `arcade` user, poweroff wrapper, hardening. SRS: SC-01, SC-02, SC-06, CON-09.

## Exit status for the start-up script (#39)

After a confirmed shutdown, `arcade` exits with status **3** (`kExitShutdownRequested` in `sw/engine/shutdown.hpp`, SR-FW-13). Screen 10b is on the display and the high scores are saved. Under SC-01 the program has no root, so the root start-up script must run `poweroff` on this status, and only on this status. Status 0 (SIGINT, SIGTERM, SR-FW-14) must not power off.

Rules: see [`AGENTS.md`](/AGENTS.md).
