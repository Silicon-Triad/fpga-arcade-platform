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

# Audio clock (#116): audio_pll outclk_0 = 12.288135 MHz (SR-AUD-01, App. A AUD_MCLK_HZ), also
# from derive_pll_clocks above. pix_clk and aud_clk are asynchronous; every crossing between them
# is a documented synchronizer inside tone_generator.sv (start_meta_q/start_sync_q,
# enable_meta_q/enable_q, mute_meta_q/mute_q), R-4.2.5.
set_clock_groups -asynchronous \
    -group [get_clocks {u_pll|*}] \
    -group [get_clocks {u_audio_pll|*}]

# The audio reset synchronizer's asynchronous input (audio PLL lock, arcade_top.sv), same as above.
set_false_path -to [get_pins -compatibility_mode {*aud_rst_sync_q[*]|clrn}]
