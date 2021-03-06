import numpy as np
import os
import itertools
import argparse


# function to generate a flow between two points
# writes entry to flow .dat file
def create_flow_entry(idx, time_step, from_node, to_node):
	data_per_timestep = 8193
	data_rate = 1
	# amount_data = np.random.randint(1e6, 1e8)#np.random.poisson(lam = internode_lambda)
	amount_data = 100000000
	flow_entry_str = '%d %d %d Tcp %d %dGbps' % (time_step, from_node, to_node, amount_data, data_rate)
	flow_h.write(flow_entry_str)
	flow_h.write('\n')

#############################
######## PARAMETERS #########
#############################

parser = argparse.ArgumentParser(description='Convert Flow Logs and Allocations to X Y data for Learning')
parser.add_argument('-f','--flow_dir', help='flow log/allocations dir', required=True)
parser.add_argument('-t','--num_tors', help='Number of TOR Switches - sets size of X,Y', type=int, required=True)
parser.add_argument('-n','--num_flows', help='number of flow logs to generate', default=100, type=int)
args = vars(parser.parse_args())

# how many TORs to use
#num_TORs = 32
num_TORs = args['num_tors'] 

# how many time steps to generate flows for
time_steps = 1

# minimum and maximum number of flows to genereate per timestep
num_flows = 15 

min_wireless_links = 2
max_wireless_links = 5

# interrack lambda for poisson sampling
internode_lambda = 1.0

# num wireless allocations per time step
num_wireless_allocations = 4

# num allocs per flow file
num_allocs_per_flow = 15

# output file name
base_output_fn = args['flow_dir']
if not os.path.exists(base_output_fn):
        os.makedirs(base_output_fn)
else:
	print "Output Directory Exists, do you want to proceed? [y/n]"
	k = raw_input()
	if k == 'y':
		pass
	else: 
		assert False, "quitting"

num_files = args['num_flows'] 

for file_num in range(num_files):
	output_fn_flows = '%s/flows__%d.dat' % (base_output_fn, file_num)

	###########################
	###########################
	###########################


	# open .dat flow file
	flow_h = open(output_fn_flows, 'w')
	total_num_flows = 0
	total_num_allocs = 0


	# list of idxs for TORs 
	# this aligns with the idxs used by NS-3
	TOR_idxs = np.arange(num_TORs)

	# create all possible pairs of flows
	all_pairs = list(itertools.permutations(TOR_idxs, 2)) 


	# generate flows for each time step
	for time_step in range(time_steps):

		# select actual pairs of to and from nodes
		flowidxs_to_use = np.random.choice(len(all_pairs), size=num_flows, replace=False)

		# create actual entry
		for idx in range(len(flowidxs_to_use)):
			create_flow_entry(idx, time_step, all_pairs[flowidxs_to_use[idx]][0], all_pairs[flowidxs_to_use[idx]][1])
			total_num_flows += 1

	# close .dat file with flows
	flow_h.close()

	print "A total of %d flows have been created" % (total_num_flows)
	print "Flows saved in %s" % (output_fn_flows)

	for k in range(num_allocs_per_flow):
		# allocation_h = open(output_fn_allocations.split('.')[0] + '_' + str(k) + '.dat', 'w')
		allocation_h = open("%s/alloc__%d_%d.dat" % (base_output_fn, file_num, k), 'w')

		# num_wireless = np.random.randint(min_wireless_links, max_wireless_links+1)
		# temporary hard coding
		num_wireless = 3
		wireless_allocations = np.random.permutation(range(num_flows))[:num_wireless]

		flow_h = open(output_fn_flows, 'r')
		for i, line in enumerate(flow_h):

			# generate wired/wireless allocations

			if i in wireless_allocations:
				link_t = 1
			else:
				link_t = 2

			#allocation_h.write(line.split('\n')[0] + ' %d' % (wired_wireless_allocations[i]))
			allocation_h.write(line.split('\n')[0] + ' %d' % (link_t))
			allocation_h.write('\n')

		allocation_h.close()

		total_num_allocs += 1


	print "A total of %d allocs have been created for the flow log" % (total_num_allocs)



