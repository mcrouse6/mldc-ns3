import numpy as np
import glob
import argparse


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
