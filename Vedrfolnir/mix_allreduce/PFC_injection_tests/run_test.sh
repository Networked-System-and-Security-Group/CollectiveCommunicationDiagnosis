#!/bin/bash

cd ../../
config_files=("50us_1000us_1_case0" "50us_1000us_1_case1" "50us_1000us_1_case2" "50us_1000us_1_case3" "50us_1000us_1_case4" "50us_1000us_1_case5" "50us_1000us_1_case6" "50us_1000us_1_case7" "50us_1000us_1_case8" "50us_1000us_1_case9" "50us_1000us_1_case10" "50us_1000us_1_case11" "50us_1000us_1_case12" "50us_1000us_1_case13" "50us_1000us_1_case14" "50us_1000us_1_case15" "50us_1000us_1_case16" "50us_1000us_1_case17" "50us_1000us_1_case18" "50us_1000us_1_case19" "50us_1000us_1_case20" "50us_1000us_1_case21" "50us_1000us_1_case22" "50us_1000us_1_case23" "50us_1000us_1_case24" "50us_1000us_1_case25" "50us_1000us_1_case26" "50us_1000us_1_case27" "50us_1000us_1_case28" "50us_1000us_1_case29" "50us_1000us_1_case30" "50us_1000us_1_case31" "50us_1000us_1_case32" "50us_1000us_1_case33" "50us_1000us_1_case34" "50us_1000us_1_case35" "50us_1000us_1_case36" "50us_1000us_1_case37" "50us_1000us_1_case38" "50us_1000us_1_case39" )

printf "%s\n" "${config_files[@]}" | xargs -P 100 -I{} bash -c '
    file={}
    python ./waf --run "scratch/mix_allreduce mix_allreduce/PFC_injection_tests/config/config_${file}.txt"
    python3 mix_allreduce/PFC_injection_tests/graph.py "mix_allreduce/PFC_injection_tests/data/data_${file}"
'
