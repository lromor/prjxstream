
[fasm-spec]: https://fasm.readthedocs.io/en/stable/#

# Development

Currently, fasm parses each line of a fasm file and unmarshals it a python namedtuple `FasmLine`
which contains https://github.com/chipsalliance/fasm/blob/master/fasm/model.py

We are only interested in mocking the behavior of
https://github.com/chipsalliance/f4pga-xc-fasm/blob/master/xc_fasm/fasm2frames.py

Which through some logic, calls https://github.com/f4pga/prjxray/blob/master/prjxray/fasm_assembler.py
which starts building up all the frames and generates them. https://f4pga.readthedocs.io/projects/prjxray/en/latest/architecture/glossary.html#term-Frame


# Current workflow using f4pga

Follow https://f4pga-examples.readthedocs.io/en/latest/getting.html

Add relevant udev rules:

```
I fixed it. I had to add the following udev rule:

File: /etc/udev/rules.d/99-ftdi.rules

ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6010", MODE="0666"
For others who arrive here with different boards, change the vendor and product to match the hex values produced by lsusb. For example here is mine:

Bus 002 Device 004: ID 0403:6010 Future Technology Devices International, Ltd FT2232C/D/H Dual UART/FIFO IC
```

on nixos:

```
  services.udev.extraRules = ''
    ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6010", MODE="0660", GROUP="dialout"
  '';
```

use `openFPGALoader -b  arty_a7_35t counter_test/build/arty_35/top.bit`

## Current F4PGA Toolchain

Download relevant magical data about the architecture given:
```
export F4PGA_PACKAGES='install-xc7 xc7a50t_test xc7a100t_test xc7a200t_test xc7z010_test'
```
from:
```
https://storage.googleapis.com/symbiflow-arch-defs/artifacts/prod/foss-fpga-tools/symbiflow-arch-defs/continuous/install/${F4PGA_TIMESTAMP}/symbiflow-arch-defs-${PKG}-${F4PGA_HASH}.tar.xz
```

1. Synthesis

```
yosys -l synth.log \
      -p "read_verilog counter.v" \
      -tcl f4pga/wrappers/tcl/xc7.f4pga.tcl

```

`xc7.f4pga.tcl` is loading a bunch of plugins which can be found here:
https://github.com/chipsalliance/yosys-f4pga-plugins/tree/dfe9b1a15b494e7dd81a2b394dac30ea707ec5cc

Some env vars are also passed via env variable `INPUT_XDC_FILES` while `OUT_FASM_EXTRA` seems to
be filled from my search via some ad-hoc, per architecture changes:
https://github.com/chipsalliance/f4pga/blob/a92635302f4178d60d7ab18fc9646a6d6cf98885/f4pga/flows/platforms.yml

A final eblif is written using the env variable `OUT_EBLIF`.


2. Packing with VPR

```bash
vpr xc7/share/f4pga/arch/xc7a50t_test/arch.timing.xml build/arty_35/top.eblif \
    --device xc7a50t_test \
    --read_rr_graph xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.rr_graph.real.bin \
    --read_router_lookahead xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.lookahead.bin \
    --read_placement_delay_lookup xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.place_delay.bin \
    --pack \
    --max_router_iterations 500 \
    --routing_failure_predictor off \
    --router_high_fanout_threshold -1 \
    --constant_net_method route \
    --route_chan_width 500 \
    --router_heap bucket \
    --clock_modeling route \
    --place_delta_delay_matrix_calculation_method dijkstra \
    --place_delay_model delta \
    --router_lookahead extended_map \
    --check_route quick \
    --strict_checks off \
    --allow_dangling_combinational_nodes on \
    --disable_errors check_unbuffered_edges:check_route \
    --congested_routing_iteration_threshold 0.8 \
    --incremental_reroute_delay_ripup off \
    --base_cost_type delay_normalized_length_bounded \
    --bb_factor 10 \
    --acc_fac 0.7 \
    --astar_fac 1.8 \
    --initial_pres_fac 2.828 \
    --pres_fac_mult 1.2 \
    --check_rr_graph off \
    --suppress_warnings noisy_warnings.log,sum_pin_class:check_unbuffered_edges:load_rr_indexed_data_T_values:check_rr_node:trans_per_R:check_route:set_rr_graph_tool_comment:calculate_average_switch \
    --sdc_file build/arty_35/xc7a50t_test_sdc.sdc \
    build/arty_35
    .
```
3. I/O Placement

```bash
python3 -m f4pga.utils.xc7.create_ioplace \
    --net  build/arty_35/top.net \
    --map  xc7/share/f4pga/arch/xc7a50t_test/xc7a35tcsg324-1/pinmap.csv \
    --blif build/arty_35/top.eblif
```
4. Place Constraints

```bash
python3 -m f4pga.utils.xc7.create_place_constraints \
    --vpr_grid_map       xc7/share/f4pga/arch/xc7a50t_test/vpr_grid_map.csv \
    --part               xc7a35tcsg324-1 \
    --db_root            prjxray-db \
    --input              build/arty_35/top.ioplace \
    --blif               build/arty_35/top.eblif \
    --arch               xc7/share/f4pga/arch/xc7a50t_test/arch.timing.xml \
    --net                build/arty_35/top.net
```

5. Placement with VPR

```bash
vpr xc7/share/f4pga/arch/xc7a50t_test/arch.timing.xml build/arty_35/top.eblif \
    --device xc7a50t_test \
    --read_rr_graph xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.rr_graph.real.bin \
    --read_router_lookahead xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.lookahead.bin \
    --read_placement_delay_lookup xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.place_delay.bin \
    --place \
    --max_router_iterations 500 \
    --routing_failure_predictor off \
    --router_high_fanout_threshold -1 \
    --constant_net_method route \
    --route_chan_width 500 \
    --router_heap bucket \
    --clock_modeling route \
    --place_delta_delay_matrix_calculation_method dijkstra \
    --place_delay_model delta \
    --router_lookahead extended_map \
    --check_route quick \
    --strict_checks off \
    --allow_dangling_combinational_nodes on \
    --disable_errors check_unbuffered_edges:check_route \
    --congested_routing_iteration_threshold 0.8 \
    --incremental_reroute_delay_ripup off \
    --base_cost_type delay_normalized_length_bounded \
    --bb_factor 10 \
    --acc_fac 0.7 \
    --astar_fac 1.8 \
    --initial_pres_fac 2.828 \
    --pres_fac_mult 1.2 \
    --check_rr_graph off \
    --suppress_warnings noisy_warnings.log,sum_pin_class:check_unbuffered_edges:load_rr_indexed_data_T_values:check_rr_node:trans_per_R:check_route:set_rr_graph_tool_comment:calculate_average_switch \
    --fix_clusters build/arty_35/top.preplace \
    --sdc_file build/arty_35/xc7a50t_test_sdc.sdc \
    build/arty_35
```

6. Routing with VPR

```bash
vpr xc7/share/f4pga/arch/xc7a50t_test/arch.timing.xml build/arty_35/top.eblif \
    --device xc7a50t_test \
    --read_rr_graph xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.rr_graph.real.bin \
    --read_router_lookahead xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.lookahead.bin \
    --read_placement_delay_lookup xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.place_delay.bin \
    --route \
    --max_router_iterations 500 \
    --routing_failure_predictor off \
    --router_high_fanout_threshold -1 \
    --constant_net_method route \
    --route_chan_width 500 \
    --router_heap bucket \
    --clock_modeling route \
    --place_delta_delay_matrix_calculation_method dijkstra \
    --place_delay_model delta \
    --router_lookahead extended_map \
    --check_route quick \
    --strict_checks off \
    --allow_dangling_combinational_nodes on \
    --disable_errors check_unbuffered_edges:check_route \
    --congested_routing_iteration_threshold 0.8 \
    --incremental_reroute_delay_ripup off \
    --base_cost_type delay_normalized_length_bounded \
    --bb_factor 10 \
    --acc_fac 0.7 \
    --astar_fac 1.8 \
    --initial_pres_fac 2.828 \
    --pres_fac_mult 1.2 \
    --check_rr_graph off \
    --suppress_warnings noisy_warnings.log,sum_pin_class:check_unbuffered_edges:load_rr_indexed_data_T_values:check_rr_node:trans_per_R:check_route:set_rr_graph_tool_comment:calculate_average_switch \
    --sdc_file build/arty_35/xc7a50t_test_sdc.sdc \
    build/arty_35
```

7. Fasm Generation

```
genfasm xc7/share/f4pga/arch/xc7a50t_test/arch.timing.xml build/arty_35/top.eblif \
    --device xc7a50t_test \
    --read_rr_graph xc7/share/f4pga/arch/xc7a50t_test/rr_graph_xc7a50t_test.rr_graph.real.bin \
    --max_router_iterations 500 \
    --routing_failure_predictor off \
    --router_high_fanout_threshold -1 \
    --constant_net_method route \
    --route_chan_width 500 \
    --router_heap bucket \
    --clock_modeling route \
    --place_delta_delay_matrix_calculation_method dijkstra \
    --place_delay_model delta \
    --router_lookahead extended_map \
    --check_route quick \
    --strict_checks off \
    --allow_dangling_combinational_nodes on \
    --disable_errors check_unbuffered_edges:check_route \
    --congested_routing_iteration_threshold 0.8 \
    --incremental_reroute_delay_ripup off \
    --base_cost_type delay_normalized_length_bounded \
    --bb_factor 10 \
    --acc_fac 0.7 \
    --astar_fac 1.8 \
    --initial_pres_fac 2.828 \
    --pres_fac_mult 1.2 \
    --check_rr_graph off \
    --suppress_warnings noisy_warnings.log,sum_pin_class:check_unbuffered_edges:load_rr_indexed_data_T_values:check_rr_node:trans_per_R:check_route:set_rr_graph_tool_comment:calculate_average_switch \
    build/arty_35
```

8. Bitstream Generation

```bash
xcfasm --bit_out build/arty_35/top.bit \
       --frm2bit xc7frames2bit \
       --fn_in   build/arty_35/top.fasm \
       --emit_pudc_b_pullup \
       --sparse \
       --part_file part.yaml \
       --part      xc7a35tcsg324-1 \
       --db-root   prjxray-db/artix7
```
