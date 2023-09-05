#!/usr/bin/env python3
read_signal=False
signal_time_toggle_dict = {}
idcode = -1
time_length = 0
max_time = 0
with open('gcd.delta','r') as f:
    for line in f.readlines()[3:]:
        words = line.split()
        if len(words) == 1 and words[0].isnumeric():
            read_signal = True
            idcode = int(words[0])
            signal_time_toggle_dict[idcode] = []
        elif read_signal and len(words) == 3:
            time = int(words[0])
            if time > max_time:
                max_time = time
            signal_time_toggle_dict[idcode].append((time,float(words[2])))
            
print(signal_time_toggle_dict)