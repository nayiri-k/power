#!/usr/bin/env python3
import yaml
import sys
import os
import shutil


exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

# todo: eventually add verilog as command line arg too
if len(sys.argv) < 2:
    config = 'RocketConfig'
else:
    config = sys.argv[1]

if len(sys.argv) < 3:
    workload = ''
else:
    workload = sys.argv[2]

if len(sys.argv) < 4:
    fsdb_suffix = '.rtl'
else:
    fsdb_suffix = sys.argv[3].replace('-','.')

deltas = True
events = False

with open(f'{power_analysis_dir}/opm/yaml_configs/opm_gen_{config}.yml','r') as f:
    opm_cfg = yaml.safe_load(f)

opm_signal_configs = opm_cfg['opm.signals']
write_header = ""
opm_signal_declaration = "  input   "
opm_signal_assignment = ""
opm_signal_messages = ""
opm_counter_declarations = ""
opm_counter_resets = ""
opm_deltas_header_1 = ""
opm_deltas_header_2 = ""
opm_deltas_write = ""

def name_opm(name):
    return f"{name}_OPM"

def name_counter(name):
    return f"{name_opm(name)}_counter"


name_cond_d = {}
for i,signal_cfg in enumerate(opm_signal_configs):
    name = signal_cfg['name']
    path = signal_cfg['path']
    condition = signal_cfg['condition']
    name_cond_d[name] = condition
    # TODO: for now don't implement posedge/negedge
    message = signal_cfg['message'] + ','
    message = message.ljust(40) + f"\t({condition} {name} in {path})"
    
    opm_signal_declaration += f"\n  input   {name_opm(name)},"
    opm_signal_assignment += f"\n    .{name_opm(name)}({name_opm(name)}),"
    if deltas:
        # TODO: eventually reset these regs after every N cycles to make counter size smaller
        opm_counter_declarations += f"\n  reg [63:0] {name_counter(name)};"
        opm_counter_resets += f"{name_counter(name)} <= 64'h0; "
        # opm_deltas_header_1 += f"{signal_cfg['message']}, ".rjust(20)
        
        if deltas:
            cond_action = f"{name_counter(name)} <= {name_counter(name)} + 1;"
        elif events:
            cond_action = f"""$fwrite(fd,"%d, {message}\\n",cycle_counter);"""

        opm_signal_messages += f"""
      if ({name_opm(name)}) begin
        {cond_action}
      end
            """

    if deltas:
        opm_deltas_write += f"{name_counter(name)}, "

if deltas:
    doubles = "%d, " * len(opm_signal_configs)
    opm_deltas_write = f"""
      if (cycle_counter % `N_CYCLES == 0) begin
        $fwrite(fd,"%d, {doubles}\\n", cycle_counter, {opm_deltas_write[:-2]});
      end
    """

# logfile_path = opm_cfg['opm.logfile_path']
# also created in sim.mk
logfile_dir = f"{vlsi_dir}/output/chipyard.TestHarness.{config}/opm"
os.makedirs(logfile_dir, exist_ok=True)
if deltas:
    logfile_path = f"{logfile_dir}/{workload}{fsdb_suffix}.opm"
else:
    logfile_path = f"{vlsi_dir}/output/chipyard.TestHarness.{config}/{config}-opm_events.log"

if deltas:
    d_print = ', '.join([f"{k}: {v}" for k,v in name_cond_d.items()])
    headings = ["cycle_counter"] + list(name_cond_d.keys())
    opm_deltas_header_2 = ", ".join([n.rjust(20) for n in headings])
    write_header = f"""
    $fwrite(fd,"{{ {d_print} }}\\n");
    $fwrite(fd,"{{{opm_deltas_header_2[1:]}}}\\n");
    """

opm_declaration = f"""
module OPM(
  input   {', '.join(list(map(lambda x: name_opm(x), name_cond_d.keys())))},
  input   clock,
  input   reset
);
  reg [63:0] cycle_counter, {', '.join(list(map(lambda x: name_counter(x), name_cond_d.keys())))};

  `ifndef SYNTHESIS
  integer fd;
  initial begin
    fd = $fopen("{logfile_path}", "w");
    {write_header}
  end // initial

  final begin
    $fwrite(fd,"%d, End of simulation\\n",cycle_counter);
    $fclose(fd);
  end // final

  always @(posedge clock) begin
    if (reset) begin
      cycle_counter <= 64'h0;
      {opm_counter_resets}
    end // reset
    else begin // not reset
      cycle_counter <= cycle_counter + 1;
      {opm_signal_messages}
      {opm_deltas_write}
    end // not reset
  end // always
  `endif // SYNTHESIS
endmodule
"""

opm_instantiation = f"""\
  OPM opm ({opm_signal_assignment}
    .clock(clock_clock),
    .reset(reset)
  );
"""


modules_dict = {}  # module_name: (module_instance_name, signal_name)
signal_parent_modules = {}  # module name: [signal names]
# create modules_dict
for signal_cfg in opm_signal_configs:
    name = signal_cfg['name']
    module_instances = signal_cfg['path'].split('/')
    modules = signal_cfg['modules'].split('/')
    for m,i in zip(modules,module_instances):
        if m not in modules_dict:
            modules_dict[m] = []
        modules_dict[m].append((i, name))
    parent = modules[-1] 
    if parent not in signal_parent_modules.keys():
        signal_parent_modules[parent] = []
    signal_parent_modules[parent].append(name)




for parent_module in modules_dict:
    out_lines = []
    assign_lines = []
    insert_assign_lines = False
    top_vlog = f"{vlsi_dir}/generated-src/chipyard.TestHarness.{config}/gen-collateral/{parent_module}.sv"
    top_vlog_orig = f"{top_vlog}_orig"
    if not os.path.exists(top_vlog_orig):
        shutil.copy2(top_vlog, top_vlog_orig)
        
    with open(top_vlog_orig,'r') as f:
        lines = f.readlines()
    for line in lines:
        words = line.split()
        before = []
        after = []
        if len(words) == 0:
            pass
        elif line.startswith('module'): 
            module = words[1].replace('(','')
            after.append(f"  output        ")
            for _,name in modules_dict[module]:
                after.append(f"{name_opm(name)}, ")
            if module in signal_parent_modules.keys():
                # go until endmodule, then insert assign statements
                for name in signal_parent_modules[module]:
                    assign_lines.append(f"  assign {name_opm(name)} = {name_cond_d[name]};\n")
                insert_assign_lines = True
        elif line.startswith('endmodule'):
            if insert_assign_lines:
                before += assign_lines
                assign_lines = []
                insert_assign_lines = False
            if parent_module == 'ChipTop': 
                before.append(opm_instantiation)
        elif words[0] in modules_dict.keys():
            instantiated_module = words[0]
            mod_inst = words[1]
            for inst,name in modules_dict[instantiated_module]:
                if mod_inst == inst:
                    after.append(f"    .{name_opm(name)}({name_opm(name)}),\n")

        out_lines += before
        out_lines.append(line)
        out_lines += after

    with open(top_vlog, 'w') as f:
        if parent_module == 'ChipTop':
            f.write(opm_declaration)
        f.write(''.join(out_lines))
    print("Added OPMs to", top_vlog)