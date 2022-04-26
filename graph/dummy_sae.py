# create a DGL object for the insts (modules) and edges (io ports)
import dgl
import dgl.function as fn
import numpy
import torch
import random
import json

UV = [ (0,4), (1,4), (1,7), (1,8), (1,9), (2,6), (3,5), (4,7), (5,6), (6,8), (6,9), (7,10), (8,10), (9,10), (9,12), (10,11) ]
stages = [ [0,1,2,3], [4,5], [6,7], [8,9], [10,12], [11] ] 
edge_stages = [ [0,1,2,3,4,5,6], [7,8], [9,10,11], [12,13,14], [15] ] 
edges = UV
NUM_STAGES = len(stages)
NUM_NODES=numpy.max(UV)+1
nodes = range(0,NUM_NODES)
NUM_EDGES=len(UV)
NUM_WINDOWS=10

U = []
V = []
for uv in UV:
    U.append(uv[0])
    V.append(uv[1])




pinorders = [ -1 for edge in edges ]
node_pin_count = [ -1 for node in nodes ]
for edge in edges:
    node_pin_count[edge[1]] += 1
    pinorders[edges.index(edge)] = node_pin_count[edge[1]]
MAX_NUM_INPUT_PINS=numpy.max(pinorders)+1
# print(pinorders)
# print(MAX_NUM_INPUT_PINS)








# toggle_rates = chance to [stay low, stay high, switch to low, switch to high] in a given time window
# this is what we're trying to estimate using the switching activity estimation (SAE)
toggle_rates = [[[0,0,0,0] for w in range(0,NUM_WINDOWS)] for n in range(0,NUM_NODES)]
for n in range(0,NUM_NODES):
    for w in range(0,NUM_WINDOWS):
        h1 = random.randint(0,100)
        h2 = random.randint(0,100)
        h3 = random.randint(0,100)
        h4 = random.randint(0,100)
        sum = h1+h2+h3+h4
        h1 = abs(round(h1/sum,2))
        h2 = abs(round(h2/sum,2))
        h3 = abs(round(h3/sum,2))
        h4 = abs(round(1 - h1 - h2 - h3,2))
        toggle_rates[n][w] = [h1,h2,h3,h4]
# print(toggle_rates)








# sim_gl_signals = from VCD of gate-level simulation
# sim_gl_signals[window][edge]
sim_gl_signals = [[0 for e in range(0,NUM_EDGES)] for w in range(0,NUM_WINDOWS)]
for w in range(0,NUM_WINDOWS):
    for e in range(0,NUM_EDGES):
        sim_gl_signals[w][e] = round(random.random(),2)



# sim_rtl_signals = from VCD of RTL simulation
# only contains values of input and output port signals
sim_rtl_signals = [[0 for e in range(0,NUM_EDGES)] for w in range(0,NUM_WINDOWS)]
for w in range(0,NUM_WINDOWS):
    for e in edge_stages[0]:
        sim_rtl_signals[w][e] = sim_gl_signals[w][e]
    for e in edge_stages[-1]:
        sim_rtl_signals[w][e] = sim_gl_signals[w][e]
# print(sim_rtl_signals)






module_ports_dict = json.load(open("modules_ports1.json",'r'))
logic_cell_types = [module for module in module_ports_dict]
LUT = [ [ [0] for pin in range(0,MAX_NUM_INPUT_PINS) ] for module in nodes ]
# LUT[module][input pin order][4-dim toggle rate weights]
for module in nodes:
    for edge in edges:
        pinorder = pinorders[edges.index(edge)]
        if module == edge[1]:
            # print((module,pinorder))
            toggle_rate_weights = [round(random.random(),2) for i in range(1)]
            LUT[module][pinorder] = toggle_rate_weights
# print(LUT)











g = dgl.graph((U,V))
# print(g.edges())
# g.ndata['h'] = torch.zeros((g.num_nodes(), MAX_NUM_INPUT_PINS)) # each node has feature size of MAX_NUM_INPUT_PINS
g.ndata['inputs'] = torch.zeros((g.num_nodes(), MAX_NUM_INPUT_PINS)) 
g.ndata['output'] = torch.zeros((g.num_nodes(), 1))
# edge features = [ pinorder, sim signal value ]
g.edata['pinorder_signal'] = torch.zeros((g.num_edges(), 2))  
# g.edata['pinorder'] = torch.zeros((g.num_edges(), 1))  
# g.edata['signal'] = torch.zeros((g.num_edges(), 1))  




# initialize edges to pin orders
for i in range(0,len(edges)):
    e=edges[i]
    g.edata['pinorder_signal'][i][0] = pinorders[i]
    # g.edata['pinorder'][i] = pinorders[i]
# print(g.edata)


# for e in edge_stages[0]:
#     w=0
#     g.edata['pinorder_signal'][e][1] = sim_gl_signals[w][e]
# print(g.edata)


# def concat_message_function(edges):
#     return {'cat_feat': torch.cat( (edges.src['h'],edges.data['w']), 1 )}




# DGLGraph.apply_edges(func, edges='__ALL__', etype=None, inplace=False)
# DGLGraph.prop_nodes(nodes_generator, message_func, reduce_func, apply_node_func=None, etype=None)




def set_edge_signal(edges):
    result = edges.data['pinorder_signal']
    outputs = edges.src['output']
    for i_signal in range(0,edges.batch_size()):
        result[i_signal][1] = outputs[i_signal]
    return {'pinorder_signal': result }


def concat_msgs_by_pinorder(nodes):
    return_tensor = torch.zeros((1, MAX_NUM_INPUT_PINS)) 
    for inputs_tensors in nodes.mailbox['inputs'][0]:
        pinorder=int(inputs_tensors[0].numpy())
        signal=float(inputs_tensors[1].numpy())
        return_tensor[0][pinorder] = signal
    return {'inputs': return_tensor }

def output_from_LUT(nodes):
    result = torch.zeros(nodes.batch_size(),1)
    input_toggles = numpy.ndarray.flatten(nodes.data['inputs'].numpy())
    node_idx = 0
    for node_id_tensor in nodes.nodes():
        node_id = int(node_id_tensor.numpy())
        LUT_weights = (numpy.ndarray.flatten(numpy.asarray(LUT[node_id])))
        tmp = numpy.asarray(numpy.matmul(LUT_weights, input_toggles))
        result[node_idx] = torch.tensor(tmp)
        node_idx += 1
    return {'output': result }


for w in range(0,NUM_WINDOWS):
    for stage in range(1,NUM_STAGES)[::-1]:
        g.apply_edges(set_edge_signal, edges=edge_stages[stage-1])
        g.prop_nodes(stages[stage], fn.copy_edge('pinorder_signal', 'inputs'), concat_msgs_by_pinorder, apply_node_func=output_from_LUT)
    # send input signal toggles 
    for edge in edges:
        if edge[0] in stages[0]:
            g.ndata['output'][stages[0].index(edge[0])] = sim_gl_signals[w][edges.index(edge)]
    print(g.ndata['output'])

