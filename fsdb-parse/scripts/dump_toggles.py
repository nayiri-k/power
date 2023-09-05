#!/usr/bin/env python3
import os, sys
import argparse

exec(open("/tools/C/nayiri/power/scripts/variables.py").read())

def main():
    parser = argparse.ArgumentParser(prog="dump_toggles",
                                     description="Write all signal names in FSDB waveform to file.")
    parser.add_argument('-w', '--workloads', type=str, help="Workload or space-separated list of workloads", required=True)
    parser.add_argument('--rtl', type=str, help="Name of  Chipyard Config", default='RocketConfig')
    parser.add_argument('--start_times', type=str, help="Start dumping toggle info after this many cycles, or list of start times (one per workload in correct order)", default='')
    parser.add_argument('--module', type=str, help="Only dump signals for one instance of this module", default='ChipTop')
    parser.add_argument('--inst', type=str, help="Only dump signals for this instance", default='chiptop')
    args = parser.parse_args()

    my_workloads = args.workloads.split()
    start_times = args.start_times.split()
    use_start_times = (len(my_workloads) == len(start_times))
    for i,w in enumerate(my_workloads):
        waveform_path = get_waveform_path(w,args.rtl)
        idcode_path = get_outfile_path('idcodes',w,args.rtl,args.module)
        toggles_path = get_outfile_path('toggles',w,args.rtl,args.module)
        start_time_cycles = start_times[i] if use_start_times else ''
        cmd = f"./dt {waveform_path} {idcode_path} {toggles_path} {start_time_cycles}"
        cmd = f"""bsubq "{cmd}" """
        print(f"Running: {cmd}")
        os.system(cmd)
        os.system("rm core.*") # idk why these get generated

if __name__ == "__main__":
    main()

