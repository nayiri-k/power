#!/usr/bin/env python3
import os, sys
import argparse

exec(open(f"/tools/C/nayiri/power/scripts/util.py").read())

def main():
    parser = argparse.ArgumentParser(prog="print_headers",
                                     description="Write all signal names in FSDB waveform to file.")
    parser.add_argument('-w', '--workloads', type=str, help="Workload or space-separated list of workloads", required=True)
    parser.add_argument('--rtl', type=str, help="Name of  Chipyard Config", default='RocketConfig')
    args = parser.parse_args()

    my_workloads = args.workloads.split()
    for w in my_workloads:
        wp = get_waveform_path(w,args.rtl)
        output_file = get_outfile_path('headers',w,args.rtl)
        cmd = f"./ph {wp} > {output_file}"
        print(f"Running: {cmd}")
        os.system(cmd)

if __name__ == "__main__":
    main()
