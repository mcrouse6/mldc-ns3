import numpy as np
import itertools


#############################
######## PARAMETERS #########
#############################

# how many TORs to use
#num_TORs = 32
num_TORs = 12

# how many time steps to generate flows for
time_steps = 1

# minimum and maximum number of flows to genereate per timestep
min_num_flows_per_timestep = 15
max_num_flows_per_timestep = 16

# interrack lambda for poisson sampling
internode_lambda = 1.0

# num wireless allocations per time step
num_wireless_allocations = 4

# num allocs per flow file
num_allocs_per_flow = 4

# output file name
base_output_fn = 'new-flow'
output_fn_flows = base_output_fn + '__flows.dat'
output_fn_allocations = base_output_fn + '__alloc.dat'

###########################
###########################
###########################


# open .dat flow file
flow_h = open(output_fn_flows, 'w')
total_num_flows = 0
total_num_allocs = 0

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

	allocation_entry_str = '%d %d %d Tcp %d %dGbps' % (time_step, from_node, to_node, amount_data, data_rate)



# list of idxs for TORs 
# this aligns with the idxs used by NS-3
TOR_idxs = np.arange(num_TORs)

# create all possible pairs of flows
all_pairs = list(itertools.permutations(TOR_idxs, 2)) 



# generate flows for each time step
for time_step in range(time_steps):

	# sample how many flows we will have at a given time step
	num_flows = np.random.randint(min_num_flows_per_timestep, max_num_flows_per_timestep)

	# select actual pairs of to and from nodes
	flowidxs_to_use = np.random.choice(len(all_pairs), size=num_flows, replace=False)

	# create actual entry
	#for flowidx_to_use in range(len(flowidxs_to_use)):
	for idx in range(len(flowidxs_to_use)):
	# for flowidx_to_use in flowidxs_to_use:
		# create_flow_entry(time_step, all_pairs[flowidx_to_use][0], all_pairs[flowidx_to_use][1])
		create_flow_entry(idx, time_step, all_pairs[flowidxs_to_use[idx]][0], all_pairs[flowidxs_to_use[idx]][1])
		total_num_flows += 1

# close .dat file with flows
flow_h.close()

print "A total of %d flows have been created" % (total_num_flows)
print "Flows saved in %s" % (output_fn_flows)

for k in range(num_allocs_per_flow):
	allocation_h = open(output_fn_allocations.split('.')[0] + '_' + str(k) + '.dat', 'w')

	flow_h = open(output_fn_flows, 'r')
	for i, line in enumerate(flow_h):

		# generate wired/wireless allocations
		wired_wireless_allocations = np.zeros(len(all_pairs))
		wired_wireless_allocations[np.random.choice(num_flows, size=num_wireless_allocations, replace=False)] = 1
		wired_wireless_allocations += 1

		allocation_h.write(line.split('\n')[0] + ' %d' % (wired_wireless_allocations[i]))
		allocation_h.write('\n')

	allocation_h.close()

	total_num_allocs += 1


print "A total of %d allocs have been created for the flow log" % (total_num_allocs)



