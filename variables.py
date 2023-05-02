import os
import yaml


N_CYCLES = 100
N = N_CYCLES  # deprecated

power_analysis_dir = "/tools/C/nayiri/power/power-analysis"
analysis_output_dir = f"{power_analysis_dir}/out"
chipyard_dir = "/tools/C/nayiri/power/chipyard-intech16-apr23"
vlsi_dir = f"{chipyard_dir}/vlsi"
power_dir = f"{vlsi_dir}/power"
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

riscv_benchmarks = [
    'dhrystone',
    'median',
    'mm',
    'mt-matmul',
    'mt-vvadd',
    'multiply',
    'pmp',
    'qsort',
    'rsort',
    'spmv',
    'towers',
    'vvadd'
]

# TODO: these are only for RocketConfig, change isa prefix based on config...
riscv_isa_tests = ["rv64ui-v-add", "rv64ui-v-addi", "rv64ui-v-addiw", "rv64ui-v-addw", "rv64ui-v-and", "rv64ui-v-andi", "rv64ui-v-auipc", "rv64ui-v-beq", "rv64ui-v-bge", "rv64ui-v-bgeu", "rv64ui-v-blt", "rv64ui-v-bltu", "rv64ui-v-bne", "rv64ui-v-fence_i", "rv64ui-v-jal", "rv64ui-v-jalr", "rv64ui-v-lb", "rv64ui-v-lbu", "rv64ui-v-ld", "rv64ui-v-lh", "rv64ui-v-lhu", "rv64ui-v-lui", "rv64ui-v-lw", "rv64ui-v-lwu", "rv64ui-v-or", "rv64ui-v-ori", "rv64ui-v-sb", "rv64ui-v-sd", "rv64ui-v-sh", "rv64ui-v-simple", "rv64ui-v-sll", "rv64ui-v-slli", "rv64ui-v-slliw", "rv64ui-v-sllw", "rv64ui-v-slt", "rv64ui-v-slti", "rv64ui-v-sltiu", "rv64ui-v-sltu", "rv64ui-v-sra", "rv64ui-v-srai", "rv64ui-v-sraiw", "rv64ui-v-sraw", "rv64ui-v-srl", "rv64ui-v-srli", "rv64ui-v-srliw", "rv64ui-v-srlw", "rv64ui-v-sub", "rv64ui-v-subw", "rv64ui-v-sw", "rv64ui-v-xor", "rv64ui-v-xori"]

# TODO: eventually add more
workload_names = riscv_benchmarks + riscv_isa_tests

workloads = {w: {} for w in workload_names}

# TODO: add which configs to pair this with? or somehow assign workloads to each config...
for w in riscv_benchmarks:
    workloads[w]['binary_path'] = f"/tools/B/nayiri/chipyard-tools/riscv-tools-install/riscv64-unknown-elf/share/riscv-tests/benchmarks/{w}.riscv"
for w in riscv_isa_tests:
    workloads[w]['binary_path'] = f"/tools/B/nayiri/chipyard-tools/riscv-tools-install/riscv64-unknown-elf/share/riscv-tests/isa/{w}"


for w in workloads:
    binary_path = workloads[w]['binary_path']
    assert os.path.exists(binary_path), f"Binary path does not exist: {binary_path}"

def get_waveform_path(workload_name, rtl='RocketConfig', power_level='rtl'):
    wp = f"{output_dir}/chipyard.TestHarness.{rtl}/{workload_name}-{power_level}.fsdb"
    if not os.path.exists(wp):
        print("WARNING: waveform path does not exist:",wp)
    return wp

def reload():
    exec(open(f"{power_analysis_dir}/util.py").read())
    return

def get_fsdb_header_path(w):
    wp = get_waveform_path(w)
    outdir = f"{power_analysis_dir}/fsdb/out/headers"
    os.makedirs(outdir, exist_ok=True)
    output_file = f"{outdir}/{os.path.basename(wp).replace('fsdb', 'txt')}"
    return output_file

# def get_fsdb_idcodes_path(w):
#     wp = get_waveform_path(w)
#     outdir = f"{power_analysis_dir}/fsdb/out/idcodes"
#     os.makedirs(outdir, exist_ok=True)
#     output_file = f"{outdir}/{os.path.basename(wp).replace('fsdb', 'txt')}"
#     return output_file


def get_fsdb_idcodes_path(w):
    if w == 'all':
        name = 'all-rtl.txt'
    else:
        name = os.path.basename(get_waveform_path(w)).replace('fsdb', 'txt')
    outdir = f"{power_analysis_dir}/fsdb/out/idcodes"
    os.makedirs(outdir, exist_ok=True)
    output_file = f"{outdir}/{name}"
    return output_file

def get_fsdb_sig_widths_path(w):
    name = os.path.basename(get_waveform_path(w)).replace('fsdb', 'txt')
    outdir = f"{power_analysis_dir}/fsdb/out/signal_widths"
    os.makedirs(outdir, exist_ok=True)
    output_file = f"{outdir}/{name}"
    return output_file

def get_module_data_path(module=None, type='toggles'):
    dir_path = f"{analysis_output_dir}/module-data"
    if type in ['toggles','jpower']:
        filename = f"module_{module}-{type}.npy"
    elif type == 'summary':
        filename = 'summary.json'
    return f"{dir_path}/{filename}"
