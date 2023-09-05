#!/usr/bin/env python3

import os 
import json
exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

power_level = 'rtl'

rtl = 'RocketConfig'

top = 'chiptop' # needs to be lowercase

for w in riscv_benchmarks:
    idcode = -1
    with open(f"./out/header-{w}-rtl.txt", 'r') as f:
        for line in f.readlines():
            words = line.split()
            if line.startswith("<Var>  wb_reg_inst[31:0]"):
                idcode = int(line.split()[2])
    assert(idcode != -1), f"Idcode not found for workload {w} in file: ./out/header-{w}-rtl.txt"
    waveform_path = get_waveform_path(w)
    cmd = f"./ds {waveform_path} {idcode}"
    print(f"Running: {cmd}")
    os.system(cmd)