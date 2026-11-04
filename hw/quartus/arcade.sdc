# Timing constraints for arcade_top (#33). CON-03, SR-VGA-01.
# HPS and DDR3 constraints come from the HPS IP's own .sdc files, which Platform Designer adds.

# 50 MHz board clock (CON-03): period 1 / 50 MHz = 20.000 ns.
create_clock -name CLOCK_50 -period 20.000 [get_ports {CLOCK_50}]

# PLL outputs (pixel_pll: 65.000 MHz, CON-03, SR-VGA-01; 25.0-25.2 MHz, SR-VGA-10) are derived
# from the PLL settings, so no frequency is typed in twice.
derive_pll_clocks
derive_clock_uncertainty

# The pixel-clock reset synchronizer's asynchronous input (B4 in arcade_top.sv).
# Only the asynchronous clear of those 2 flip-flops is cut; their data path stays timed.
set_false_path -to [get_pins -compatibility_mode {*rst_sync_q[*]|clrn}]
