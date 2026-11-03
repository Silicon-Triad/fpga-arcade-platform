# `hw/quartus/`

**Owner:** Mujtaba  
**Issues:** #33 (closes the PLL part of #25)

Quartus Prime Lite project (`.qpf`, `.qsf` pins, `.sdc`) and the board top level `arcade_top.sv`.
Pin values only from the DE1-SoC manual tables cited in SRS §3.1.2.

Rules: see [`AGENTS.md`](/AGENTS.md).

## Files

| File | What |
|---|---|
| `arcade.qpf`, `arcade.qsf` | Project, device 5CSEMA5F31C6 (CON-01), file list, pins |
| `arcade.sdc` | 50 MHz board clock (CON-03), PLL clocks, reset-synchronizer cut |
| `arcade_top.sv` | PLL + Platform Designer system (`hw/ip/arcade_soc.qsys`) + `hw/rtl/arcade_video.sv` |

`arcade_top.sv` is here and not in `hw/rtl/` because it instantiates Quartus-generated modules
(`arcade_soc`, `pixel_pll`) that Verilator cannot build. Everything simulatable is in
`hw/rtl/arcade_video.sv`, tested by `hw/tb/tb_arcade_video.sv`.

## Finishing the build on your own machine (#33)

These steps need Quartus, so they are done by hand. Commit what each step says, never `db/`,
`incremental_db/`, `qdb/`, `output_files/` or generated folders (R-4.2.11).

### 1. Pixel-clock PLL (`pixel_pll`)

*Tools → IP Catalog → Basic Functions → Clocks, PLLs and Resets → PLL → PLL Intel FPGA IP*
(`altera_pll`). Save it as `hw/ip/pixel_pll`, language **Verilog**.

| Setting | Value | Source |
|---|---|---|
| Reference clock | 50.0 MHz | CON-03 |
| Number of clocks | 2 | B3 |
| outclk0 | **65.000000 MHz** (Quartus should report exactly 65 MHz: 50 × 13 ÷ 10) | CON-03, SR-VGA-01 |
| outclk1 | **25.175000 MHz** requested; the actual value Quartus shows must be **25.0–25.2 MHz** | SR-VGA-10 |
| Enable locked output | yes | B4 |
| Reset input | always present; port `rst` in `pixel_pll.v`, tied low in `arcade_top` | |

Write the actual frequencies Quartus reports for both outputs into the PR. Commit the IP
source file(s) the IP Catalog writes next to `hw/ip/` (check `git status`; the `.qip`/`.ip`,
the `.v` wrapper), not its generated subfolders, and add the `.qip` line in `arcade.qsf`
(there is a commented placeholder).

### 2. Generate the Platform Designer system

Open `hw/ip/arcade_soc.qsys`, *Generate HDL* (Verilog, no simulation model), as in
`hw/ip/platform-designer-guide.md` step 4.7. This creates
`hw/ip/arcade_soc/synthesis/arcade_soc.qip` (used by `arcade.qsf`) and
`hw/ip/arcade_soc/arcade_soc_inst.v`, the full port list.

### 3. HPS ports and settings (done, from the GHRD)

`arcade_top.sv` has the HPS DDR3 and I/O ports and `u_soc` connections copied from the Terasic
DE1-SoC GHRD rev E (`ghrd_top.v`), and `arcade.qsf` has the GHRD's HPS I/O standards and DDR3
settings (instance path `u0|` changed to `u_soc|`). HPS pins need no location assignment. To
re-check against your GHRD copy:

```bash
diff <(grep -hE "HPS_|hps_" <ghrd>/*.qsf | grep -v hps_reset | sed 's/u0|/u_soc|/') <(grep -E "HPS_|u_soc\\|" hw/quartus/arcade.qsf)
```

### 4. FPGA pins: `CLOCK_50` and `VGA_*`

See the comment block at the end of `arcade.qsf`. These pins are not yet set. The SRS
cites Manual Table 3-5 (clock) and Table 3-16 (VGA) but does not list the pin numbers, so
this needs a team decision (see the #33 PR).

### 5. Compile, check, collect

```bash
quartus_sh --flow compile hw/quartus/arcade
tools/collect_reports.sh
python3 tools/check_reports.py
```

Check in the fit report that the video RAM (tilemap banks, pattern RAM, palette) uses no
more than about 33 M10K blocks (CON-04), and that timing has no negative slack. Commit
`hw/reports/`, and add the **`hw-milestone`** label to the PR (AGENTS.md section 3.4, item 4).
