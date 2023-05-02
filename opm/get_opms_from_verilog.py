#!/usr/bin/env python3
import yaml
import sys
import os
import shutil

exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

# todo: eventually add verilog as command line arg too
if len(sys.argv) == 1:
  config = 'RocketConfig'
else:
  config = sys.argv[1]

# top_vlog = f"{vlsi_dir}/generated-src/chipyard.TestHarness.{config}/chipyard.TestHarness.{config}.top.v"
top_vlog = f"{vlsi_dir}/generated-src/chipyard.TestHarness.{config}/gen-collateral/Rocket.sv"
top_vlog_orig = f"{top_vlog}_orig"
if not os.path.exists(top_vlog_orig):
    shutil.copy2(top_vlog, top_vlog_orig)

yaml_config = f"{power_analysis_dir}/opm/yaml_configs/opm_gen_{config}.yml"
os.makedirs(os.path.dirname(yaml_config),exist_ok=True)

conditions = []
with open(top_vlog_orig, 'r') as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
        line = line.strip()
        words = line.split()
        # s = "_csr_io_counters_0_inc_sets_T_"
        # s = 'csr_io_counters_'
        s = [f'_GEN_{i}' for i in [5,7,8]]
        if len(words) < 3: continue
        # print(words[0])
        idx = 0
        if words[1] in s:
            idx = 1
        elif words[2] in s:
            idx = 2
        else: continue
        # get line
        j = i
        while not ';' in line:
            j += 1
            line += ' ' + lines[j].strip()
        # remove anything after ';'
        line = line[:line.rfind(';')]
        line = ' '.join(line.split()[idx+2:])
        # remove surrounding brackets
        line = line.strip()[1:-1]
        # print(line)
        conds = line.split(',')
        # print(conds)
        # print()
        conditions += [c.strip() for c in conds]

print("Got OPMs from", top_vlog_orig)


d = {}
d["opm.signals"] = []
for i,c in enumerate(conditions):
    s = {}
    assert('\n' not in c)
    s['name'] = f"signal{i}"
    s['path'] = "ChipTop/system/tile_prci_domain/tile_reset_domain_tile/core"
    s['modules'] = "ChipTop/DigitalTop/TilePRCIDomain/RocketTile/Rocket"
    s['condition'] = c
    s['message'] = "Rocket event"
    d["opm.signals"].append(s)

with open(yaml_config, 'w') as f:
    yaml.dump(d, f, sort_keys=False)


print("Wrote OPM config to", yaml_config)