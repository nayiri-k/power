import os
import yaml
import json
from textwrap import dedent

CLOCK_PERIOD = 4
N_CYCLES = 100
N = N_CYCLES  # deprecated

clock = "clock_uncore_clock"
reset = "reset_io"

power_dir = "/tools/C/nayiri/power"
scratch_dir = "/tools/scratch/nayiri/power"
scripts_dir = f"{power_dir}/scripts"
analysis_output_dir = f"{scratch_dir}/out"
chipyard_dir = "/tools/scratch/nayiri/power/chipyard-intech16-sep23"
vlsi_dir = f"{chipyard_dir}/vlsi"
build_dir = f"{vlsi_dir}/build"
output_dir = f"{vlsi_dir}/output"

##################################
#####   RTL 
##################################
rtl_config_names = [
    'TinyRocketConfig',
    'RocketConfig',
    'HwachaRocketConfig',
    'GemminiRocketConfig',
    # 'SmallBoomConfig',
    # 'MediumBoomConfig',
    # 'LargeBoomConfig',
]

def rtl_config_name(config):
    return config.replace('Config','')

rtls = {}
for c in rtl_config_names:
    rtl_name = rtl_config_name(c)
    rtls[c] = {}
    rtls[c]['name'] = rtl_name

# TODO: eventually don't have an all-to-all mapping from RTL to workload (i.e. only run certain RTL with certain workloads)



##################################
#####   WORKLOADS 
##################################

workload_types = {'bmark', 'isa', 'coremark', 
                  'torture', 'riscvdv', 'gemmini_baremetal'}
def get_workload_type(w):
    if w in riscv_benchmarks:   return 'bmark'
    if w in riscv_isa_tests:    return 'isa'
    if w in riscv_coremark:     return 'coremark'
    if w in riscv_torture:      return 'torture'
    if w in riscv_dv:           return 'riscvdv'
    if w in gemmini_baremetal:  return 'gemmini_baremetal'

riscv_benchmarks = [
    'dhrystone',
    'median',
    'mm',
    'mt-matmul',
    'mt-vvadd',
    'multiply',
    # 'pmp',
    'qsort',
    'rsort',
    'spmv',
    'towers',
    'vvadd'
]

# TODO: these are only for RocketConfig, change isa prefix based on config...
riscv_isa_tests = ["rv64ui-v-add", "rv64ui-v-addi", "rv64ui-v-addiw", "rv64ui-v-addw", "rv64ui-v-and", "rv64ui-v-andi", "rv64ui-v-auipc", "rv64ui-v-beq", "rv64ui-v-bge", "rv64ui-v-bgeu", "rv64ui-v-blt", "rv64ui-v-bltu", "rv64ui-v-bne", "rv64ui-v-fence_i", "rv64ui-v-jal", "rv64ui-v-jalr", "rv64ui-v-lb", "rv64ui-v-lbu", "rv64ui-v-ld", "rv64ui-v-lh", "rv64ui-v-lhu", "rv64ui-v-lui", "rv64ui-v-lw", "rv64ui-v-lwu", "rv64ui-v-or", "rv64ui-v-ori", "rv64ui-v-sb", "rv64ui-v-sd", "rv64ui-v-sh", "rv64ui-v-simple", "rv64ui-v-sll", "rv64ui-v-slli", "rv64ui-v-slliw", "rv64ui-v-sllw", "rv64ui-v-slt", "rv64ui-v-slti", "rv64ui-v-sltiu", "rv64ui-v-sltu", "rv64ui-v-sra", "rv64ui-v-srai", "rv64ui-v-sraiw", "rv64ui-v-sraw", "rv64ui-v-srl", "rv64ui-v-srli", "rv64ui-v-srliw", "rv64ui-v-srlw", "rv64ui-v-sub", "rv64ui-v-subw", "rv64ui-v-sw", "rv64ui-v-xor", "rv64ui-v-xori"]


riscv_coremark = ['coremark.bare']

############################
######  Torture Tests ######
############################
riscv_torture = []
torture_outputs_dir = f"{output_dir}/chipyard.harness.TestHarness.RocketConfig/torture-outputs"
if os.path.exists(torture_outputs_dir):
    for torture_dir in os.listdir(torture_outputs_dir):
        if not torture_dir.startswith('test-'): continue
        riscv_torture.append(torture_dir)

############################
###### RISCV DV TESTS ######
############################
riscv_dv = []
# riscvdv_run = "riscvdv-20230504-160810"
riscvdv_run = "riscvdv-20230509-163604"
riscvdv_outputs_dir = f"{analysis_output_dir}/riscvdv-outputs/{riscvdv_run}"
for elf_file in os.listdir(f"{riscvdv_outputs_dir}/elf"):
    if not elf_file.startswith('riscvdv-'): continue
    if not elf_file.endswith('.elf'): continue
    riscv_dv.append(elf_file.replace('.elf',''))

############################
##### GEMMINI BAREMETAL ####
############################
gemmini_build_dir = f"{power_dir}/gemmini-rocc-tests/build"
with open(f'{power_dir}/gemmini-rocc-tests/power/tiled_matmul.txt','r') as f:
    gemmini_tiled_matmul = [w.strip() for w in f.readlines()]
gemmini_baremetal = gemmini_tiled_matmul + [
    # 'gemm_tiled_matmul_ws_perf',
	'gemm_conv_perf',
	# 'gemm_conv_dw_perf', # doesn't work well on Gemmini
	# 'gemm_resadd'
]
gemmini_tiled_matmul = [w+'-baremetal' for w in gemmini_tiled_matmul]

gemmini_baremetal = [w+'-baremetal' for w in gemmini_baremetal]

# TODO: eventually add more
workload_names =    riscv_benchmarks + riscv_isa_tests + \
                    riscv_coremark + riscv_torture + riscv_dv + \
                    gemmini_baremetal

def get_waveform_path(workload_name, rtl='RocketConfig', power_level='rtl'):
    waveform_outdir = f"{output_dir}/chipyard.harness.TestHarness.{rtl}"
    w_type = get_workload_type(workload_name)
    if w_type=='torture':
        wp = f"{waveform_outdir}/torture-outputs/{workload_name}/{workload_name}.fsdb"
    elif w_type=='riscvdv':
        wp = f"{waveform_outdir}/riscvdv-outputs/{workload_name}.fsdb"
    else:
        wp = f"{waveform_outdir}/{workload_name}.fsdb"
    # if not os.path.exists(wp):
    #     print("WARNING: waveform path does not exist:",wp)
    return wp

def reload():
    exec(open(f"{power_dir}/util.py").read())
    return

def get_workload_filename(w):
    wp = get_waveform_path(w)
    return os.path.basename(wp).replace('.fsdb','')


def get_outfile_path(ftype,workload='',rtl='RocketConfig',module='ChipTop'):
    types = {'headers','idcodes','signal_widths','toggles',
            'sim_out','sim_opm',
            'proxy_signals',
            'hammer_joules_yaml_config','opm_yaml_config'}
    assert(ftype in types), f"Type must be one of: {types}"
    create_dir = True
    if ftype in {'headers','idcodes','signal_widths','toggles','proxy_signals'}:
        if ftype in {'headers','idcodes','signal_widths','toggles'}:
            outdir = f"{analysis_output_dir}/fsdb/{ftype}"
        elif ftype in {'proxy_signals'}:
            outdir = f"{analysis_output_dir}/{ftype}"
        if ftype in {'toggles'}:
            ext = 'bin'
        else:
            ext = 'txt'
        workload = '' if workload == '' else f"-{workload}"
        m = ''
        if ftype in {'idcodes','signal_widths','toggles'}:
            m = '' if module == 'ChipTop' else f"-module_{module}"
        fpath = f"{outdir}/{rtl}{workload}{m}.{ext}"
    elif ftype == 'sim_out':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/{workload}.out"
        create_dir = False
    elif ftype == 'sim_opm':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/{workload}.opm"
        create_dir = False
    elif ftype == 'hammer_joules_yaml_config':
        fpath = f"{analysis_output_dir}/yaml_configs/hammer-joules/{rtl}.yml"
    elif ftype == 'opm_yaml_config':
        fpath = f"{analysis_output_dir}/yaml_configs/opm/{rtl}.yml"
    if create_dir:
        os.makedirs('/'.join(fpath.split('/')[:-1]), exist_ok=True)
    return fpath
        

def get_generated_src_path(rtl):
    return f"{vlsi_dir}/generated-src/chipyard.harness.TestHarness.{rtl}"

def get_tmh_path(rtl):
    return f"{get_generated_src_path(rtl)}/top_module_hierarchy.json"

def get_module_data_path(module=None, type='toggles', rtl='RocketConfig'):
    outdir = f"{analysis_output_dir}/module-data/{rtl}"
    os.makedirs(outdir, exist_ok=True)
    if type in ['toggles','jpower']:
        filename = f"{module}-{type}.bin"
    elif type == 'summary':
        filename = 'summary.json'
    return f"{outdir}/{filename}"


workloads = {w: {} for w in workload_names}

riscv_tests_path = "/tools/B/nayiri/chipyard-tools/riscv-tools-install/riscv64-unknown-elf/share/riscv-tests"
for w in riscv_benchmarks:
    workloads[w]['binary_path'] = f"{riscv_tests_path}/benchmarks/{w}.riscv"
for w in riscv_isa_tests:
    workloads[w]['binary_path'] = f"{riscv_tests_path}/isa/{w}"
for w in riscv_coremark:
    workloads[w]['binary_path'] = f"{power_dir}/riscv-coremark/coremark.bare.riscv"
for w in riscv_torture:
    workloads[w]['binary_path'] = f"{torture_outputs_dir}/{w}/{w}"
for w in riscv_dv:
    workloads[w]['binary_path'] = f"{riscvdv_outputs_dir}/elf/{w}.elf"
for w in gemmini_baremetal:
    workloads[w]['binary_path'] = f"{gemmini_build_dir}/bareMetalC/{w}"

    
for w in workloads:
    workloads[w]['type'] = get_workload_type(w)
    workloads[w]['waveform_path'] = get_waveform_path(w)

# file existence
for w in workloads:
    binary_path = workloads[w]['binary_path']
    assert os.path.exists(binary_path), f"Binary path does not exist: {binary_path}"
for w in riscv_torture:
    waveform_path = get_waveform_path(w)
    assert(os.path.exists(waveform_path)), f"ERROR: FSDB file not found, {waveform_path}"

with open(f'{analysis_output_dir}/workloads.json','w') as f:
    json.dump(workloads,f,indent=2,sort_keys=False)

def get_workloads_from_args(args,rtl='RocketConfig',
                            waveforms_exist=True):
    ws = riscv_benchmarks  # default
    if len(args) > 1:
        type = args[1]
        if type == 'bmark':
            ws = riscv_benchmarks
        elif type == 'isa':
            ws = riscv_isa_tests
        elif type == 'coremark':
            ws = riscv_coremark
        elif type == 'torture':
            ws = riscv_torture
        elif type == 'riscvdv':
            ws = riscv_dv
        else:
            ws = []
            for w in args[1:]:
                ws.append(w)
    if waveforms_exist:
        for w in ws:
            wp = get_waveform_path(w,rtl)
            if not os.path.exists(wp):
                print(f"WARNING: waveform path does not exist, omitting from list, {wp}")
                ws.remove(w)
    return ws

### NOTE: THESE ARE DEPRECATED!!!! Use get_outfile_path('bin',w,rtl) instead
# def get_fsdb_header_path(w, rtl='RocketConfig'):
#     # name = get_workload_filename(w)
#     outdir = f"{analysis_output_dir}/fsdb/headers"
#     os.makedirs(outdir, exist_ok=True)
#     return f"{outdir}/{rtl}-{w}.txt"

# def get_fsdb_idcodes_path(w,rtl='RocketConfig'):
#     outdir = f"{analysis_output_dir}/fsdb/idcodes"
#     os.makedirs(outdir, exist_ok=True)
#     return f"{outdir}/{rtl}-{w}.txt"

# def get_fsdb_sig_widths_path(w,rtl='RocketConfig'):
#     # name = get_workload_filename(w)
#     outdir = f"{analysis_output_dir}/fsdb/signal_widths"
#     os.makedirs(outdir, exist_ok=True)
#     return f"{outdir}/{rtl}-{w}.txt"

# def get_toggle_bin_path(w,rtl='RocketConfig'):
#     outdir = f"{analysis_output_dir}/fsdb/bin"
#     os.makedirs(outdir, exist_ok=True)
#     return f"{outdir}/{rtl}-{w}.txt"
