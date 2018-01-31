import sys
import os
import numpy as np
import glob


base_output_fn = "flow-data-15-200-2"
flow_files = glob.glob('%s/flows*.dat' % (base_output_fn))
gain = 25

for flow_log in flow_files:
    flow_log_id = int(flow_log.split("__")[1].split(".")[0])
    print flow_log_id
    link_configs = glob.glob("%s/alloc__%d_*" % (base_output_fn, flow_log_id))
    for link_config in link_configs:
        link_config_id = int(link_config.split("_")[-1].split('.')[0])
        output_fn = "results-15-200-2/%d_%d.txt" % (flow_log_id, link_config_id)
        cmd = "./waf --run \"flyway-data \
                     --topo=scratch/topo.dat \
                     --flow=scratch/%s \
                     --wl=scratch/%s \
                     --outfile=scratch/%s \
                     --gain=%02f\"" % (flow_log, link_config, output_fn, gain)
        print cmd
        os.system("cd ~/Research/datacenter/mldc_ns3/ns-3.9/; " + cmd)

