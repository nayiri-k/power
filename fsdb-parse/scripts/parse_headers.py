#!/usr/bin/env python3
import os, sys, json
import argparse

exec(open(f"/tools/C/nayiri/power/scripts/util.py").read())

top = 'chiptop' # needs to be lowercase

def main():
    parser = argparse.ArgumentParser(prog="parse_headers",
                                     description="Write all signal names in FSDB waveform to file.")
    parser.add_argument('-w', '--workloads', type=str, help="Workload or space-separated list of workloads", required=True)
    parser.add_argument('--rtl', type=str, help="Name of  Chipyard Config", default='RocketConfig')
    parser.add_argument('--module', type=str, help="Only dump signals for one instance of this module", default='ChipTop')
    parser.add_argument('--inst', type=str, help="Only dump signals for this instance [NOT IMPLEMENTED]", default='chiptop')
    args = parser.parse_args()

    my_workloads = args.workloads.split()

    module_insts_dict = get_module_insts_dict(args.rtl)
    module_insts_dict['ChipTop'] = ['ChipTop'] # instead of 'chiptop'
    instance = module_insts_dict[args.module][0]
    instance_paths = [inst for insts in module_insts_dict.values() for inst in insts if inst.startswith(instance)]
    instance_names = {inst.split('/')[-1] for inst in instance_paths}

    signals = set()
    signal_id_dict = {}
    w_signal_id_dict = {}
    id_width_dict = {}
    for w in my_workloads:
        id_signal_dict = {}
        inst_hier = []
        inst_hier_path = ""
        inst_hier_path_chiptop = ""
        with open(get_outfile_path('headers',w,rtl=args.rtl), 'r') as f:
            for line in f.readlines():
                words = line.split()
                if len(words) < 1: continue
                if words[0] == '<Scope>':
                    name=words[1].replace('name:','')
                    inst_hier.append(name)
                    inst_hier_path_chiptop = '/'.join(['ChipTop']+inst_hier[3:])
                    inst_hier_path = '/'.join(inst_hier)
                elif words[0] == '<Upscope>':
                    inst_hier.pop(-1)
                elif words[0] == '<Var>':
                    if not inst_hier_path.startswith('TestDriver/testHarness/chiptop'): continue
                    if inst_hier_path_chiptop not in instance_paths: continue
                    idcode=int(words[2])
                    if idcode in id_signal_dict: continue
                    signal=words[1]
                    path = inst_hier_path
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

        print(w.ljust(10), len(list(id_signal_dict.keys())), len(signals), '-->',
              get_outfile_path('idcodes',w,rtl=args.rtl,module=args.module))

    print('\ntotal'.ljust(10), len(signals), '-->',
          get_outfile_path('idcodes','all',rtl=args.rtl,module=args.module))
    signal_id_dict = {s:i for s,i in signal_id_dict.items() if s in signals}
    signals = [s for _,s in sorted(id_signal_dict.items()) if s in signals]


    for w,signal_id_dict in w_signal_id_dict.items():
        with open(get_outfile_path('idcodes',w,rtl=args.rtl,module=args.module),'w') as fi:
            with open(get_outfile_path('signal_widths',w,rtl=args.rtl,module=args.module),'w') as fs:
                fi.write(f"{len(signals)} ")
                for s in signals:
                    idcode = signal_id_dict[s]
                    fi.write(f"{idcode} ")
                    fs.write(f"{id_width_dict[idcode]} ")

    with open(get_outfile_path('idcodes','all',rtl=args.rtl,module=args.module),'w') as f:
        f.write(f"{len(signals)}\n")
        for s in signals:
            f.write(f"{signal_id_dict[s]} {s}\n")
            

if __name__ == "__main__":
    main()


        