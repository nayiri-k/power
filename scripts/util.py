import math
import matplotlib.pyplot as plt
from sklearn import linear_model, preprocessing
import sklearn.linear_model
from sklearn.metrics import r2_score
import re
import numpy
import numpy as np
import skglm
import sys
import scipy.sparse #import csr_matrix, hstack
import os
from sklearn.utils._testing import ignore_warnings
from sklearn.exceptions import ConvergenceWarning,UndefinedMetricWarning
import json
import networkx as nx
from textwrap import dedent
import functools


exec(open("/tools/C/nayiri/power/scripts/variables.py").read())
exec(open(f"{scripts_dir}/opm.py").read())


sys.path.append("/tools/C/nayiri/power/simmani")
import simmani
import simmani.model.clustering

def sec_to_cycle(time):
    return int(time * 1e9)  # since sim timescale is 1ns

def convert_to_sec(time, unit):
    if unit == 'us':
        time = time*1e-6
    if unit == 'ns':
        time = time*1e-9
    if unit == 'ps':
        time = time*1e-12
    if unit == 'fs':
        time = time*1e-15
    return time

def get_time_power_units(first_line):
    units_regexp = r"^.*\((.*)\)"
    time_unit = 'ps'
    power_unit ='mW'
    for cmd in first_line.split('-'):
        if cmd.startswith('xlabel'):
            time_unit = re.match(units_regexp,cmd).group(1)
        if cmd.startswith('ylabel'):
            power_unit = re.match(units_regexp,cmd).group(1)
    return time_unit, power_unit

def read_opm_file(opm_file):
    cycles = []
    deltas = []
    with open(opm_file,'r') as f:
        lines = f.readlines()
    signals_info = lines[0]
    headers = lines[1]
    # 2 lines for header, 1 line for cycle #0
    # omit last empty line and "end of simulation" line
    data = lines[3:-1]
    prev_data = list(map(int, lines[2].replace(',','').split()[1:]))
    for line in data:
        data = line.replace(',','').split()
        cycles.append(int(data[0]))
        data = list(map(int, data[1:]))
        deltas.append([i-j for i,j in zip(data,prev_data)])
        prev_data = data
    for d in deltas:
        assert(len(d) == 33), f"Wrong number of OPM signals: {opm_file}"
    return cycles, deltas

def get_report_avgpow(workload, report_file=None):
    if report_file is None: report_file = get_power_filepath(workload,output_format='report')
    with open(report_file,'r') as f:
        for line in f.readlines():
            words = line.split()
            if len(words) != 8: continue
            isnum = ''.join([c for c in words[5] if c not in '.e-+'])
            if not isnum.isnumeric(): continue
            power = float(words[5])
            instpath = words[7][1:] # remove first '/'
            if instpath == 'ChipTop':
                return power

def read_profile_file(workload="towers", rtl='RocketConfig', 
                      module='ChipTop',inst='ChipTop',num_toggles=N_CYCLES,
                      power_level='rtl', profile_file=None):
    if profile_file is None:
        profile_file = get_power_filepath(workload, rtl=rtl, module=module,
                                            inst=inst, num_toggles=num_toggles)
    if not os.path.exists(profile_file): 
        print(f"WARNING: file does not exist, {profile_file}")
        return [], []
    with open(profile_file,'r') as f:
        lines = f.readlines()
    if len(lines) == 0:  # file becomes empty when this workload is being run
        print(f"WARNING: file {profile_file} is empty")
        return [], []
    
    time_unit = 'ps'
    while lines[0].startswith('#'):
        first_line = lines.pop(0)
        time_unit, _ = get_time_power_units(first_line)

    time_power = [l.split() for l in lines]
    time = list(map(lambda x: convert_to_sec(float(x[0]), time_unit), time_power))
    cycles = list(map(lambda x: sec_to_cycle(x), time))
    # power = list(map(lambda x: sum(list(map(float, x[1:]))), time_power))
    # power = [sum([float(p) for p in l[1:]]) for l in time_power]
    time_power = [tp for tp in time_power if len(tp) == 2]
    power = list(map(lambda x: float(x[1]), time_power))
    return cycles[1:], power[1:] # throw out first value bc it's usually inaccurate

def read_togglebits_ones_file(workload):
    base_dir = "/tools/C/nayiri/power/power-analysis/fsdb/out"
    toggles = numpy.fromfile(f"{base_dir}/dump-toggle_bits/{get_workload_filename(workload)}.bin",dtype=numpy.uint16)
    ones = numpy.fromfile(f"{base_dir}/dump-ones/{get_workload_filename(workload)}.bin",dtype=numpy.uint16)
    N = np.uint32(toggles[-3]) << 16 | np.uint32(toggles[-4])
    M = np.uint32(toggles[-1]) << 16 | np.uint32(toggles[-2])
    toggles_reshape = toggles[:-4].reshape(N,-1)
    w_toggles = numpy.transpose(toggles_reshape)
    ones_reshape = ones[:-4].reshape(N,-1)
    w_ones = numpy.transpose(ones_reshape)
    return w_toggles, w_ones

def read_toggle_bin_file(workload, rtl='RocketConfig', module='ChipTop'):
    bin_file = get_outfile_path('toggles',workload,rtl,module=module)
    if not os.path.exists(bin_file): 
        print(f"WARNING: file does not exist, {bin_file}")
        return []
    
    array = numpy.fromfile(bin_file,dtype=numpy.uint16)
    
    if len(array) < 4: return []
    N = numpy.uint32(array[1]) << 16 | numpy.uint32(array[0])
    M = numpy.uint32(array[3]) << 16 | numpy.uint32(array[2])
    if (len(array) - 4) % M != 0:
        print(f"ERROR: Wrong array dimensions - ({len(array)-4}) --> ({N},{M}), {bin_file}")
    arr_reshape = array[4:].reshape(-1,M)
    w_toggles = numpy.transpose(arr_reshape)
    return w_toggles

def short_isa_names(isa_list):
    return list(map(lambda x: x.replace('rv64ui-v-',''), isa_list))


def get_workload_toggles_jpower(workload, toggles, jpower, wb):
    start,end = wb[workload]
    w_toggles = toggles[start:end,:]
    w_jpower = jpower[start:end]
    return w_toggles, w_jpower

def get_workloads_subset_toggles_jpower(workloads, toggles, jpower, wb):
    for workload in workloads:
        start,end = wb[workload]
        if workload == workloads[0]:
            w_toggles = toggles[start:end,:]
            w_jpower = jpower[start:end]
        else:
            w_toggles = np.concatenate((w_toggles,toggles[start:end,:]))
            w_jpower = np.concatenate((w_jpower,jpower[start:end]))
    return w_toggles, w_jpower

def get_workloads_toggles_jpower(workloads, rtl='RocketConfig', 
                                 module='ChipTop', inst='ChipTop', num_toggles=N_CYCLES,
                                 power_level='rtl'):
    num_signals = get_num_idcodes(rtl,module=module)
    toggles = np.ndarray((0,num_signals))
    jpower = []
    start = 0
    workload_borders = {}
    for w in workloads:
        w_toggles = read_toggle_bin_file(w, rtl=rtl, module=module)
        w_jcycles, w_jpower = read_profile_file(w, rtl=rtl, module=module,inst=inst,num_toggles=num_toggles)
        if len(w_toggles) == 0 or len(w_jpower) == 0:
            continue
        if abs(len(w_jpower) - len(w_toggles)) > 5:
            print(f"{w}, {rtl}: Large mismatch btwn data size ({len(w_jpower)} jpower vs {len(w_toggles)} toggles)")
            continue
        size = min(len(w_jpower), len(w_toggles))
        w_jpower = w_jpower[:size]
        w_toggles = w_toggles[:size]
        if toggles.shape[1] != w_toggles.shape[1]: 
            print(f"{w}: Mismatch in number of signals ({toggles.shape[1]} vs {w_toggles.shape[1]})")
            continue
        toggles = numpy.concatenate((toggles,w_toggles))
            
        jpower += w_jpower
        workload_borders[w] = (start, start+size)
        start += size
    return toggles, numpy.array(jpower), workload_borders

def get_togglebits_ones_jpower(workloads, rtl='RocketConfig', power_level='rtl'):
    toggles = None
    ones = None
    jpower = []
    bmark_borders = [0]
    for w in workloads:
        w_toggles, w_ones = read_togglebits_ones_file(w)
        w_jcycles, w_jpower = read_profile_file(w)
        assert(abs(len(w_jpower) - len(w_toggles)) < 5), f"Large mismatch btwn data size ({len(w_jpower)} jpower vs {len(w_toggles)} toggles)"
        size = min(len(w_jpower), len(w_toggles))
        w_jpower = w_jpower[:size]
        w_toggles = w_toggles[:size]
        if toggles is None: 
            toggles = w_toggles; ones = w_ones
        else:
            toggles = numpy.concatenate((toggles,w_toggles))
            ones    = numpy.concatenate((ones,   w_ones))
            
        jpower += w_jpower
        bmark_borders.append(bmark_borders[-1]+size)
    return toggles, ones, numpy.array(jpower), numpy.array(bmark_borders)


def get_workload_from_idx(idx, workload_borders):
    for w in workload_borders:
        start,end = workload_borders[w]
        if start <= idx and idx < end: return w

def get_workloads_data(rtl, workloads, power_level='rtl'):
    deltas = []
    jpower = []
    bmark_borders = [0]
    for w in workloads:
        w_jcycles, w_jpower = read_profile_file(w, rtl, power_level)
        opm_file = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/opm/{w}.{power_level}.opm"
        if not os.path.exists(opm_file): 
            print(f"WARNING: file does not exist, {opm_file}")
            continue
        w_cycles, w_deltas = read_opm_file(opm_file)
        # if len(w_jpower) > 1000:
        #     print(f"WARNING: {w} workload is {len(w_deltas)} frames (> 1000 max set by Joules)")
        #     w_deltas = w_deltas[:len(w_jpower)]  # max frames in joules is 1000
        size = len(w_deltas)
        # if len(w_jcycles) != len(w_deltas):
        #     print(f"WARNING: Size of Joules data != size of counters ({len(w_jcycles)} != {len(w_deltas)})")
        #     print('\t', filename)
        #     print('\t', opm_file)
        w_jcycles = w_jcycles[:size]
        w_jpower = w_jpower[:size]
        size = len(w_jpower)
        w_deltas = w_deltas[:size]
        # for intech16 the first power value is HUGE so we just set it to the next power value
        # w_jpower[0] = w_jpower[1]
        deltas += w_deltas
        jpower += w_jpower
        bmark_borders.append(bmark_borders[-1]+size)
    return deltas, jpower, bmark_borders

def predict(_X, _y, regr_dict,
            # scaler_X, scaler_y,
            degree=1, print_errors=True, 
            verbose=False):
    
    regr = regr_dict['regr']
    scaler_X = regr_dict['scaler_X']
    scaler_y = regr_dict['scaler_y']

    X = scaler_X.transform(_X)
    y = scaler_y.transform(numpy.array(_y).reshape(-1,1)).reshape(-1)

    # # don't fit scaler to test data, that's cheating!
    # scaler_X = sklearn.preprocessing.StandardScaler()
    # scaler_y = sklearn.preprocessing.StandardScaler()
    # X = scaler_X.fit_transform(_X)
    # y = scaler_y.fit_transform(numpy.array(_y).reshape(-1,1)).reshape(-1)

    # create polynomial inputs 
    if degree > 1:
        polyf = preprocessing.PolynomialFeatures(degree=degree, include_bias=False)
        X = polyf.transform(X) #.reshape(-1, 1))

    # report score
    _r2 = regr.score(X, y)
    yp = regr.predict(X) 
    _yp = scaler_y.inverse_transform(yp.reshape(-1,1)).reshape(-1)
    _nrmse = get_nrmse(_y,_yp)*100

    if verbose: print("Predicting y based on X...")

    if verbose or print_errors:
        if print_errors == 'oneline':
            print('\t', round(_r2,2), '\t', round(_nrmse), end='')
        else:
            print(f"R^2 = {round(_r2,2)}, NRMSE = {round(_nrmse)}%")
    regr_dict['y_pred'] = _yp
    regr_dict['r2'] = _r2
    regr_dict['nrmse'] = _nrmse
    
    return regr_dict

@ignore_warnings(category=(ConvergenceWarning,UndefinedMetricWarning,UserWarning))
def fit(_X, _y,
        regr_dict={},
        scale=True,
        degree=1, 
        type='linear', alpha=1.0, l1_ratio=0.5, gamma=3,
        alphas=numpy.logspace(-1,2,num=10),
        l1_ratios=[.1, .5, .7, .9, .95, .99, 1], # recommended from ElasticNetCV docs
        max_iter=1000, print_errors=False,
        verbose=False):
    
    y_tmp = numpy.array(_y).reshape(-1,1)

    # default values
    scaler_X = sklearn.preprocessing.StandardScaler()
    scaler_y = sklearn.preprocessing.StandardScaler()
    X = _X
    y = _y

    if 'scaler_X' in regr_dict:
        scaler_X = regr_dict['regr_dict']
    elif scale:
        scaler_X.fit(_X)
        X = scaler_X.transform(_X)
    
    if 'scaler_y_train' in regr_dict:
        scaler_y = regr_dict['scaler_y_train']
    elif scale:
        scaler_y.fit(y_tmp)
        y = scaler_y.transform(y_tmp).reshape(-1)
    
    # create polynomial inputs 
    if degree > 1:
        polyf = preprocessing.PolynomialFeatures(degree=degree, include_bias=False)
        X = polyf.fit_transform(X) #.reshape(-1, 1))

    if verbose: print(f"Creating linear_model of type {type}...")
    
    # select regression model
    if type == 'linear':
        regr = sklearn.linear_model.LinearRegression()
    elif type == 'lasso':
        regr = sklearn.linear_model.Lasso(alpha=alpha)
    elif type == 'lassocv':
        regr = sklearn.linear_model.LassoCV(alphas=alphas,
                                            max_iter=max_iter, cv=5)
    elif type == 'ridge':
        regr = sklearn.linear_model.Ridge(alpha=alpha)
    elif type == 'ridgecv':
        regr = sklearn.linear_model.RidgeCV(alphas=alphas, cv=5)
    elif type == 'elasticnet':
        regr = sklearn.linear_model.ElasticNet(alpha=alpha, l1_ratio=l1_ratio)
    elif type == 'elasticnetcv':
        regr = sklearn.linear_model.ElasticNetCV(alphas=alphas,
                                                 l1_ratio=l1_ratios,
                                                 max_iter=max_iter, cv=5,
                                                 fit_intercept=True)
    elif type == 'mcp':
        regr = skglm.MCPRegression(alpha=alpha, # alpha=0.005 works well 
                                   gamma=gamma)
    elif type == 'gbr':
        regr = sklearn.ensemble.GradientBoostingRegressor(loss="squared_error", # alpha=0.005 works well 
                                    learning_rate=0.05, # 0.01
                                    n_estimators=200, # 500
                                    max_depth=2, # 4
                                    min_samples_leaf=9,
                                    min_samples_split=9)
    
    if verbose: print(f"Fitting model to data...")

    # fit model to data
    regr.fit(X, y)

    regr_dict = {}
    regr_dict['regr'] = regr
    regr_dict['scaler_X'] = scaler_X
    regr_dict['scaler_y'] = scaler_y

    _r2 = round(regr.score(X, y),2)
    regr_dict['r2_fit'] = _r2
    yp = regr.predict(X) 
    if scale: _yp = scaler_y.inverse_transform(yp.reshape(-1,1)).reshape(-1)
    else: _yp = yp
    _nrmse = get_nrmse(_y,_yp)*100
    regr_dict['y_pred'] = _yp
    regr_dict['nrmse_fit'] = _nrmse
    if verbose or print_errors: print(f"R^2 = {_r2}, NRMSE = {round(_nrmse)}%")
    elif print_errors=='oneline': print('\t', _r2, end='')

    num_coefs = len([c for c in regr.coef_ if c != 0]) if type != 'gbr' else X.shape[1]
    if verbose: print("Num coefs: ", num_coefs)
    
    return regr_dict

def apollo(X_train, X_test, y_train, y_test,
            regr_dict={},
            alpha=0.005, gamma=3.0,
            print_errors=False,
            verbose=False):
    # APOLLO
    regr_dict = fit(X_train, y_train,
                    regr_dict=regr_dict,
                    alpha=alpha, # alpha=0.005 gives best accuracy but ~50 signals
                    gamma=gamma,
                    type='mcp', verbose=verbose)
    regr_mcp = regr_dict['regr']
    coef_indexes = [i for i,c in enumerate(regr_mcp.coef_) if c != 0]
    X_train_mcp = X_train[:,coef_indexes]
    X_test_mcp = X_test[:,coef_indexes]
    regr_dict = fit(X_train_mcp, y_train,
                        type='ridge', verbose=verbose)
    regr_ridge = regr_dict['regr']
    regr_dict = predict(X_test_mcp, y_test, regr_dict, print_errors=print_errors,
                             verbose=verbose)
    regr_dict['coef_indexes'] = coef_indexes
    return regr_dict

def apollo_predict(X_test, y_test, regr_dict,
            alpha=0.001, gamma=3.0,
            print_errors=False,
            verbose=False):
    X_test_mcp = X_test[:,regr_dict['coef_indexes']]
    regr_dict = predict(X_test_mcp, y_test, regr_dict, print_errors=print_errors,
                             verbose=verbose)
    return regr_dict


def fit_and_predict(X_train, X_test, y_train, y_test,
                    degree=1, 
                    type='linear', alpha=1.0, l1_ratio=0.5,
                    alphas=numpy.logspace(-1,2,num=10),
                    l1_ratios=[.1, .5, .7, .9, .95, .99, 1], # recommended from ElasticNetCV docs
                    print_errors=False, verbose=False):
    
    regr_dict = fit(X_train, y_train,
                degree=degree, 
                type=type, alpha=alpha, l1_ratio=l1_ratio,
                alphas=alphas,
                l1_ratios=l1_ratios, # recommended from ElasticNetCV docs
                print_errors=print_errors,
                verbose=verbose)

    regr_dict = predict(X_test, y_test, regr_dict,
                degree=degree,
                print_errors=print_errors, verbose=verbose)
    
    return regr_dict
    
def get_rmse(y, yp):
    terms = [((i-j)**2) for i,j in zip(y,yp)]
    result = math.sqrt(sum(terms)/len(y))
    return result

def get_nrmse(y, yp):
    # divisor = max(y)-min(y)
    divisor = numpy.mean(y)
    return 1/divisor * get_rmse(y, yp)

def get_nmae(y,yp):
    terms = [ abs(i-j) for i,j in zip(y,yp) ]
    return sum(terms) / sum(y)

def get_r2(y,yp):
    y_avg = sum(y)/len(y)
    num = numpy.sum([ (yi-yip)**2 for yi,yip in zip(y,yp) ])
    denom = sum([ (yi-y_avg)**2 for yi in y ])
    score = 1 - num/denom
    return score

def print_errors(y,yp):
    return f"$R^2$ = {round(get_r2(y,yp),2)}, NRMSE = {round(get_nrmse(y,yp)*100)}%" #, NMAE = {round(nmae(y,yp)*100)}%"


def reorder_jpower(jpower, workload_borders, 
                   workloads_ordered=None, workload_vals=None):
    workloads_ordered = None
    if workloads_ordered is None:
        if workload_vals is None:
            workload_vals = get_workload_avgpow(workload_borders.keys())
        workloads_ordered = [w for p,w in sorted([(p,w) for w,p in workload_vals.items()])]
    new_data = np.ndarray((0,))
    new_workload_borders = {}
    idx = 0
    for w in workloads_ordered:
        start,end = workload_borders[w]
        new_data = np.concatenate((new_data, jpower[start:end]), axis=0)
        new_workload_borders[w] = (idx,new_data.shape[0])
        idx = new_data.shape[0]
    return new_data, new_workload_borders


def get_workload_avgpow(workloads):
    workload_avgpow_dict = {}
    for w in workloads:
        workload_avgpow_dict[w] = get_report_avgpow(w)
    return workload_avgpow_dict

def plot_minmax_lines(y,x=None, plt_min=True, plt_max=True,
                    c='r', linestyle='--',
                    label='', digits=1):
    if x is None:
        x = (0,len(y)-1)
    x1,x2 = x
    
    
    bbox_props = dict(boxstyle="square,pad=-0.05", fc="white", ec="white")
    if plt_min:
        plt.axhline(y=min(y), c=c,linestyle=linestyle)
        plt.text(x1, min(y), str(round(min(y),digits)), c=c, 
                fontsize=12, bbox=bbox_props,
                horizontalalignment='right')
    if plt_max:
        plt.axhline(y=max(y), c=c,linestyle=linestyle, label=label)
        plt.text(x2, max(y), str(round(max(y),digits)), c=c, 
                fontsize=12, bbox=bbox_props,
                horizontalalignment='left')
    plt.legend(framealpha=1, shadow=True, ncol=2, loc='upper right')
    

def plot_power_vals(workload_borders, workload_vals=None,
                    c='r', label='Average Power per Workload'):
    workloads = list(workload_borders.keys())
    if workload_vals is None:
        workload_vals = get_workload_avgpow(workloads)
    x = sorted([i for b in workload_borders.values() for i in b])
    y = [workload_vals[w] for w in workloads for _ in range(2)]
    plt.plot(x, y,c,linewidth=3, label=label)
    plt.axhline(y=min(y), c=c,linestyle='--')
    plt.axhline(y=max(y), c=c,linestyle='--')
    plt.text(0, min(y), str(round(min(y),1)), c=c, 
            fontsize=12,
            horizontalalignment='right')
    plt.text(x[-1], max(y), str(round(max(y),1)), c=c, 
            fontsize=12,
            horizontalalignment='left')
    plt.legend(framealpha=1, shadow=True, ncol=2, loc='upper right')
    return workload_vals

def plot_borders(workload_borders,
                 y=None,
                 ys=None,
                 rotation=30, color='g', alignment=('left','bottom')):
    if y is None and ys is None:
        ys = [0 for _ in range(len(workload_borders))]
    elif y is not None:
        ys = [y for _ in range(len(workload_borders))]
    
    if not workload_borders: return
    if type(workload_borders) == dict:
        workloads = workload_borders.keys()
        borders = list(workload_borders.values())
    else:
        workloads = ['' for _ in workload_borders]
        borders = workload_borders
    for w, b, y in zip(workloads, borders, ys):
        # TODO: make a 'name' field in workloads dict to use here
        plt.axvline(x=b[0], c='k', linewidth=0.8)
        w = w.replace('rv64ui-v-','')
        if w in riscv_torture:
            w = str(riscv_torture.index(w))
        if w in riscv_dv:
            w = str(riscv_dv.index(w))
        if w in gemmini_baremetal:
            w = get_gemmini_workload_description(w)
        
        bbox_props = dict(boxstyle="round,pad=-0.05,rounding_size=0.2", fc="white", ec="white")
        plt.text(b[0], y, w, rotation=rotation, c=color, 
                bbox=bbox_props,
                # backgroundcolor='w',
                horizontalalignment=alignment[0],
                verticalalignment=alignment[1])
    
    # final border
    plt.axvline(x=max([i for b in borders for i in b]), c='k', linestyle='-', linewidth=0.8)
    return

def plot_errors(y, yp, workload_borders=[], color='r'):
    hp_cutoff = np.quantile(y, 0.9)
    idxs = np.where(y > hp_cutoff)
    cycles = np.array(range(len(y)))
    cycles_hp = cycles[idxs]
    y_hp = y[idxs]
    yp_hp = yp[idxs]
    errors = [iyp-iy for iy,iyp in zip(y_hp, yp_hp)]
    plt.plot(cycles_hp, errors, color+'.');
    plt.axhline(y=0, c='k', label="y=0", linewidth=0.8)
    plt.xlabel("Count")
    plt.ylabel("Predicted - Actual Power (mw)");
    plot_borders(workload_borders,y=max(errors)*1.1)

def plot_power(y, yp=None, workload_borders={}, error=False, x=None, xp=None,
               legend=True, labels=None, marker='-',
               title=None, fig=None, ax=None, y_first=True, y_mult=1.3):
    '''
    workload_borders: { workload: (left, right), ... }
    '''
    if fig is None:
        fig = plt.figure(figsize=(13,4))
    if ax is None:
        ax = fig.add_subplot()
    
    # ymin = 0 if error else min(y)
    xmin = 0
    xmax = len(y)
    ymax = max(y)*y_mult
    
    if not y_first:
        if x is not None:
            ax.plot(x,y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
            xmin = min(x)
            xmax = max(x)
        else:
            ax.plot(y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
    
    if yp is not None:
        if xp is not None:
            ax.plot(xp, yp, marker, color='tab:orange', label=labels[1] if labels else "Model Power Prediction")
        else:
            ax.plot(yp, marker, color='tab:orange', label=labels[1] if labels else "Model Power Prediction")
    
        if error == 'abs':
            ax.plot([abs(i - j) for i,j in zip(y,yp)], 'r', label="Absolute Error")
            ax.plot([0,xmax], [0,0], 'k')
        elif error:
            ax.plot([(i - j) for i,j in zip(y,yp)], 'r', label="Error")
            ax.plot([0,xmax], [0,0], 'k')
        
        t = title if title else "RTL Signal Toggles Power Prediction"
        ax.set_title(f"{t}, {print_errors(y,yp)}")
    else:
        t = title if title else "Power Profile"
        ax.set_title(t)
    
    if y_first:
        if x is not None:
            ax.plot(x,y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
            xmin = min(x)
            xmax = max(x)
        else:
            ax.plot(y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
    
    plot_borders(workload_borders, ys=[max(y[s:e]) for s,e in workload_borders.values()])

    ax.set_xlim(xmin,xmax)
    ax.set_ylim(min(y)-10,ymax)

    
    
    
    plt.xlabel(f'Windows of N Cycles (N={N_CYCLES})') # only show for last one
    ax.set_ylabel(f'Power (mW)')
    
    if legend:
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles, labels, 
                   loc='upper right', bbox_to_anchor=(1,1), ncol=2, 
                   frameon=True, framealpha=1)
        

def get_uniform_indexes(length, train_size=None):
    if train_size is None: train_size = 0.05
    if type(train_size) is float: train_size = int(train_size*length)
    return np.linspace(0, length-1, num=train_size, dtype=int)

def get_train_test_split(_X, _y,
                         train_workloads=None, test_workloads=None, workload_borders=None,
                         indexes=None, train_size=None,
                         normalize=False):
    if train_workloads is not None:
        test_workloads = [w for w in workload_borders if w not in train_workloads]
    elif test_workloads is not None:
        train_workloads = [w for w in workload_borders if w not in test_workloads]
    
    if (train_workloads is not None) and (test_workloads is not None):
        assert(workload_borders is not None), "Key word arg missing: workload_borders"
        X_train, y_train = get_workloads_subset_toggles_jpower(train_workloads, _X, _y, workload_borders)
        X_test, y_test = get_workloads_subset_toggles_jpower(test_workloads, _X, _y, workload_borders)
    else:
        if indexes is None:
            indexes = get_uniform_indexes(len(_y), train_size)
        if normalize:
            scaler_X = sklearn.preprocessing.StandardScaler()
            scaler_y = sklearn.preprocessing.StandardScaler()
            X = scaler_X.fit_transform(_X)
            y = scaler_y.fit_transform(numpy.array(_y).reshape(-1,1)).reshape(-1)
        else:
            X = _X
            y = _y
        mask = np.ones(X.shape[0], dtype=bool)
        mask[indexes] = False
        X_train = X[indexes]
        y_train = y[indexes]
        X_test = X[mask]
        y_test = y[mask]
    return X_train, X_test, y_train, y_test

def get_train_test_workload_borders(workload_borders, indexes=None,
                                    train_size=None,):
    if indexes is None:
        end = max([i for se in workload_borders.values() for i in se])
        indexes = get_uniform_indexes(end, train_size)
    workload_borders_train = {}
    workload_borders_test = {}
    start=0
    for w in workload_borders:
        w_start,w_end = workload_borders[w]
        end = sum([1 if i < w_end else 0 for i in indexes ])
        workload_borders_train[w] = (start,end)
        start = end
    max_border = max([i for t in workload_borders.values() for i in t])
    index_test = [i for i in range(max_border) if i not in indexes]
    start=0
    for w in workload_borders:
        w_start,w_end = workload_borders[w]
        end = sum([1 if i < w_end else 0 for i in index_test ])
        workload_borders_test[w] = (start,end)
        start = end
    return workload_borders_train, workload_borders_test

def manhattan_dist(a,b):
    dist = sum([abs(ia-ib) for ia,ib in zip(a,b)])
    return dist

def euclidean_dist(a,b):
    dist = math.sqrt(sum([(ia-ib)**2 for ia,ib in zip(a,b)]))
    return dist


def get_cluster_center_indexes(kmeans, scaled_features):
    cluster_dists = [[] for _ in range(kmeans.n_clusters)]  # [(dist,data_idx) for k in clusters for data in data_in_k]
    for i,k in enumerate(kmeans.labels_):
        i_data = scaled_features[i]
        i_center = kmeans.cluster_centers_[k]
        # dist = math.sqrt(sum([(x-c)**2 for x,c in zip(i_data,i_center)]))
        dist = manhattan_dist(i_data,i_center)
        cluster_dists[k].append((dist,i))
    cluster_dists = [sorted(l) for l in cluster_dists if len(l) > 0]
    return [l[0][1] for l in cluster_dists]

def get_cluster_points(kmeans, scaled_features, X, y):
    idxs = get_cluster_center_indexes(kmeans, scaled_features)
    X_t = X[idxs]
    y_t = y[idxs]
    return np.array(X_t), np.array(y_t)

def get_cluster_train_test_split(kmeans, scaled_features, X, y):
    idxs = get_cluster_center_indexes(kmeans, scaled_features)
    idxs_test = [i for i in range(len(X)) if i not in idxs]
    X_t = X[idxs]
    y_t = y[idxs]
    X_test = X[idxs_test]
    y_test = y[idxs_test]
    return X_t, y_t, X_test, y_test

def variance(data,kmeans):
    cluster_centers = kmeans.cluster_centers_
    x_u = [] # list[(data,label)]
    for i,data_label in enumerate(zip(data, kmeans.labels_)):
        x_u.append(data_label)
    n = len(data)
    k = len(cluster_centers)
    diff_2 = [(xi - ui)**2 for data,label in x_u for xi,ui in zip(data,cluster_centers[label])]
    v = 1/(n-k) * sum(diff_2)
    return v

def log_likelihood(data,kmeans):
    n = len(data)
    d = len(data[0])
    k = kmeans.n_clusters
    cluster_sizes = numpy.bincount(kmeans.labels_)
    t1 = -(n/2)*math.log(2*math.pi)
    t2 = -(n*d/2)*math.log(variance(data,kmeans))
    t3 = -(n-k)/2
    t4 = sum([ni*math.log(ni/n) for ni in cluster_sizes])
    ln_lj = t1 + t2 + t3 + t4
    return ln_lj

def bic(data, kmeans):
    n = len(data)
    pj = len(data[0])  # linear regression
    b = pj * math.log(n) - 2*log_likelihood(data,kmeans)
    return b

def sum_of_squared_distances(data, kmeans):
    cluster_centers = kmeans.cluster_centers_
    data_labels = [] # list[(data,label)]
    for data_label in zip(data, kmeans.labels_):
        data_labels.append(data_label)
    n = len(data)
    k = len(cluster_centers)
    diff_2 = [(xi - ui)**2 for data,label in data_labels for xi,ui in zip(data,cluster_centers[label])]
    return s

# def get_profile_dir(w, rtl='RocketConfig', power_level='rtl'):
#     profile_dir = f"{build_dir}/chipyard.harness.TestHarness.{rtl}-ChipTop/power-{power_level}-rundir/plot_profile"
#     return profile_dir

def get_module_profile_dir(w, rtl='RocketConfig', power_level='rtl'):
    profile_dir = f"{build_dir}/chipyard.harness.TestHarness.{rtl}-ChipTop/power-{power_level}-rundir/plot_profile/modules-{w}"
    os.makedirs(profile_dir,exist_ok=True)
    return profile_dir

def get_power_filepath(workload="", rtl='RocketConfig', 
                         output_format='plot_profile',
                         filename=None,
                         power_level='rtl', 
                         module='ChipTop',
                         inst='ChipTop',
                         num_toggles=N_CYCLES,
                         levels=None):
    assert(output_format in {'plot_profile','report'}), "output_format must be one of {'plot_profile','report'}"
    basename = f"{build_dir}/chipyard.harness.TestHarness.{rtl}-ChipTop/power-{power_level}-rundir/{output_format}"
    if filename is None:
        # filename = get_workload_filename(workload)
        filename = get_run_name(workload,rtl,module=module,inst=inst,num_toggles=num_toggles)
    if output_format == 'plot_profile': ext = 'png.data'
    elif output_format == 'report': ext = 'rpt'
    return f"{basename}/{filename}.{ext}"


def get_run_name(workload, rtl='RocketConfig',
                 module='ChipTop',
                 inst='ChipTop',
                 num_toggles=N_CYCLES):
    ''' Names for Joules output reports
        Applies to both plot_profile/ and report/ outputs
    '''
    name = workload
    if module != 'ChipTop':
        # name += f"-module_{module}"
        inst = get_module_insts_dict(rtl)[module][0]
    if inst != 'ChipTop':
        inst = inst.replace('/','-')
        name += f".inst-{inst}"
    if num_toggles != N_CYCLES:
        name += f".toggles-{num_toggles}"
    return name

def get_fsdb_idcodes(w, rtl='RocketConfig'):
    with open(get_outfile_path('idcodes',w, rtl), 'r') as f:
        line = f.readlines()[0]
        words = line.split()
        idcodes = [int(w) for w in words[1:]]
        assert(int(words[0]) == len(idcodes))
    return idcodes

def get_num_idcodes(rtl='RocketConfig',module='ChipTop'):
    with open(get_outfile_path('idcodes','all', rtl=rtl,module=module), 'r') as f:
        return int(f.readlines()[0].strip())

def get_idcodes_proxies(rtl='RocketConfig', indexes=None, module='ChipTop'):
    with open(get_outfile_path('idcodes','all', rtl, module=module), 'r') as f:
        lines = f.readlines()[1:]
    proxy_lines = lines if indexes is None else [lines[i] for i in indexes]
    proxies = [l.split() for l in proxy_lines]
    proxies = [(int(i),p) for i,p in proxies]
    return proxies

def get_proxy_signal_names(rtl='RocketConfig', indexes=None, module='ChipTop'):
    with open(get_outfile_path('idcodes','all', rtl, module=module), 'r') as f:
        lines = f.readlines()[1:]
    proxy_lines = lines if indexes is None else [lines[i] for i in indexes]
    proxies = [l.split()[1] for l in proxy_lines]
    return proxies


def dump_proxy_signals(indexes, workload="", rtl='RocketConfig', module='ChipTop', 
                    filename=None, top_inst=None):
    if filename is None:
        w = '' if workload == '' else '-'+workload
        m = '' if module == 'ChipTop' else f"-module_{module}"
        filename = f"{rtl}{w}{m}"
        filename = get_outfile_path('proxy_signals',workload,rtl,module=m)
    proxies = sorted(get_proxy_signal_names(rtl=rtl,indexes=indexes,module=module))
    if not filename.startswith('/'):
        filename = f"{scratch_dir}/out/proxy_signals/{filename}.txt"
    with open(filename,'w') as f:
        for p in proxies:
            if top_inst is not None:
                p = get_inst_rel_path(p, top_inst)
            f.write(p+'\n')
    print(f"Wrote signals to: {filename}")


def get_modules_data(path=None):
    module_toggles_dict = {}
    module_jpower_dict = {}
    with open(get_module_data_path(type='summary'),'r') as f:
        summary_dict = json.load(f)
    for m in summary_dict['modules']:
        module_toggles_dict[m] = np.load(get_module_data_path(m,'toggles'))
        module_jpower_dict[m]  = np.load(get_module_data_path(m,'jpower'))
    return module_toggles_dict, module_jpower_dict, summary_dict['workloads']

def get_module_tree(rtl='RocketConfig', 
                    module='ChipTop'):
    '''
    module - root module of tree
    '''

    g = nx.DiGraph()

    tmh = {}
    tmh_file = get_tmh_path(rtl)
    with open(tmh_file, 'r') as f:
        tmh = json.load(f)

    start = tmh['instance_name']
    # only keep DigitalTop (throw out generic IO cells)
    tmh['instances'] = [i for i in tmh['instances'] if i['module_name'] == 'DigitalTop']
    parents_dicts = [(start, tmh)]

    root_path = None

    while parents_dicts:
        parent, parent_dict = parents_dicts.pop()
        parent_module = parent_dict['module_name']
        children = parent_dict['instances']
        if parent_module == module:
            root_path = parent
            g.add_node(parent, module_name=parent_module)

        for child_dict in children:
            child = child_dict['instance_name']
            child_path = parent+'/'+child

            if root_path is not None and \
                child_path.startswith(root_path):
                g.add_node(child_path, module_name=child_dict['module_name'])
                g.add_edge(parent, child_path)

            parents_dicts.append((child_path, child_dict))
    assert(nx.is_tree(g)), "Graph is not a tree"
    return g

gemmini_default_fields = {
    'activation': 0,
    'no_bias': 0,
    'repeating_bias': 1,
    'a_transpose': 0, 'b_transpose': 0,
    'mat_dim_i': 64, # 128
    'mat_dim_k': 128, # 256
    'mat_dim_j': 128, # 256
    'a_data': 'r', 'b_data': 'r',
    'idx': 0, 'extra': ''
}

def get_gemmini_workload_description(workload, full=False):
    words = workload.split('-')[1:-1] # remove tmw-*-baremetal
    words = [int(w.replace('n','-')) if w.replace('n','').isnumeric() else w for w in words]
    workload_fields = {k: words[i] for i,k in enumerate(gemmini_default_fields.keys()) if i < len(words)}
    workload_fields['extra'] = '-'.join(words[11:])

    keys_trans = ['a_transpose','b_transpose']
    keys_dims = ['mat_dim_i','mat_dim_k','mat_dim_j']
    keys_data = ['a_data','b_data']

    A_TRANSPOSE = workload_fields['a_transpose']
    B_TRANSPOSE = workload_fields['b_transpose']
    MAT_DIM_I = workload_fields['mat_dim_i']
    MAT_DIM_K = workload_fields['mat_dim_k']
    MAT_DIM_J = workload_fields['mat_dim_j']


    trans = []
    if A_TRANSPOSE: trans.append("$A^T$")
    if B_TRANSPOSE: trans.append("$B^T$")
    trans = ','.join(trans)

    if MAT_DIM_K == MAT_DIM_J: dim=f"A: ({MAT_DIM_I}x{MAT_DIM_K})"
    elif MAT_DIM_I == MAT_DIM_K: dim=f"B: ({MAT_DIM_K}x{MAT_DIM_J})"
    else: dim=f"dim: ({', '.join([str(workload_fields[k]) for k in keys_dims])})"

    if workload_fields['a_data'] == workload_fields['b_data']: data=f"data: {str(workload_fields['a_data']).replace('r','rand')}"
    else: data=f"data: {', '.join([str(workload_fields[k]).replace('r','rand') for k in keys_data])}"

    name=[]
    if any([gemmini_default_fields[k] != workload_fields[k] for k in keys_dims]): name.append(dim)
    if any([gemmini_default_fields[k] != workload_fields[k] for k in keys_data]): name.append(data)
    if any([gemmini_default_fields[k] != workload_fields[k] for k in keys_trans]): name.append(trans)
    name = ','.join(name)
    if len(name) == 0: name=f"default #{workload_fields['idx']}"

    
    if full: name=f"({MAT_DIM_I}x{MAT_DIM_K})({MAT_DIM_K}x{MAT_DIM_J}), {data}"
        
    return name


def get_inst_module_dict(rtl='RocketConfig'):
    inst_module_dict = {}

    tmh = {}
    with open(get_tmh_path(rtl), 'r') as f:
        tmh = json.load(f)

    root = tmh['instance_name']
    parent_children = [(root, tmh['instances'])]
    inst_module_dict[tmh['instance_name']] = tmh['module_name']

    while parent_children:
        parent, children = parent_children.pop()
        for child_dict in children:
            child = child_dict['instance_name']
            child_path = parent+'/'+child
            inst_module_dict[child_path] = child_dict['module_name']
            parent_children.append((child_path, child_dict['instances']))
    return inst_module_dict

def get_module_insts_dict(rtl='RocketConfig',root_name='chiptop'):
    module_insts_dict = {}
    inst_module_dict = get_inst_module_dict(rtl)
    for inst,module in inst_module_dict.items():
        if module not in module_insts_dict: module_insts_dict[module] = []
        module_insts_dict[module].append(inst)
    module_insts_dict['ChipTop'] = [root_name]
    return module_insts_dict


def get_module_insts_sigids_dict(workload, rtl='RocketConfig'):
    inst_module_dict = get_inst_module_dict(rtl)
    modules = set(inst_module_dict.values())

    inst_hier = []
    module = ''
    w_fsdb_idcodes = set(get_fsdb_idcodes(workload, rtl))
    module_insts_sigids_dict = {m: {} for m in modules}
    with open(get_outfile_path('headers',workload,rtl), 'r') as f:
        in_module = False
        for line in f.readlines():
            words = line.split()
            if len(words) < 1: continue
            if words[0] == '<Scope>':
                name=words[1].replace('name:','')
                inst_hier.append(name)
                inst = '/'.join(inst_hier).replace('TestDriver/testHarness/chiptop', 'ChipTop')
                module = inst_module_dict[inst] if inst in inst_module_dict else "NONE"
                in_module = module in modules
                if in_module:
                    module_insts_sigids_dict[module][inst] = set()
            elif words[0] == '<Upscope>':
                inst = '/'.join(inst_hier).replace('TestDriver/testHarness/chiptop', 'ChipTop')
                module = inst_module_dict[inst] if inst in inst_module_dict else "NONE"
                in_module = False
                inst_hier.pop(-1)
            elif words[0] == '<Var>':
                if len(words) < 3: continue                
                if in_module:
                    signal=words[1]
                    idcode=int(words[2])
                    if idcode not in w_fsdb_idcodes: 
                        continue
                    module_insts_sigids_dict[module][inst].add((signal, idcode))
    return module_insts_sigids_dict

def get_module_sigids_dict(workload, rtl='RocketConfig'):
    module_insts_sigids_dict = get_module_insts_sigids_dict(workload,rtl)
    module_signal_idcodes_dict = {}
    for module in module_insts_sigids_dict:
        ''' get signals+idcodes for first instance of module
            since we need columns to correspond to the same signal
            across different time slices
        '''
        module_insts = list(module_insts_sigids_dict[module].keys())
        if len(module_insts) < 1: continue
        inst0 = module_insts[0]
        signal_idcode_list = module_insts_sigids_dict[module][inst0]
        # signals0 = sorted([s for s,_ in signal_idcode_list])
        signal_idcode_dict = {s:[id] for s,id in signal_idcode_list}
        for inst in module_insts[1:]:
            signal_idcode_list = module_insts_sigids_dict[module][inst]
            for s,id in signal_idcode_list:
                if s in signal_idcode_dict: signal_idcode_dict[s].append(id)
                else: print(module, inst, s)
        signal_idcode_dict = {s:ids for s,ids in signal_idcode_dict.items() if len(ids) == len(module_insts_sigids_dict[module])}
        module_signal_idcodes_dict[module] = signal_idcode_dict.copy()
    return module_signal_idcodes_dict


def print_joules_commands(my_workloads,rtl='RocketConfig',
                          module='ChipTop',
                          modules=None,
                          inst=None,
                          insts=None,
                          num_toggles=N_CYCLES,
                          w_data_dict=None,
                          output_formats=['plot_profile'],
                          overwrite=True):

    yaml_config_file = get_outfile_path('hammer_joules_yaml_config',rtl=rtl)

    if modules is None: modules = [module]
    if insts is None:
        if inst is not None: insts = [inst]
        else: insts = [get_module_insts_dict(rtl)[module][0] for m in modules]
    insts = [i.replace('chiptop','ChipTop') for i in insts]

    if w_data_dict is None: w_data_dict = {w: {} for w in my_workloads}

    cfg = {
            'power.inputs.saifs': [],
            'power.inputs.waveforms': [],
            'power.inputs.report_configs': [],
            'power.joules.version': '211',
            'power.voltus.version': '211_ISR3',
            'vlsi.core.power_tool': 'joules'
        }

    for w in my_workloads:
        start_time = w_data_dict[w]['start_time'] if 'start_time' in w_data_dict[w] else 100
        for inst in insts:
            output_format_torun = output_formats.copy()
            for output_format in output_formats:
                fpath = get_power_filepath(w,rtl=rtl,inst=inst,num_toggles=num_toggles,output_format=output_format)
                if os.path.exists(fpath) and not overwrite:
                    output_format_torun.remove(output_format)
            if len(output_format_torun) == 0: continue
            report_cfg = {
                'waveform_path': get_waveform_path(w, rtl=rtl),
                'inst': inst,
                'toggle_signal': f'/ChipTop/{clock}',
                'start_time': f"{start_time}ns", # add 100ns since this is when we usually start
                'num_toggles': num_toggles,#N_CYCLES,
                'report_name': get_run_name(w,rtl,inst=inst,num_toggles=num_toggles),
                'output_formats': output_format_torun
            }
            cfg['power.inputs.report_configs'].append(report_cfg.copy())
            
        
    with open(yaml_config_file,'w') as f:
        yaml.dump(cfg, f, sort_keys=False)
    

    make_target = 'redo-power-rtl' if os.path.exists(f"{build_dir}/chipyard.harness.TestHarness.{rtl}-ChipTop/power-rtl-rundir/pre_report_power") else 'power-rtl'

    hammer_extra_args = f"""export HAMMER_EXTRA_ARGS="-p {vlsi_dir}/nk.yml -p {yaml_config_file}" && export BINARY={workloads[my_workloads[0]]['binary_path']}"""
    hammer_extra_args += f""" && export CONFIG={rtl} """
    cmd = f"echo {rtl} && make {make_target}"
    cmd= f"""bsubq "{cmd}" """
    to_print = dedent(f"""
    {hammer_extra_args}
    {cmd}
    """).strip()

    if make_target == "redo-power-rtl" and len(cfg['power.inputs.report_configs']) == 0: return
    print(to_print)

    return

def get_highest_power_insts(workload, 
                              rtl='RocketConfig', 
                              module='ChipTop',
                              num_insts=None,
                              power_cutoff=None,
                              subtract_children=True):
    g = get_module_tree(rtl=rtl,module=module)
    # get power from avg power report
    instpath_power_dict = {}
    report_path = get_power_filepath(workload,rtl,output_format='report')
    if not os.path.exists(report_path):
        print(f"Warning: file does not exists, {report_path}")
        return []
    with open(report_path,'r') as f:
        for line in f.readlines():
            words = line.split()
            if len(words) != 8: continue
            isnum = ''.join([c for c in words[5] if c not in '.e-+'])
            if not isnum.isnumeric(): continue
            power = float(words[5])
            # if power < .01: continue
            instpath = words[7][1:] # remove first '/'
            instpath_power_dict[instpath] = power
    # add power to graph
    for n in g.nodes:
        g.nodes[n]['power'] = 0
    # find power that is in parent but not in children
    for instpath,p in instpath_power_dict.items():
        if instpath in g.nodes:
            g.nodes[instpath]['power'] = p

    if subtract_children:
        # for each instance, subtract out the children's powers
        for src,dst in nx.bfs_edges(g, get_module_insts_dict(rtl)[module][0]):
            g.nodes[src]['power'] -= g.nodes[dst]['power']
            assert(round(g.nodes[src]['power']) >= 0)

    nodes_power = g.nodes(data='power')
    power_nodes = sorted([(p,n) for n,p in nodes_power if round(p,2) > 0], reverse=True)
    power_nodes = [(n,p) for p,n in power_nodes]

    if num_insts is not None:
        return power_nodes[:num_insts]
    elif power_cutoff is not None:
        return [i for i in power_nodes if i[1] > power_cutoff]
    else: return power_nodes

    # if power_cutoff is not None:
    #     return [n for p,n in power_nodes if p > power_cutoff]
    # elif num_insts is not None:
    #     return [n for p,n in power_nodes][:num_insts]
    # else: return [n for p,n in power_nodes]


def print_fsdb_commands(my_workloads,rtl='RocketConfig',
                      module='ChipTop',
                      start_times=None,
                      w_data_dict=None,
                      overwrite=True):
    
    if not overwrite:
        my_workloads = [w for w in my_workloads if not os.path.exists(get_outfile_path('toggles',w,rtl,module=module))]

    module_str = f"--module {module}" if module != 'ChipTop' else ""

    if (start_times is None) and (w_data_dict is not None):
        start_times = [str(int(w_data_dict[w]['start_time']/CLOCK_PERIOD)) for w in my_workloads]

    if start_times is not None: assert(len(start_times) == len(my_workloads)), f"Number of start times should match number of workloads ({len(start_times)} vs {len(my_workloads)})"
    
    start_times_str = f"""--start_times "{' '.join(start_times)}" """ if start_times is not None else ""

    if len(my_workloads) == 0: return
    
    print(dedent(f"""
    e fsdb
    ./scripts/print_headers.py --rtl {rtl} --workloads "{' '.join(my_workloads)}"
    ./scripts/parse_headers.py --rtl {rtl} {module_str} --workloads "{' '.join(my_workloads)}"
    ./scripts/dump_toggles.py --rtl {rtl} {module_str} --workloads "{' '.join(my_workloads)}" {start_times_str}
    """))
    return

def print_sim_out_commands(my_workloads,rtl='RocketConfig',overwrite=True):
    if not overwrite:
        my_workloads = [w for w in my_workloads if not os.path.exists(get_outfile_path('sim_out',w,rtl))]
    if overwrite or (my_workloads and not os.path.exists(f"{vlsi_dir}/build/chipyard.harness.TestHarness.{rtl}-ChipTop/sim-rtl-rundir/simv")):
        # if 
        print(f"""make redo-sim-rtl CONFIG={rtl} HAMMER_EXTRA_ARGS="--stop_before_step run_simulation" BINARY=placeholder""")
    for w in my_workloads:
        cmd = f"""bsubq make sim-out CONFIG={rtl} BINARY={workloads[w]['binary_path']}"""
        print(cmd)
    return

def print_sim_opm_commands(my_workloads,rtl='RocketConfig',overwrite=True):
    if not overwrite:
        my_workloads = [w for w in my_workloads if not os.path.exists(get_outfile_path('sim_opm',w,rtl))]
    if overwrite or (my_workloads and not os.path.exists(f"{vlsi_dir}/build/chipyard.harness.TestHarness.{rtl}-ChipTop/sim-rtl-rundir/simv-opm")):
        print(f"""make gen-sim-opm CONFIG={rtl} BINARY=placeholder""") # use any workload for BINARY so we don't error
    for w in my_workloads:
        cmd = f"""bsubq make sim-opm CONFIG={rtl} BINARY={workloads[w]['binary_path']}"""
        print(cmd)
    return

def print_sim_commands(my_workloads,rtl='RocketConfig', overwrite=True, overwrite_simv=False, parallel=True, bsub='bsubq'):
    if not overwrite:
        my_workloads = [w for w in my_workloads if not os.path.exists(get_waveform_path(w,rtl))]
    for i,w in enumerate(my_workloads):
        target = 'redo-sim-rtl-debug' if not overwrite_simv and os.path.exists(f"{vlsi_dir}/build{i}/chipyard.harness.TestHarness.{rtl}-ChipTop/sim-rtl-rundir/simv-debug") else 'sim-rtl-debug'
        build_dir = f"build{i}" if parallel else "build"
        cmd = f"""{bsub} make {target} CONFIG={rtl} BINARY={workloads[w]['binary_path']} VLSI_OBJ_DIR={build_dir}"""
        print(cmd.strip())
    return

def get_workload_starttimes(my_workloads,rtl='RocketConfig',module='Rocket',d=None):
    w_start_dict = {}
    for w in my_workloads:
        w_start_dict[w] = 0
        outfile_path = get_outfile_path('sim_out',w,rtl)
        assert(os.path.exists(outfile_path)), f"File does not exist: {outfile_path}"
        sim_cycles = None
        cntr_cycles = None
        with open(outfile_path,'r') as f:
            cycle_count = 0
            rocket_start = False
            for line in f.readlines():
                if not rocket_start: rocket_start = line.startswith('C0:')
                words = line.split()
                if len(words) > 0 and words[0] == 'END':
                    cntr_cycles = int(words[1])
                    continue
                elif len(words) > 1 and line.strip().startswith('*** PASSED ***'):
                    sim_cycles = int(words[5])
                    continue
                if not line.startswith('EVENT'): continue
                if not words[1].isnumeric(): continue
                cycle_count = int(words[1])
                if len(words) > 2 and ((module == 'Gemmini') and (words[2] == 'Gemmini') and ('start' in words)) or \
                    ((module == 'Rocket') and (words[2] == 'Rocket') and rocket_start):
                    cntr_start_cycle = int((cycle_count))
        if (sim_cycles is None) or (cntr_cycles is None):
            print(f"ERROR: {outfile_path}")
            continue
        cntr_to_sim = sim_cycles - cntr_cycles
        w_start_dict[w] = cntr_start_cycle + cntr_to_sim

    if d is not None:
        for w in w_start_dict:
            if not w in d: d[w] = {}
            d[w]['start_time'] = w_start_dict[w] * CLOCK_PERIOD
            d[w]['start_cycles'] = w_start_dict[w]
    
    return [w_start_dict[w] for w in my_workloads]

def get_inst_rel_path(inst_path,parent_inst,incl_parent=False):
    words = inst_path.split('/')
    adj = 0 if incl_parent else 1
    return '/'.join(words[words.index(parent_inst)+adj:])


def plot_power_adjustments():
    plt.xlabel(f'Windows of N Cycles (N={N_CYCLES})')
    plt.ylabel('Power (mW)')

def plot_gemmini_adjustments():
    # plt.title(f"Power of Gemmini Running Different Workloads")
    plt.xlabel('Tiled matrix multiplication variations, sorted by decreasing max power \ndefault = (64,128)x(128x128), data: random')
    plt.ylabel('Power (mW)')

def get_rtl_name(rtl):
    if rtl.endswith('Config'):
        rtl = rtl[:-6]
    rtl = rtl.replace('Boom','BOOM')
    return rtl


def infer_to_one_workload(toggles, jpower, workload_borders, w_data_dict=None):
    if w_data_dict is None:
        w_data_dict = {w: {} for w in workload_borders}
    for w in workload_borders:
        s,e = workload_borders[w]
        X_train = np.concatenate((toggles[:s,:],toggles[e:,:]),axis=0)
        X_test = toggles[s:e,:]
        y_train = np.concatenate((jpower[:s],jpower[e:]),axis=0)
        y_test = jpower[s:e]
        w_data_dict[w]['apollo_regr_dict'] = apollo(X_train, X_test, y_train, y_test,verbose=False)
    return w_data_dict




def get_workloads_proxytoggles_jpower(workloads, rtl='RocketConfig', 
                                 module='ChipTop', inst='ChipTop', num_toggles=N_CYCLES,
                                 power_level='rtl'):
    w_data_dict = {w: {} for w in workloads}
    for w in workloads:
        opm_fpath = get_outfile_path('sim_opm',w,rtl)
        with open(opm_fpath) as f:
            lines = f.readlines()
            lines = [l for l in lines if len(l) > 0]
            last_idx = -2
            last = lines[last_idx]
            words = last.split()
            assert(words[-3].isnumeric()), f"Last line doesn't match expected format: {last}"
            simcycles_end = int(words[-3])
            last = lines[-1]
            words = last.split()
            assert(words[1].isnumeric()), f"Last line doesn't match expected format: {last}"
            cntr_end = int(words[1])
            cntr_to_simcycles = simcycles_end - cntr_end
            # start_idx = 1 + min([i for i,line in enumerate(lines) if line.startswith('DRAMSim2 Clock Frequency')])
            # cntr_start = int(lines[start_idx].split()[0])
            # TODO: use get_workload_starttimes function
            for i,line in enumerate(lines):
                if "Gemmini start" in line: 
                    start_idx = i
                    break
            # print(start_idx)
            cntr_start = int(lines[start_idx].split()[1])
            simcycles_start = cntr_start + cntr_to_simcycles
            num_windows = int((cntr_end - cntr_start)/N_CYCLES) + 1

            for p in proxies_dict:
                proxies_dict[p][w] = [0 for _ in range(num_windows)]

            w_ptoggles = numpy.zeros((num_windows,len(proxies)))

            for line in lines[start_idx:last_idx]:
                words = line.split()
                if (len(words) == 2) and words[0].isnumeric() and words[1].isnumeric():
                    cycle = int(words[0])
                    window = math.floor((cycle - cntr_start)/N_CYCLES)
                    index = int(words[1])
                    w_ptoggles[window][index] += 1

            w_data_dict[w]['ptoggles'] = w_ptoggles.copy()
            w_data_dict[w]['simcycle_start_end'] = (simcycles_start,simcycles_end)

    for w in workloads:
        w_jcycles, w_jpower = read_profile_file(w,rtl,module)

        start,end = w_data_dict[w]['simcycle_start_end']
        s,e = (w_jcycles[0],w_jcycles[-1])
        target_length = len(w_data_dict[w]['ptoggles'])

        # cut jpower
        w_jpower = w_jpower[-target_length:]

        w_data_dict[w]['jpower'] = w_jpower.copy()

    workloads = [w for _,w in sorted([(max(w_data_dict[w]['jpower']),w) for w in workloads], reverse=True)]

    jpower = np.array([p for w in workloads for p in w_data_dict[w]['jpower']])
    toggles_proxies = np.ndarray((0,len(proxies)))
    workload_borders = {}
    for w in workloads:
        start = toggles_proxies.shape[0]
        toggles_proxies = np.concatenate((toggles_proxies,w_data_dict[w]['ptoggles']),axis=0)
        end = toggles_proxies.shape[0]
        workload_borders[w] = (start,end)


    return toggles_proxies, numpy.array(jpower), workload_borders
