import yaml
import sys
import os
import shutil
import re


defines = """
`ifndef OPM_
    `ifdef OPM
        `define OPM_ 1
        `define PRINTF_COND_ 0
    `else
        `define OPM_ 0
    `endif // def OPM
`else
    `define OPM_ 0
`endif // not def OPM_

"""

gemmini_start_verilog = f"""
    if (_T_1 & ~{reset})
        $fwrite(32'h80000002, "EVENT %d Gemmini start\\n", cycle_counter);

"""

def add_final_cntr_to_verilog(my_rtl):
    vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/ChipTop.sv"
    vpath_orig = vpath + "_orig"
    if os.path.exists(vpath) and not os.path.exists(vpath_orig):
        shutil.copy2(vpath, vpath_orig)
    verilog = f"""
    `ifndef SYNTHESIS
    reg  [63:0]  cycle_counter;
    always @(posedge {clock}) begin
        if ({reset}) begin
            cycle_counter <= 64'h0;
        end
        else begin
            cycle_counter <= cycle_counter + 64'h1;
        end
    end // always @(posedge {clock})

    final begin
        if (`PRINTF_COND_) begin
            $fwrite(32'h80000002, "END %d\\n", cycle_counter);
        end
    end // final
    `endif // SYNTHESIS

    """
    with open(vpath_orig,'r') as sf:
        with open(vpath, 'w') as df:
            df.write(defines)
            for line in sf.readlines():
                if line.strip() == 'endmodule':
                    df.write(verilog)
                df.write(line)


def dump_opms_to_yaml(my_rtl, regr_dict):
    signals = get_proxy_signal_names(my_rtl,module=my_module)
    signal_names = [get_inst_rel_path(p,'gemmini') for p in signals]

    toggles_proxies = toggles[:,regr_dict['coef_indexes']]

    proxy_fullnames = [signals[i] for i in regr_dict['coef_indexes']]
    proxy_names = [get_inst_rel_path(p,'gemmini') for p in proxy_fullnames]

    inst_module_dict = get_inst_module_dict(my_rtl)

    d = {}
    d['opm.signals'] = []

    for i,p in enumerate(proxy_fullnames):
        dd = {}
        path = 'ChipTop/' + '/'.join(p.split('/')[3:])
        inst_path = '/'.join(path.split('/')[:-1])
        dd['signal_path'] = path
        
        insts = inst_path.split('/')
        insts = ['/'.join(insts[:i+1]) for i,_ in enumerate(insts)]
        dd['module_path'] = '/'.join([inst_module_dict[i] for i in insts])
        dd['index'] = i
        dd['message'] = "OPM"
        d['opm.signals'].append(dd.copy())
        # print(signal, path, modules)
    with open(get_outfile_path('opm_yaml_config', my_rtl),'w') as f:
        yaml.dump(d,f,sort_keys=False,indent=2)

    fpath = get_outfile_path('proxy_signals',rtl=my_rtl,module=my_module)
    
    with open(fpath,'w') as f:
        for p in sorted(proxy_fullnames):
            f.write(get_inst_rel_path(p,'gemmini',incl_parent=True)+'\n')
    print(f"Wrote proxy signals to: {fpath}")


def add_opms_to_verilog(my_rtl):

    # write out memory files to separate module files
    memory_modules = []
    vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/chipyard.harness.TestHarness.{my_rtl}.top.mems.v"
    vpath_orig = vpath + "_orig"
    if not os.path.exists(vpath_orig):
        shutil.copy2(vpath, vpath_orig)
    with open(vpath_orig, 'r') as sf:
        for line in sf.readlines():
            if line.startswith('module'):
                match = re.search(r"\bmodule\s+(\w+)\(", line.strip())
                if match: 
                    module = match.group(1)
                    memory_modules.append(module)
                    vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/{module}.sv"
                    df = open(vpath,'w')
            df.write(line)
            if line.startswith('endmodule'): df.close()
    
    yaml_config_file = get_outfile_path('opm_yaml_config', my_rtl)
    with open(yaml_config_file,'r') as f:
        opm_cfg = yaml.safe_load(f)

    opm_signal_configs = opm_cfg['opm.signals']

    module_proxies_d = {}

    signals_d = {}

    for i,signal_cfg in enumerate(opm_signal_configs):
        index = signal_cfg['index']
        wire = f"signal{index}"
        signal_path = signal_cfg['signal_path']
        bus = '['+signal_path.split('[')[-1] if ':' in signal_path else ''
        # if wire == 'signal32': print(signal,wire, bus)
        module = signal_cfg['module_path'].split('/')[-1]

        signals_d[signal_path] = {}
        signals_d[signal_path]['wire'] = wire
        signals_d[signal_path]['bus'] = bus
        signals_d[signal_path]['index'] = index
        signals_d[wire] = {}
        signals_d[wire]['bus'] = bus
        signals_d[wire]['signal_path'] = signal_path
        signals_d[wire]['index'] = index

        modules = signal_cfg['module_path'].split('/')
        instances = signal_cfg['signal_path'].split('/')[:-1]
        for m in modules:
            if m not in module_proxies_d:
                module_proxies_d[m] = {}
                module_proxies_d[m]['signals'] = set()
                module_proxies_d[m]['wires'] = set()
                module_proxies_d[m]['children'] = set()
        
        module_proxies_d[module]['signals'].add(signal_path)

        parent = modules[0]
        for m in modules[1:]:
            module_proxies_d[m]['parent'] = parent
            inst = instances[modules.index(m)]
            module_proxies_d[parent]['children'].add((m,inst,wire))
            module_proxies_d[m]['wires'].add(wire)
            parent = m


    for m in module_proxies_d:
        signals = module_proxies_d[m]['signals']
        wires = [signals_d[s]['wire'] for s in signals]
        buses = [signals_d[s]['bus'] for s in signals]
        swb = zip(signals,wires,buses)
        children = module_proxies_d[m]['children']

        wires = module_proxies_d[m]['wires']
        vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/{m}.sv"
        vpath_orig = vpath + "_orig"
        if os.path.exists(vpath) and not os.path.exists(vpath_orig):
            shutil.copy2(vpath, vpath_orig)
        outputs = ''.join([f"output {signals_d[w]['bus']} {w},\n" for w in wires])
        assigns =  ''.join([f"assign {w} = {s.split('/')[-1]};\n" for s,w,b in swb])

        wrote_outputs = False
        with open(vpath_orig,'r') as sf:
            with open(vpath, 'w') as df:
                if m == 'Gemmini':
                    df.write(defines)
                for line in sf.readlines():
                    if not wrote_outputs and (line.strip().startswith('output') or line.strip().startswith('input')):
                        df.write(outputs)
                        wrote_outputs = True
                    if line.strip() == 'endmodule':
                        df.write("`ifndef SYNTHESIS\n")
                        df.write(assigns)
                        df.write("`endif // SYNTHESIS\n")
                    df.write(line)
                    for cm,ci,w in children:
                        # Module mod_inst_name
                        if line.strip().startswith(cm) and (cm in line.split()) and (ci in line.split()):
                            # if cm == 'MulAddRecFN': 
                                # print(cm,ci)
                                # print(line)
                            # connects = ''.join([f".{w}({w}),\n" for w in module_proxies_d[cm]['wires'] if w in wires])
                            connects = f".{w}({w}),\n"
                            df.write(connects)
                    if m == 'Gemmini' and line.strip().startswith("""$fwrite(32'h80000002, "EVENT %d Gemmini start\\n", cycle_counter);"""):
                        df.write(gemmini_start_verilog)
        

    # create cycle counter and print every N cycles
    vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/ChipTop.sv"
    vpath_orig = vpath + "_orig"
    if not os.path.exists(vpath_orig):
        shutil.copy2(vpath, vpath_orig)
    c = 'DigitalTop'
    wires = module_proxies_d[c]['wires']
    signals = [signals_d[w]['signal_path'] for w in wires]
    indexes = [signals_d[w]['index'] for w in wires]
    wires = [signals_d[s]['wire'] for s in signals]
    buses = [signals_d[s]['bus'] for s in signals]
    wire_decs = ''.join([f"wire {b} {w};\n" for w,b in zip(wires,buses)])
    wrote_wires = False





    reg_decs = ''.join([f"    reg {b} {w}_reg;\n" for w,b in zip(wires,buses)])
    assigns = ''.join([f"        {w}_reg <= {w};\n" for w in wires])
    prints = ""
    for i,w,b in zip(indexes,wires,buses):
        prints += f"""
                if ((`OPM_) & ({w}_reg != {w})) begin
                    $fwrite(32'h80000002, "%d {i}\\n", cycle_counter);
                end"""

    verilog = f"""
    `ifndef SYNTHESIS

    final begin
      if (`PRINTF_COND_) begin
          $fwrite(32'h80000002, "END %d\\n", cycle_counter);
      end
    end // final

    {reg_decs}

        reg  [63:0]  cycle_counter;
        always @(posedge {clock}) begin
            if ({reset}) begin
                cycle_counter <= 64'h0;
            end
            else begin
                cycle_counter <= cycle_counter + 64'h1;
            end
        end // always @(posedge {clock})


        always @(posedge {clock}) begin
    {assigns}
            if (!{reset}) begin
                {prints}
            end // if !({reset})
        end // always @(posedge {clock})
    `endif // SYNTHESIS
    """

    with open(vpath_orig,'r') as sf:
        with open(vpath, 'w') as df:
            df.write(defines)
            for line in sf.readlines():
                if line.strip() == 'endmodule':
                    df.write(verilog)
                if not wrote_wires and line.strip().startswith('wire'):
                    df.write(wire_decs)
                    wrote_wires = True
                df.write(line)
                if line.strip().startswith(c):
                    connects = ''.join([f".{w}({w}),\n" for w in wires])
                    df.write(connects)

    # write memory files back to top.mems.v file
    vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/chipyard.harness.TestHarness.{my_rtl}.top.mems.v"
    vpath_orig = vpath + "_orig"
    if not os.path.exists(vpath_orig):
        shutil.copy2(vpath, vpath_orig)
    with open(vpath, 'w') as df:
        for m in memory_modules:
            vpath = f"{get_generated_src_path(my_rtl)}/gen-collateral/{m}.sv"
            with open(vpath,'r') as sf:
                df.write(sf.read())
    
    return opm_signal_configs