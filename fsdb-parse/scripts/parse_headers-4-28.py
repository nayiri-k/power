#!/usr/bin/env python3

import os 
import json
exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

power_level = 'rtl'

rtl = 'RocketConfig'

w = 'towers'

top = 'chiptop' # needs to be lowercase

tmh = {}
tmh_file = f"{vlsi_dir}/generated-src/chipyard.TestHarness.RocketConfig/top_module_hierarchy.json"
with open(tmh_file, 'r') as f:
    tmh = json.load(f)

instance_names = {top}  # instead of tmh['instance_name'] bc needs to be lowercase
instances = tmh['instances']
while instances:
    inst_dict = instances.pop(-1)
    instance_names.add(inst_dict['instance_name'])
    instances += inst_dict['instances']

# header_dict = [i: {} for i in instance_names]

signals = set()
signal_id_dict = {}
w_signal_id_dict = {}
id_width_dict = {}
for w in riscv_benchmarks:
    id_signal_dict = {}
    mod_hier = []
    with open(get_fsdb_header_path(w), 'r') as f:
        for line in f.readlines():
            words = line.split()
            if len(words) < 1: continue
            if words[0] == '<Scope>':
                name=words[1].replace('name:','')
                mod_hier.append(name)
            elif words[0] == '<Upscope>':
                mod_hier.pop(-1)
            elif words[0] == '<Var>':
                if mod_hier[-1] not in instance_names: continue
                idcode=int(words[2])
                if idcode in id_signal_dict: continue
                signal=words[1]
                path = '/'.join(mod_hier)
                sig_path=f"{path}/{signal}"
                id_signal_dict[idcode] = sig_path
                signal_id_dict[sig_path] = idcode
                if signal.endswith(':0]'):
                    width = signal.split('[')[-1]  # name[3][31:0] --> only keep 31:0]
                    width = int(width[:-3]) # remove :0]
                    id_width_dict[idcode] = width
                else:
                    id_width_dict[idcode] = 0

    w_signal_id_dict[w] = {s:i for i,s in id_signal_dict.items()}

    new_signals = set(id_signal_dict.values())
    
    if not signals:
        signals = new_signals
    signals = signals.intersection(new_signals)

    print(w.ljust(10), len(list(id_signal_dict.keys())), len(signals))

print('\ntotal'.ljust(10), len(signals))
signal_id_dict = {s:i for s,i in signal_id_dict.items() if s in signals}
signals = [s for _,s in sorted(id_signal_dict.items()) if s in signals]

for w,signal_id_dict in w_signal_id_dict.items():
    with open(get_fsdb_idcodes_path(w),'w') as fi:
        with open(get_fsdb_sig_widths_path(w),'w') as fs:
            fi.write(f"{len(signals)} ")
            for s in signals:
                idcode = signal_id_dict[s]
                fi.write(f"{idcode} ")
                fs.write(f"{id_width_dict[idcode]} ")

with open(get_fsdb_idcodes_path('all'),'w') as f:
    f.write(f"{len(signals)}\n")
    for s in signals:
        f.write(f"{signal_id_dict[s]} {s}\n")
        