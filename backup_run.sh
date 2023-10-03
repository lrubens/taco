#!/bin/bash

NUM_ITER=1

# ablation
for mat in filter3D email-Enron amazon0312; do
	for i in {1..$NUM_ITER}; do
		./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t simple -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -m ytopt -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t hamming -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t kendall -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t naive -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t nms -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t nolog -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t nolsp -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $1.mtx -n 10 -c $i -t RF -o SpMM
	done
done


#SpMM
for mat in cage12 scircuit laminar_duct3D; do
	for i in {1..$NUM_ITER}; do
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m bayesian_optimization -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m opentuner -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m ytopt_ccs -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m random_sampling -o SpMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m embedding_random_sampling -o SpMM
	done
done



#SDDMM
for mat in email-Enron ACTIVSg10K Goodwin_040; do
	for i in {1..$NUM_ITER}; do
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m bayesian_optimization -o SDDMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m opentuner -o SDDMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m ytopt_ccs -o SDDMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m random_sampling -o SDDMM
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m embedding_random_sampling -o SDDMM
	done
done


#SPMV
for mat in laminar_duct3D cage12 filter3D; do
	for i in {1..$NUM_ITER}; do
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m bayesian_optimization -o SpMV
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m opentuner -o SpMV
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m ytopt_ccs -o SpMV
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m random_sampling -o SpMV
		numactl -i all ./bin/taco-taco_dse -mat $mat.mtx -n 10 -c $i -m embedding_random_sampling -o  SpMV
	done
done


#TTV
for mat in facebook uber3 random; do
	for i in {1..$NUM_ITER}; do
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m bayesian_optimization -o TTV
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m opentuner -o TTV
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m ytopt_ccs -o TTV
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m random_sampling -o TTV
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m embedding_random_sampling -o TTV
	done
done


#MTTKRP
for mat in uber nips chicago; do
	for i in {1..$NUM_ITER}; do
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m bayesian_optimization -o MTTKRP
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m opentuner -o MTTKRP
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m ytopt_ccs -o MTTKRP
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m random_sampling -o MTTKRP
		numactl -i all ./bin/taco-taco_dse -mat $mat.tns -n 10 -c $i -m embedding_random_sampling -o MTTKRP
	done
done