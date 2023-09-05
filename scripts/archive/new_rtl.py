'''
For each new RTL config:
    generate RTL (Verilog) and build
    for each (existing) workload:
        run RTL sim & generate waveform
    generate YAML config for RTL + all waveforms (for now I do this separately using yaml_configs/gen_yaml_configs.py)
    run power-rtl
'''

exec(open("/tools/C/nayiri/power/chipyard-tstech28/vlsi/power/variables.py").read())

opm = True
run_build = True
run_sim = False
run_power = True

# needs to be in rtls dict
new_rtls = ["RocketConfig"]
new_rtls = list(rtls.keys())  # generate for all

for rtl in new_rtls:
    build_dir = f"/tools/C/nayiri/power/chipyard-tstech28/vlsi/build/chipyard.TestHarness.{rtl}-ChipTop"
    config_cmd = f"export CONFIG={rtl}"
    build_cmd = f"make buildfile"
    touch_sim_cmd = f"touch {build_dir}/sim-rtl-rundir/sim-output-full.json"
    sim_to_power_cmd = f"make sim-rtl-to-power"
    power_rtl_cmd = f"make power-rtl"
    redo_power_rtl_cmd = f"make redo-power-rtl"

    cmd = f"{config_cmd}"
    
    if run_build:
        cmd += f" && {build_cmd}"
    
    for w in workloads:
        
        sim_rtl_cmd = f"make BINARY={workloads[w]['binary_path']} sim-rtl-debug"

        if opm:
            opm_src = f"/tools/C/nayiri/power/chipyard-tstech28/vlsi/output/chipyard.TestHarness.{rtl}/{rtl}-opm_deltas.log"
            opm_dest = f"/tools/C/nayiri/power/chipyard-tstech28/vlsi/output/chipyard.TestHarness.{rtl}/{rtl}-{w}.opm"
            sim_rtl_cmd = f"{sim_rtl_cmd} && mv {opm_src} {opm_dest}"
        
        if run_sim:
            cmd += f" && {sim_rtl_cmd}"
        
    if run_power:
        cmd += f" && {touch_sim_cmd} && {sim_to_power_cmd} && {power_rtl_cmd}"

        
    bsub_cmd = f'bsub -q bora "{cmd}"'
    print(bsub_cmd)
