import sys
import os
import glob
import argparse
import numpy as np

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert Flow Logs and Allocations to X Y data for Learning')
    parser.add_argument('-f','--flow_dir', help='flow log/allocations dir', required=True)
    parser.add_argument('-r','--results_dir', help='ns-3 simulation results directory', required=True)
    parser.add_argument('-g','--gain', help='Antenna Gain for each of the TOR antennas', type=float, default=25.0)
    args = vars(parser.parse_args())

    flow_dir = args['flow_dir'] 
    results_base = args['results_dir']

    if not os.path.exists(results_base):
        os.makedirs(results_base)
    else:
        print "Output Directory Exists, do you want to proceed? [y/n]"
        k = raw_input()
        if k == 'y':
            pass
        else: 
            assert False, "quitting"

    flow_files = glob.glob('%s/flows*.dat' % (flow_dir))
    gain = 25

    for flow_log in flow_files:
        flow_log_id = int(flow_log.split("__")[1].split(".")[0])
        print flow_log_id
        link_configs = glob.glob("%s/alloc__%d_*" % (flow_dir, flow_log_id))
        for link_config in link_configs:
            link_config_id = int(link_config.split("_")[-1].split('.')[0])
            output_fn = "%s/%d_%d.txt" % (results_base, flow_log_id, link_config_id)
            cmd = "./waf --run \"flyway-data \
                        --topo=scratch/topo-search.dat \
                        --flow=scratch/%s \
                        --outfile=scratch/%s \
                        --gain=%02f\"" % (link_config, output_fn, gain)
            print cmd
            os.system("cd ~/Research/datacenter/mldc_ns3/ns-3.9/; " + cmd)

