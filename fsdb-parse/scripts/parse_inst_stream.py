#!/usr/bin/env python3

import os 
import json
exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

power_level = 'rtl'

rtl = 'RocketConfig'

w = 'towers'

top = 'chiptop' # needs to be lowercase

root_dir = '/tools/C/nayiri/power/power-analysis/fsdb/out/inst_stream'

with open(f'{root_dir}/{w}-{power_level}.txt','r') as f:
    for line in f.readlines():
        words = line.split()
        if len(words) == 2:
            time = words[0]
            inst = words[1]