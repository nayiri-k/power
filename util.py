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
from sklearn.exceptions import ConvergenceWarning
import json


exec(open("/tools/C/nayiri/power/power-analysis/variables.py").read())

sys.path.append("/tools/C/nayiri/power/simmani")
import simmani
import simmani.model.clustering

def sec_to_cycle(time):
    return time * 1e9  # since sim timescale is 1ns

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


def read_profile_file(workload, rtl='RocketConfig', power_level='rtl', profile_file=None):
    if profile_file is None:
        profile_file = get_profile_filepath(workload)
    if not os.path.exists(profile_file): 
        print(f"WARNING: file does not exist, {profile_file}")
        return [], []
    with open(profile_file,'r') as f:
        lines = f.readlines()
    if len(lines) == 0:  # file becomes empty when this workload is being run
        print(f"WARNING: file {profile_file} is empty")
        return [], []
    first_line = lines.pop(0)
    time_unit, _ = get_time_power_units(first_line)

    time_power = [l.split() for l in lines]
    time = list(map(lambda x: convert_to_sec(float(x[0]), time_unit), time_power))
    cycles = list(map(lambda x: sec_to_cycle(x), time))
    # power = list(map(lambda x: sum(list(map(float, x[1:]))), time_power))
    # power = [sum([float(p) for p in l[1:]]) for l in time_power]
    time_power = [tp for tp in time_power if len(tp) == 2]
    power = list(map(lambda x: float(x[1]), time_power))
    return cycles, power

# DEPRECATED
def read_profile_file_total(workload, rtl='RocketConfig', power_level='rtl'):
    read_profile_file(workload, rtl=rtl, power_levels=power_level)

def read_togglebits_ones_file(workload):
    base_dir = "/tools/C/nayiri/power/power-analysis/fsdb/out"
    toggles = numpy.fromfile(f"{base_dir}/dump-toggle_bits/{workload}-rtl.bin",dtype=numpy.uint16)
    ones = numpy.fromfile(f"{base_dir}/dump-ones/{workload}-rtl.bin",dtype=numpy.uint16)
    N = np.uint32(toggles[-3]) << 16 | np.uint32(toggles[-4])
    M = np.uint32(toggles[-1]) << 16 | np.uint32(toggles[-2])
    toggles_reshape = toggles[:-4].reshape(N,-1)
    w_toggles = numpy.transpose(toggles_reshape)
    ones_reshape = ones[:-4].reshape(N,-1)
    w_ones = numpy.transpose(ones_reshape)
    return w_toggles, w_ones

def read_toggle_bin_file(workload, rtl='RocketConfig'):
    base_dir = "/tools/C/nayiri/power/power-analysis/fsdb/bin"
    bin_file = f"{base_dir}/{workload}-rtl.bin"
    if not os.path.exists(bin_file): return []
    array = numpy.fromfile(bin_file,dtype=numpy.uint16)
    if len(array) < 4: return []
    N = numpy.uint32(array[1]) << 16 | numpy.uint32(array[0])
    M = numpy.uint32(array[3]) << 16 | numpy.uint32(array[2])
    arr_reshape = array[4:].reshape(-1,M)
    w_toggles = numpy.transpose(arr_reshape)
    return w_toggles

def short_isa_names(isa_list):
    return list(map(lambda x: x.replace('rv64ui-v-',''), isa_list))

def get_workload_toggles_jpower(toggles, jpower, borders, idx):
    start = borders[idx]
    end = borders[idx+1]
    w_toggles = toggles[start:end,:]
    w_jpower = jpower[start:end]
    return w_toggles, w_jpower

def get_workloads_toggles_jpower(workloads, rtl='RocketConfig', power_level='rtl'):
    num_signals = get_num_idcodes()
    toggles = np.ndarray((0,num_signals))
    jpower = []
    prev_w = 'start'
    workload_borders = {prev_w:0}
    for w in workloads:
        w_toggles = read_toggle_bin_file(w)
        w_jcycles, w_jpower = read_profile_file(w)
        if abs(len(w_jpower) - len(w_toggles)) > 5:
            print(f"{w}: Large mismatch btwn data size ({len(w_jpower)} jpower vs {len(w_toggles)} toggles)")
            continue
        size = min(len(w_jpower), len(w_toggles))
        w_jpower = w_jpower[:size]
        w_toggles = w_toggles[:size]
        # if toggles is None: 
        #     toggles = w_toggles
        # else:
        if toggles.shape[1] != w_toggles.shape[1]: 
            print(f"{w}: Mismatch in number of signals ({toggles.shape[1]} vs {w_toggles.shape[1]})")
            continue
        toggles = numpy.concatenate((toggles,w_toggles))
            
        jpower += w_jpower
        workload_borders[w] = workload_borders[prev_w]+size
        prev_w = w
    return toggles, numpy.array(jpower), workload_borders

def get_workloads_toggles_jpower0(workloads, rtl='RocketConfig', power_level='rtl'):
    num_signals = get_num_idcodes()
    toggles = np.ndarray((0,num_signals))
    jpower = []
    workload_borders = [0]
    for w in workloads:
        w_toggles = read_toggle_bin_file(w)
        w_jcycles, w_jpower = read_profile_file(w)
        if abs(len(w_jpower) - len(w_toggles)) > 5:
            print(f"{w}: Large mismatch btwn data size ({len(w_jpower)} jpower vs {len(w_toggles)} toggles)")
            continue
        size = min(len(w_jpower), len(w_toggles))
        w_jpower = w_jpower[:size]
        w_toggles = w_toggles[:size]
        # if toggles is None: 
        #     toggles = w_toggles
        # else:
        if toggles.shape[1] != w_toggles.shape[1]: 
            # print(f"{w}: Mismatch in number of signals ({toggles.shape[1]} vs {w_toggles.shape[1]})")
            continue
        toggles = numpy.concatenate((toggles,w_toggles))
            
        jpower += w_jpower
        workload_borders.append(workload_borders[-1]+size)
    return toggles, numpy.array(jpower), numpy.array(workload_borders)

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
    idx_less_than = [(b,w) for w,b in workload_borders.items() if idx < b]
    b_w_leftmostborder = sorted(idx_less_than)[0]
    return b_w_leftmostborder[1]

def get_workloads_data(rtl, workloads, power_level='rtl'):
    deltas = []
    jpower = []
    bmark_borders = [0]
    for w in workloads:
        w_jcycles, w_jpower = read_profile_file(w, rtl, power_level)
        opm_file = f"{output_dir}/chipyard.TestHarness.{rtl}/opm/{w}.{power_level}.opm"
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


def predict(_X, _y, regr_scaler_X_y,
            # scaler_X, scaler_y,
            degree=1, print_oneline=False, 
            return_r2=False,
            return_nrmse=False,
            verbose=False):
    regr, scaler_X, scaler_y = regr_scaler_X_y
    # scaler_X = sklearn.preprocessing.StandardScaler(with_mean=False)
    # scaler_y = sklearn.preprocessing.StandardScaler()
    # X = scaler_X.fit_transform(numpy.array(_X))
    X = scaler_X.fit_transform(_X)
    y = scaler_y.fit_transform(numpy.array(_y).reshape(-1,1)).reshape(-1)
    # create polynomial inputs 
    if degree > 1:
        polyf = preprocessing.PolynomialFeatures(degree=degree, include_bias=False)
        X = polyf.fit_transform(X) #.reshape(-1, 1))

    # report score
    _r2 = round(regr.score(X, y),2)
    yp = regr.predict(X) 
    _yp = scaler_y.inverse_transform(yp.reshape(-1,1)).reshape(-1)
    _nrmse = round(get_nrmse(_y,_yp)*100)
    if not print_oneline:
        print(f"R^2 = {_r2}, NRMSE = {_nrmse}%")
    else:
        print('\t', _r2, '\t', _nrmse, end='')
    
    if verbose: print("Predicting y based on X...")

    # use model to predict labels for inputs
    _y_pred = regr.predict(X)
    y_pred = scaler_y.inverse_transform(_y_pred.reshape(-1,1)).reshape(-1)
    ret_val = y_pred
    
    if return_r2 or return_nrmse:
        ret_val = (ret_val,)
    if return_r2:
        ret_val = (_r2,)
    if return_nrmse:
        ret_val += (_nrmse,)
    
    return ret_val

@ignore_warnings(category=ConvergenceWarning)
def fit(_X, _y,
        degree=1, 
        type='linear', alpha=1.0, l1_ratio=0.5, gamma=3,
        alphas=numpy.logspace(-1,2,num=10),
        l1_ratios=[.1, .5, .7, .9, .95, .99, 1], # recommended from ElasticNetCV docs
        max_iter=1000, print_oneline=False,
        verbose=False,
        return_r2=False,
        return_nrmse=False):
    
    scaler_X = sklearn.preprocessing.StandardScaler()
    scaler_y = sklearn.preprocessing.StandardScaler()
    # X = scaler_X.fit_transform(numpy.array(_X))
    # y = scaler_y.fit_transform(numpy.array(_y).reshape(-1,1)).reshape(-1)
    X = scaler_X.fit_transform(_X)
    y = scaler_y.fit_transform(numpy.array(_y).reshape(-1,1)).reshape(-1)
    # X = _X
    # y = _y
    
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
                                            max_iter=1000, cv=5)
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
    
    if verbose: print(f"Fitting model to data...")

    # fit model to data
    regr.fit(X, y)

    if verbose or print_oneline or return_r2 or return_nrmse:
        _r2 = round(regr.score(X, y),2)
        if verbose or return_nrmse:
            yp = regr.predict(X) 
            _yp = scaler_y.inverse_transform(yp.reshape(-1,1)).reshape(-1)
            _nrmse = get_nrmse(_y,_yp)*100
            print(f"R^2 = {_r2}, NRMSE = {round(_nrmse)}%")
        elif print_oneline: print('\t', _r2, end='')

    coefs = [c for c in regr.coef_ if c > 0]
    if verbose: print("Num coefs: ", len(coefs))

    ret_val = (regr, scaler_X, scaler_y)

    if return_r2 or return_nrmse:
        ret_val = (ret_val,)

    if return_r2:
        ret_val += (_r2,)
    if return_nrmse:
        ret_val += (_nrmse,)

    return ret_val


def fit_and_predict(_X, _y,
                    degree=1, 
                    type='linear', alpha=1.0, l1_ratio=0.5,
                    alphas=numpy.logspace(-1,2,num=10),
                    l1_ratios=[.1, .5, .7, .9, .95, .99, 1], # recommended from ElasticNetCV docs
                    print_oneline=False, verbose=False):
    
    regr = fit(_X, _y,
                degree=degree, 
                type=type, alpha=alpha, l1_ratio=l1_ratio,
                alphas=alphas,
                l1_ratios=l1_ratios, # recommended from ElasticNetCV docs
                print_oneline=print_oneline,
                verbose=verbose)

    return regr, predict(_X, _y, regr,
                         degree=degree,
                         print_oneline=print_oneline, verbose=verbose)
    
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

def plot_borders(workload_borders, ymax):
    if not workload_borders: return
    b_w = sorted(zip(workload_borders.values(), workload_borders.keys()))
    w_b = {b_w[i+1][1]:b_w[i][0] for i in range(len(b_w)-1)}
    for w, b in w_b.items():
        plt.axvline(x=b, c='k', linestyle='--', linewidth=0.8)
        plt.text(b, ymax, w.replace('rv64ui-v-',''), rotation=50, horizontalalignment='left')
    
    # final border
    plt.axvline(x=b_w[-1][0], c='k', linestyle='--', linewidth=0.8)

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
    plot_borders(workload_borders,max(errors)*1.1)

def plot_power(y, yp=None, workload_borders={}, error=False, x=None, xp=None,
               legend=True, labels=None, marker='-',
               title=None, fig=None, ax=None, y_first=True):
    '''
    workload_borders: { workload: border x_coor, ... }
    '''
    if fig is None:
        fig = plt.figure(figsize=(13,4))
    if ax is None:
        ax = fig.add_subplot()
    
    ymin = 0 if error else min(y)
    xmax = len(y)+100
    ymax = max(y)*1.1

    if not y_first:
        if x is not None:
            ax.plot(x,y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
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
    
    if y_first:
        if x is not None:
            ax.plot(x,y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
        else:
            ax.plot(y, 'g'+marker, label=labels[0] if labels else "Joules Power Trace")
    
    plot_borders(workload_borders, max(y))
    
    ax.set_xlim(0,len(y))
    ax.set_ylim(0,ymax)
    plt.xlabel(f'Windows of N Cycles (N={N_CYCLES})') # only show for last one
    ax.set_ylabel(f'Power (mW)')
    
    if legend:
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles, labels, 
                   loc='upper right', bbox_to_anchor=(1,1), ncol=2, 
                   frameon=True)
        

def read_profile_file_levels(profile_file):
    with open(profile_file,'r') as f:
        lines = f.readlines()

    first_line = lines.pop(0)
    first_line = first_line.replace('/ChipTop/system/','').replace(':all:total','')
    first_line_words = first_line.split()
    start = first_line_words.index('-ykeylabel')
    end = first_line_words.index('-title')
    y_labels = first_line_words[start+1:end]
    time_unit, _ = get_time_power_units(first_line)

    time_power = [l.split() for l in lines]
    time = list(map(lambda x: convert_to_sec(float(x[0]), time_unit), time_power))
    cycles = list(map(lambda x: sec_to_cycle(x), time))
#     power = list(map(lambda x: float(x[1]), time_power))
    power = [[float(p) for p in list(l[1:])] for l in time_power]
    power_T = [list(t) for t in list(zip(*power))]
    power_labels = zip(power_T, y_labels)
    power_labels_nonzero = [(p,l) for p,l in power_labels if sum(p) > 0]
    # print(len(power_T), len(y_labels))
    power_T_nonzero = [p for p,_ in power_labels_nonzero]
    power = [list(t) for t in list(zip(*power_T_nonzero))]
    
    return cycles, power, power_labels_nonzero

def get_uniform_indexes(len, num=None, percent=None):
    if num is None:
        if percent is None:
            percent = 5
        num = int(len*0.01*percent)
        
    return np.linspace(0, len-1, num=num, dtype=int)

def get_train_test_split(_X, _y, indexes=None,
                         train_size=None,
                         normalize=False):
    if indexes is None:
        if train_size is None: train_size = int(0.05*len(_y))
        if type(train_size) is float: train_size = int(train_size*len(_y))
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

def get_train_test_workload_borders(workload_borders, indexes):
    return

def get_cluster_center_indexes(kmeans, scaled_features):
    cluster_dists = [[] for _ in range(kmeans.n_clusters)]  # [(dist,data_idx) for k in clusters for data in data_in_k]
    for i,k in enumerate(kmeans.labels_):
        i_data = scaled_features[i]
        i_center = kmeans.cluster_centers_[k]
        # dist = math.sqrt(sum([(x-c)**2 for x,c in zip(i_data,i_center)]))
        dist = sum([abs(x-c) for x,c in zip(i_data,i_center)])
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


def get_module_profile_dir(w, rtl='RocketConfig', power_level='rtl'):
    profile_dir = f"{build_dir}/chipyard.TestHarness.{rtl}-ChipTop/power-{power_level}-rundir/plot_profile/modules-{w}"
    if not os.path.exists(profile_dir):
        os.makedirs(profile_dir,exist_ok=True)
    return profile_dir


def get_profile_filepath(workload, rtl='RocketConfig', 
                         power_level='rtl', module='chiptop',
                         toggles=N_CYCLES,
                         levels=None):
    basename = f"{build_dir}/chipyard.TestHarness.{rtl}-ChipTop/power-{power_level}-rundir/plot_profile"
    filename = f"{workload}-{power_level}"
    # if module:
    #     filename += f"-module_{module}"
    # if toggles:
    #     filename += f"-toggles_{toggles}"
    # if levels:
    #     filename += f"-levels_{levels}"
    # return f"{basename}/{filename}.png.data"
    return f"{basename}/{filename}.png.data"

# THIS IS BROKEN
def get_module_profile_filepath(workload, module='chiptop',
                                rtl='RocketConfig', 
                                power_level='rtl', 
                                toggles=N_CYCLES,
                                levels=None):
    profile_dir = get_module_profile_dir(workload)
    filename = profile_dir+'/'
    if module:
        filename += f"module_{module}"
    if toggles:
        filename += f"-toggles_{toggles}"
    if levels:
        filename += f"-levels_{levels}"
    # return f"{basename}/{filename}.png.data"
    return f'{profile_dir}/module_{module}-inst_{i}-toggles_{N_CYCLES}.png.data'

def get_fsdb_idcodes(w):
    with open(get_fsdb_idcodes_path(w), 'r') as f:
        line = f.readlines()[0]
        words = line.split()
        idcodes = [int(w) for w in words[1:]]
        assert(int(words[0]) == len(idcodes))
    return idcodes

def get_num_idcodes():
    with open(get_fsdb_idcodes_path('all'), 'r') as f:
        return int(f.readlines()[0].strip())


def dump_proxy_signals(indexes, filename):
    with open(get_fsdb_idcodes_path('all'), 'r') as f:
        lines = f.readlines()[1:]
    proxy_lines = [lines[i] for i in indexes]
    proxies = [l.split()[1] for l in proxy_lines]
    if not filename.startswith('/'):
        filename = f"{power_analysis_dir}/out/{filename}.txt"
    with open(filename,'w') as f:
        for p in proxies:
            f.write(p+'\n')
    print(f"Wrote signals to: {filename}")



def get_hammer_yaml_file(w, rtl='RocketConfig', 
                         power_level='rtl', module='chiptop',
                         toggles=N_CYCLES,
                         levels=None):
    return f"{power_analysis_dir}/yaml_configs/{rtl}_{w}-{power_level}.yml"

# def get_modules_list(path=None):
#     if path is None:
#         path = f"{analysis_output_dir}/module-data"
#     files = os.listdir(path)
#     modules = []
#     for f in files:
#         if not f.endswith('toggles.npy'): continue
#         if not (f.replace('toggles','jpower') in files): continue
#         f = f.replace('module_','').replace('-toggles.npy','')
#         modules.append(f)
#     return modules

def get_modules_data(path=None):
    module_toggles_dict = {}
    module_jpower_dict = {}
    with open(get_module_data_path(type='summary'),'r') as f:
        summary_dict = json.load(f)
    for m in summary_dict['modules']:
        module_toggles_dict[m] = np.load(get_module_data_path(m,'toggles'))
        module_jpower_dict[m]  = np.load(get_module_data_path(m,'jpower'))
    return module_toggles_dict, module_jpower_dict, summary_dict['workloads']