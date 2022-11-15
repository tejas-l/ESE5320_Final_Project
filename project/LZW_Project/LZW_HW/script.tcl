############################################################
## This file is generated automatically by Vitis HLS.
## Please DO NOT edit it.
## Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
############################################################
open_project LZW_Project
set_top LZW_encoding_HW
add_files fpga/LZW_HW.cpp
add_files fpga/LZW_HW.h
add_files -tb fpga/testbench.cpp
open_solution "LZW_HW" -flow_target vitis
set_part {xczu3eg-sbva484-1-i}
create_clock -period 150MHz -name default
#source "./LZW_Project/LZW_HW/directives.tcl"
csim_design
csynth_design
cosim_design
export_design -format ip_catalog
