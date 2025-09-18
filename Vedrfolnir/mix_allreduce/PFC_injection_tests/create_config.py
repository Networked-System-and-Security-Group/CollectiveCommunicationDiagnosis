# -*- coding: utf-8 -*-
import os

threshold_list = ["50"]
epoch_time_list = ["1000"]
step_detect_times_list = ['1']

file = open("config_tp.txt", "r")
lines = file.readlines()
file.close()

if not os.path.exists("data"):
    os.makedirs("data")
if not os.path.exists("data_result"):
    os.makedirs("data_result")
if not os.path.exists("config"):
    os.makedirs("config")

cases = [
    (0, 4, 2.117, 0.076),
    (0, 4, 2.07, 0.012),
    (0, 4, 2.141, 0.017),
    (0, 4, 2.144, 0.09),
    (0, 4, 2.054, 0.01),
    (0, 4, 2.004, 0.059),
    (0, 4, 2.063, 0.015),
    (0, 4, 2.057, 0.052),
    (0, 4, 2.029, 0.042),
    (0, 4, 2.07, 0.097),
    (5, 1, 2.085, 0.067),
    (5, 1, 2.03, 0.032),
    (5, 1, 2.087, 0.029),
    (5, 1, 2.035, 0.038),
    (5, 1, 2.122, 0.079),
    (5, 1, 2.047, 0.065),
    (5, 1, 2.134, 0.029),
    (5, 1, 2.13, 0.027),
    (5, 1, 2.106, 0.075),
    (5, 1, 2.095, 0.087),
    (10, 2, 2.039, 0.081),
    (10, 2, 2.065, 0.046),
    (10, 2, 2.031, 0.08),
    (10, 2, 2.121, 0.072),
    (10, 2, 2.058, 0.037),
    (10, 2, 2.123, 0.069),
    (10, 2, 2.051, 0.07),
    (10, 2, 2.097, 0.079),
    (10, 2, 2.039, 0.082),
    (10, 2, 2.098, 0.045),
    (2, 3, 2.146, 0.072),
    (2, 3, 2.135, 0.088),
    (2, 3, 2.031, 0.083),
    (2, 3, 2.104, 0.083),
    (2, 3, 2.137, 0.062),
    (2, 3, 2.148, 0.06),
    (2, 3, 2.145, 0.069),
    (2, 3, 2.04, 0.012),
    (2, 3, 2.082, 0.082),
    (2, 3, 2.104, 0.021),
]

config_files = []

for threshold in threshold_list:
    for epoch_time in epoch_time_list:
        for detect_times in step_detect_times_list:
            for i, case in enumerate(cases):
                file_str = threshold+"us_"+epoch_time+"us_"+detect_times+"_case"+str(i)
                config_files.append(file_str)
                config_file = open("config/config_"+file_str+".txt", "w")
                for line in lines:
                    config_file.write(line)
                config_file.write("\n")
                data_path = "mix_allreduce/PFC_injection_tests/data/data_"+file_str
                config_file.write(f"PFC_STORM {case[0]} {case[1]} {case[2]} {case[3]}\n")
                config_file.write("DIR "+ data_path + "\n")
                config_file.write("FCT_OUTPUT_FILE "+ data_path + "/fct.txt" +"\n")
                config_file.write("PFC_OUTPUT_FILE "+ data_path + "/pfc.txt" +"\n")
                config_file.write("AGENT_THRESHOLD "+threshold+"\n")
                config_file.write("EPOCH_TIME "+epoch_time+"000\n")
                config_file.write("STEP_DETECT_TIMES "+detect_times+"\n")
                
                config_file.close()
                dir = "data/data_"+file_str
                if not os.path.exists(dir):
                    # print("create dir")
                    os.makedirs(dir)

#create run_test.sh
file = open("run_test.sh", "w")
file.write("#!/bin/bash\n")
file.write("\n")
file.write("cd ../../\n")
file.write("config_files=(")

for file_str in config_files:
    file.write("\""+file_str+"\" ")
file.write(")\n")
file.write("\n")

file.write("printf \"%s\\n\" \"${config_files[@]}\" | xargs -P 100 -I{} bash -c '\n")
file.write("    file={}\n")
file.write("    python ./waf --run \"scratch/mix_allreduce mix_allreduce/PFC_injection_tests/config/config_${file}.txt\"\n")
file.write("    python3 mix_allreduce/PFC_injection_tests/graph.py \"mix_allreduce/PFC_injection_tests/data/data_${file}\"\n")
file.write("'\n")
file.close()