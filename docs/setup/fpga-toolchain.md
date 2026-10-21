# FPGA toolchain setup

The lab has no Quartus, so **each member installs the free tools on their own machine**. No paid editions are used.

| Tool | Version | Licence |
|---|---|---|
| **Quartus Prime Lite Edition** | **25.1 or later** | Free, no licence file |
| **Platform Designer** | included with Quartus | n/a |
| **Questa–Intel FPGA Starter Edition** | the version bundled with your Quartus | Free, needs a licence file from Intel |

Quartus Prime Lite supports the DE1-SoC's Cyclone V SE device, **5CSEMA5F31C6** (SRS CON-01).

## 1. Install Quartus Prime Lite

1. Go to the Intel FPGA Download Center and choose **Quartus Prime Lite Edition**, version 25.1 or later, for Windows or Linux.
2. Install at least: **Quartus Prime**, **Cyclone V device support** and **Questa–Intel FPGA Starter Edition**.
3. Install the **USB-Blaster II** driver (Windows: from `<quartus>/drivers`; Linux: add a udev rule as Intel's documentation describes) to program the board over its USB-Blaster port.
4. Put the tools on your `PATH`, so the repo's Makefiles find them:
   - Linux: add `<install>/quartus/bin` and `<install>/questa_fse/bin` to `PATH`.
   - Windows: add the same folders (`quartus\bin64`, `questa_fse\win64`) to the user `Path`.
5. Check:
   ```bash
   quartus_sh --version
   vsim -version
   ```

## 2. Licence for Questa–Intel FPGA Starter Edition

1. Sign in to the **Intel FPGA Self-Service Licensing Center** and request the free *Questa–Intel FPGA Starter Edition* licence for your computer. It is tied to your network card's MAC address.
2. Save the licence file outside the repository. **Never commit it** (R-10.20).
3. Point Questa to it with the environment variable Intel's licensing e-mail names for your version (`LM_LICENSE_FILE` or `SALT_LICENSE_SERVER`).
4. Check: `make -C hw/sim questa TB=<any testbench>` runs without a licence error.

## 3. Daily use

| Task | Command |
|---|---|
| Lint (as CI) | `make -C hw/sim lint` |
| Testbenches in Verilator (as CI) | `make -C hw/sim verilator` |
| One testbench in QuestaSim, writing the log CI checks | `make -C hw/sim questa TB=tb_<module>` |
| Full build | `quartus_sh --flow compile hw/quartus/arcade` |
| Collect fit and timing summaries (milestone PRs) | `tools/collect_reports.sh` |

Rules for logs and reports are in `AGENTS.md` sections 3.4, 4.2 and 7.2. In short: commit `hw/sim/logs/<module>.log` (last line `PASS`) with every RTL change, and commit `hw/reports/` in the PR that closes M2 or M4, labelled `hw-milestone`.

## 4. Team note

Quartus 25.1 project files open in later versions but not always in earlier ones. If you upgrade, tell the team in your PR. **Mujtaba** decides the project's Quartus version (#33).
