import numpy as np
import glob
import argparse
import os
import matplotlib.pyplot as plt


def parseAllocFileLine(line):
    #ts fromNode toNode protocol transferSize dataRate 
    #0 8 6 Tcp 100000000 1Gbps 2
    data = line.split(" ")
    ts = int(data[0])
    from_node = int(data[1])
    to_node = int(data[2])
    link_t = int(data[-1])
    data_size = int(data[4])
    return from_node, to_node, link_t

def parseFlowFileLine(line):
    #ts fromNode toNode protocol transferSize dataRate 
    #0 8 6 Tcp 100000000 1Gbps 2
    data = line.split(" ")
    ts = int(data[0])
    from_node = int(data[1])
    to_node = int(data[2])
    data_size = int(data[4])
    return from_node, to_node


def convertAllocFileToXY(file_name):
    with open(file_name, 'r') as fh:
        X = np.zeros((num_tors, num_tors))
        Y = np.zeros((num_tors,num_tors))
        for line in fh:
            from_node, to_node, link_t = parseAllocFileLine(line) 
            X[from_node, to_node] = 1
            if link_t == 1: # if link is wireless - set to 1 in output
                Y[from_node, to_node] = 1
        Y = Y.reshape((Y.shape[0]**2, -1))
    return X, Y

def convertXYToAllocFile(X, Y, fle_name):
    assert False, "Not Implented"
    """
    with open(file_name, 'w') as fh:
        X = np.zeros((num_tors, num_tors))
        Y = np.zeros((num_tors,num_tors))
        for line in fh:
            from_node, to_node, link_t = parseAllocFileLine(line) 
            X[from_node, to_node] = 1
            if link_t == 1: # if link is wireless - set to 1 in output
                Y[from_node, to_node] = 1
        Y = Y.reshape((Y.shape[0]**2, -1))
    return X, Y
    """

def parseAllFlowFiles(flow_dir, results_dir, num_tors, output_dir):

    link_configs = glob.glob("%s/alloc__*" % (flow_dir))

    Xs = []
    Ys = []
    num_files = len(link_configs)
    for i, file_name in enumerate(link_configs):
        print "%d of %d: Converting %s" % (i, num_files, file_name)
        X, Y = convertAllocFileToXY(file_name)
        Xs.append(X)
        Ys.append(Y)

    Xs= np.array(Xs)
    Xs = Xs.reshape(Xs.shape[0], 1, Xs.shape[1], Xs.shape[2])
    Ys= np.array(Ys)
    return Xs, Ys

def nodeToXY(idx, racks_per_row, x_spacing, y_spacing):
    x_pos = (idx % racks_per_row) * x_spacing
    y_pos = ((idx / racks_per_row) ) * y_spacing
    return x_pos, y_pos

def calcDistance(from_x_pos, from_y_pos, to_x_pos, to_y_pos):
    dx = to_x_pos - from_x_pos
    dy = to_y_pos - from_y_pos
    return dx, dy

def plotConnectionGraph(flow_dir, flow_file, x_spacing=1, y_spacing=3, num_rows=3, racks_per_row=4):
    fig = plt.figure()
    ax = fig.add_subplot(111)

    xs = []
    ys = []
    legend = []
    for row in range(num_rows):
        for rack in range(racks_per_row):
            x_pos = rack*x_spacing
            y_pos = row*y_spacing
            ax.plot(x_pos, y_pos, c='k', markersize=15, marker='s', zorder=1, mfc='none', linestyle = 'None', mew=2)

    with open("%s/%s" % (flow_dir, flow_file), 'r') as flow_fh:
        for line in flow_fh: 
            from_idx, to_idx, link_t = parseAllocFileLine(line)
            if link_t == 1:
                fc = "r"
                width= .1
            else:
                fc = 'b'
                width = .05
            from_x_pos, from_y_pos = nodeToXY(from_idx, racks_per_row, x_spacing, y_spacing)
            to_x_pos, to_y_pos = nodeToXY(to_idx, racks_per_row, x_spacing, y_spacing)
            dx, dy = calcDistance(from_x_pos, from_y_pos, to_x_pos, to_y_pos)
            plt.arrow(from_x_pos, from_y_pos, dx, dy, width=width, length_includes_head=True, ec='none',  fc=fc)
    
    plt.axis('equal')
    plt.show()

def calcAllocValue( results_dir, results_file):
    """
    Sim: 1.000000, WallClock: 20.000000, WallDiff: 20.000000
    [0: 100000000.000000 40.000]
    """

    tp_arr = []
    data_arr = []
    run_time = 0.
    fname = "%s/%s" % (results_dir, results_file)
    if os.path.isfile(fname):
        # with open("%s/%s" % (results_dir, results_file), 'r') as fh:
        with open(fname, 'r') as fh:
            l1 = fh.readline()
            if "Sim:" in l1:
                run_time = float(l1.split(",")[1].split(':')[1])
            for i in range(15):
                line = fh.readline()
                all_data = line.split(" ")
                tp = float(all_data[-1].strip("\n").strip("]"))
                data = float(all_data[1])
                tp_arr.append(tp)
                data_arr.append(data)

        return sum(data_arr)/15.
    else:
        return 0.0
        

def getBestAllocs(flow_file, flow_dir="flow-data-3-200", results_dir="results-3-200-1"):
    results_file_base = flow_file.split("__")[1].split(".")[0]
    max_val = 0
    max_val_idx = 0
    for i in range(15):
        val = calcAllocValue(results_dir, "%s_%d.txt" % (results_file_base, i) )
        if val > max_val:
            max_val = val
            max_val_idx = i
    return max_val, max_val_idx


def plotBestAlloc(flow_file, flow_dir="flow-data-3-200"):
    val, best_idx = getBestAllocs(flow_file)
    flow_id = int(flow_file.split("__")[1].split(".")[0])
    print flow_file, flow_id, best_idx, val
    plotConnectionGraph(flow_dir=flow_dir, flow_file="alloc__%d_%d.dat" % (flow_id, best_idx))



if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Convert Flow Logs and Allocations to X Y data for Learning')
    parser.add_argument('-f','--flow_dir', help='flow log/allocations dir', required=True)
    parser.add_argument('-r','--results_dir', help='ns-3 simulation results directory', required=True)
    parser.add_argument('-o','--output_dir', help='Where to put X,Y numpy files', required=True)
    parser.add_argument('-t','--num_tors', help='Number of TOR Switches - sets size of X,Y', type=int, required=True)
    args = vars(parser.parse_args())

    log_directory = args['flow_dir']
    results_diretory = args['results_dir']
    num_tors = args['num_tors'] 

    Xs, Ys = parseAllFlowFiles(**args)
