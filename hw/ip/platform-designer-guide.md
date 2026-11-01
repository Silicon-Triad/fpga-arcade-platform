# Platform Designer system for the bridge register block (#28)

**Owner:** Mujtaba. Rules: [`AGENTS.md`](/AGENTS.md).

This is the step-by-step guide for the parts of #28 that need Quartus on your own machine: the
QuestaSim log for `bridge_regs`, and the Platform Designer system that connects the HPS
lightweight bridge to `hw/rtl/bridge_regs.sv`.

The design choices that the SRS does not give (A1–A7) are listed at the top of
`hw/rtl/bridge_regs.sv` and in the #28 PR. The settings below follow them; if a reviewer changes
one, change the matching setting here.

---

## 0. Run the QuestaSim testbench first (needed for CI)

There is **no testbench setting in Quartus** for this project. Do not use *Assignments → Settings
→ EDA Tool Settings → Simulation* (NativeLink). Testbenches run through the repo Makefile, which
also writes the log CI checks (AGENTS.md section 3.4):

```bash
make -C hw/sim lint
make -C hw/sim verilator TB=tb_bridge_regs
make -C hw/sim questa TB=tb_bridge_regs
python3 tools/check_sim_logs.py
```

The third command writes `hw/sim/logs/bridge_regs.log`; its last line must be `PASS`. Commit it,
never edit it. If `bridge_regs.sv`, `tilemap_banks.sv`, the package or the testbench change
later, run it again (the log holds a hash of each).

On Windows, run these from Git Bash with `vsim` on your `Path` (see
`docs/setup/fpga-toolchain.md`).

---

## 1. Quartus project (only if `hw/quartus/arcade.qpf` does not exist yet)

The project belongs to #33, but Platform Designer needs one to open. If it is not in the repo yet:

1. *File → New Project Wizard*.
2. Working directory: `<repo>/hw/quartus`. Project name: `arcade`. Top-level entity:
   `arcade_top` (the module comes with #33; the name is fine before it exists).
3. *Empty project*. Add no files yet.
4. Family **Cyclone V**, device **5CSEMA5F31C6** (CON-01).
5. EDA tool settings: leave every simulation entry at **None** (see step 0).
6. Finish. This creates `arcade.qpf` and `arcade.qsf`.

Decide whether to commit the `.qpf`/`.qsf` in the #28 PR or leave them for #33, and say which in
the PR. Never commit `db/`, `incremental_db/`, `output_files/` or `qdb/` (R-4.2.11);
`.gitignore` already ignores them.

---

## 2. Start from the board's HPS settings: do not type them in

The HPS component carries board-specific settings (DDR3 memory timing, HPS pin muxing, clocks).
These are hardware values the SRS does not list, so **do not enter them by hand** (R-10.10). Copy
the HPS component from the **DE1-SoC Golden Hardware Reference Design (GHRD)** on the Terasic
DE1-SoC System CD, which already has them set for this board:

1. Copy the GHRD's `.qsys` into `hw/ip/` as `arcade_soc.qsys` (keep it outside `hw/quartus/` so
   generated output lands in the ignored `hw/ip/**/synthesis/` and `simulation/` folders).
2. Open it with *Tools → Platform Designer*, with the `arcade` project open.
3. Delete what the arcade does not use (LEDs, switches, buttons, on-chip RAM, JTAG UART, and so on).
   Keep the **clock source** (`clk_0`, the 50 MHz board clock, CON-03) and the **HPS**
   (`hps_0`).
4. In `hps_0` → *FPGA Interfaces → AXI Bridges*: the **Lightweight HPS-to-FPGA** interface must
   be enabled (it is what the HAL maps at `0xFF20_0000`, §3.1.4, SR-HAL-01). Leave its width as
   the GHRD has it. The FPGA-to-HPS and full HPS-to-FPGA bridges are not used and can be
   disabled.

If you change any HPS setting the GHRD does not already have, write in the PR which one and why.

---

## 3. Make `bridge_regs` a Platform Designer component

1. *File → New Component* (Component Editor). Name `bridge_regs`, display name "Arcade bridge
   registers".
2. *Files* tab → *Synthesis Files*: add `hw/rtl/arcade_regs_pkg.sv` and `hw/rtl/bridge_regs.sv`
   (package first). Click *Analyze Synthesis Files*; top level `bridge_regs`.
3. *Signals & Interfaces* tab: set up the interfaces as below.

| Interface | Type | Signals (role) |
|---|---|---|
| `clock` | Clock Input | `clk` (clk) |
| `reset` | Reset Input, associated clock `clock`, **synchronous edges: Both**, active high | `rst` (reset) |
| `s0` | Avalon Memory Mapped Agent (Slave), associated clock `clock`, reset `reset` | `avs_address` (address), `avs_read` (read), `avs_write` (write), `avs_writedata` (writedata), `avs_byteenable` (byteenable), `avs_readdata` (readdata), `avs_waitrequest` (waitrequest) |
| `video` | Conduit | `tm_wr_*`, `swap_req`, `front_bank`, `swap_pending`, `pat_wr_*`, `pal_wr_*`, `vblank_start`, `fpga_ready`, `reset_tilemap`, `display_enable` |
| `audio` | Conduit | `tone_frequency`, `tone_duration`, `tone_start`, `audio_enable`, `audio_mute` |

4. Settings of `s0` (assumptions A1, A3):

| Setting | Value |
|---|---|
| Address units | **WORDS** |
| Read latency | **1** |
| Read wait / Write wait / Setup / Hold | **0** |
| Uses waitrequest | yes (it is a signal of `s0`; leave *waitrequestAllowance* 0) |
| Maximum pending read transactions | 0 (fixed latency, no readdatavalid) |
| Bursting | none |

The 14-bit word address gives a span of **64 KB** (0x0000–0xFFFF), which covers every §3.1.4
offset up to 0x805C.

5. *Finish*. Save the generated `bridge_regs_hw.tcl` in `hw/ip/` and commit it.

---

## 4. Build the system

1. Add **`bridge_regs`** from the IP Catalog. Name the instance `bridge_regs_0`.
2. **Clock (A2):** the block runs on the 65 MHz renderer clock (CON-03), which comes from the PLL
   in the top level (#33), not from Platform Designer. Add a **Clock Bridge** named `pix_clk`,
   export its input as `pix_clk`, and add a **Reset Bridge** `pix_rst` clocked by it, exported as
   `pix_rst`. Connect `bridge_regs_0.clock` to `pix_clk` and `bridge_regs_0.reset` to `pix_rst`.
3. **Clock crossing (A2):** add an **Avalon-MM Clock Crossing Bridge** `lw_ccb`:
   - its `s0` side on the LW master's clock (the clock already driving `hps_0`'s
     `h2f_lw_axi_master` in the GHRD), its `m0` side on `pix_clk`;
   - data width 32, symbol width 8, address width so it spans at least 64 KB;
   - connect `hps_0.h2f_lw_axi_master` → `lw_ccb.s0`, and `lw_ccb.m0` → `bridge_regs_0.s0`.

   (Connecting the LW master straight to `bridge_regs_0.s0` also works: Platform Designer then
   adds its own clock-crossing adapter. The explicit bridge makes the crossing visible.)
4. **Address map:** `bridge_regs_0.s0` at base **0x0000_0000** as seen from
   `h2f_lw_axi_master`, so register offset X is at physical `0xFF20_0000 + X` (§3.1.4). Check in
   the *Address Map* tab that 0x8050 lands on `bridge_regs_0` and that no other agent overlaps
   0x0000–0xFFFF.
5. Export the `video` and `audio` conduits of `bridge_regs_0` (double-click the *Export* column).
   Keep the HPS exports the GHRD already has (`memory`, `hps_io`).
6. *System → Show System with Platform Designer Interconnect*: there must be no errors. Fix any
   warning about clock domains before going on.
7. *Generate HDL*: Synthesis **Verilog**, simulation model **None**. Output goes under
   `hw/ip/arcade_soc/`; it is generated output: do not commit it.

---

## 5. What to commit for #28

| Commit | Do not commit |
|---|---|
| `hw/rtl/bridge_regs.sv`, `hw/tb/tb_bridge_regs.sv` | anything under `hw/ip/arcade_soc/` |
| `hw/sim/logs/bridge_regs.log` (from step 0) | `db/`, `incremental_db/`, `output_files/`, `qdb/` |
| `hw/ip/arcade_soc.qsys`, `hw/ip/bridge_regs_hw.tcl` | your Questa licence file (R-10.20) |
| optionally `hw/quartus/arcade.qpf`, `.qsf` (see step 1) | `.qsys` backups (`*.qsys.bak`) |

Wiring the exported conduits to `tilemap_banks`, `tile_renderer_pipeline`, the VGA Controller
and the PLL is the top level's job (#33). On the board, the first check is that the HAL reads
`0xA2CADE01` at `0xFF20_0000 + 0x8050` (SR-HAL-09).
