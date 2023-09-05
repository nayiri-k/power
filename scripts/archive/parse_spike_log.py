#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re
'''

'''

exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

spike_folder="/tools/C/nayiri/power/chipyard-intech16-apr23/sims/spike"
cache_miss = []
with open(f"{spike_folder}/cache-miss.log", 'r') as f:
    for line in f.readlines():
        words = line.split()
        type = None
        if words[2] == 'miss':
            type = f"{words[1]}_{words[2]}"
            cache_miss.append((type, words[0], words[3]))



exec = []
with open(f"{spike_folder}/exec.log", 'r') as f:
    for line in f.readlines():
        words = line.split()
        type = None
        if len(words) >= 5:
            pc = words[2]
            inst = words[3][1:-1]
            asm = ' '.join(words[4:])
        elif len(words) == 4:
            func = words[3]

commits = []
i = 0
with open(f"{spike_folder}/commits.log", 'r') as f:
    for line in f.readlines():
        # print(line.strip())
        d = {}
        words = line.split()
        type = None
        start = r"^core\s+\d+:\s+\d+"
        addr = r"\s+(0x[0-9a-f]+)"
        inst = r"\s+\((0x[0-9a-f]+)\)"
        all = start+addr+inst

        rd = r"\s(\w+)"
        rd_x = r"\s(x\d+)"
        rd_f = r"\s(f\d+)"
        wval = addr
        # rd_wval = rd+addr

        mem = r"\s+mem"
        mem_addr = mem+addr
        mem_addr_val = mem+addr+addr

        # csr = r"\s+(c\w+)"
        csr = r"\b(c\w+)\s+(0x\w+)\b"
        
        result = re.search(all, line)

        num_words = 3
        if result is not None:
            # print(num_words)
            d['addr'] = result.group(1)
            d['inst'] = result.group(2)
            num_words += 2
            # print(num_words)

            # register
            result = re.search(all+rd_x+wval, line)  # int
            if result is None: result = re.search(all+rd_f+wval, line)  # float
            if result is not None:
                d['rd'] = result.group(3)
                d['wval'] = result.group(4)
                num_words += 2

                # rd 0x.. mem 0x..
                result = re.search(all+rd+wval+mem_addr, line)
                if result is not None:
                    d['mem_addr'] = result.group(5)
                    num_words += 2
            
            # mem 0x.. 0x..
            result = re.search(all+mem_addr_val, line)
            if result is not None:
                d['mem_addr'] = result.group(3)
                d['wval'] = result.group(4)
                num_words += 3
            
            # csr
            result = re.findall(csr, line)
            if len(result) > 0:
                d['csrs'] = result
                num_words += len(result)*2
                # print(num_words)

            assert(num_words == len(line.split()))
        else:
            print(line)
        i += 1
        
        if i > 100: 
            break
            
