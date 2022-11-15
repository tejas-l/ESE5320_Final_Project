// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#include "xparameters.h"
#include "xlzw_encoding_hw.h"

extern XLzw_encoding_hw_Config XLzw_encoding_hw_ConfigTable[];

XLzw_encoding_hw_Config *XLzw_encoding_hw_LookupConfig(u16 DeviceId) {
	XLzw_encoding_hw_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XLZW_ENCODING_HW_NUM_INSTANCES; Index++) {
		if (XLzw_encoding_hw_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XLzw_encoding_hw_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLzw_encoding_hw_Initialize(XLzw_encoding_hw *InstancePtr, u16 DeviceId) {
	XLzw_encoding_hw_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLzw_encoding_hw_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLzw_encoding_hw_CfgInitialize(InstancePtr, ConfigPtr);
}

#endif

