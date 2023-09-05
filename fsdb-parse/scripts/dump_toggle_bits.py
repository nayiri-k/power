#!/usr/bin/env python3

import os 

exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

power_level = 'rtl'

rtl = 'RocketConfig'

for w in riscv_benchmarks:
    waveform_path = f"{output_dir}/chipyard.TestHarness.{rtl}/{w}-{power_level}.fsdb"
    if not os.path.exists(waveform_path):
        print("WARNING: waveform path does not exist:",waveform_path)
        continue
    cmd = f"./dtb {waveform_path}"
    # cmd = f"""bsub -q bora "{cmd}" """
    print(f"Running: {cmd}")
    os.system(cmd)
    # print(cmd)