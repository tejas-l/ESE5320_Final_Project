// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xlzw_encoding_hw.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XLzw_encoding_hw_CfgInitialize(XLzw_encoding_hw *InstancePtr, XLzw_encoding_hw_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XLzw_encoding_hw_Start(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL) & 0x80;
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XLzw_encoding_hw_IsDone(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XLzw_encoding_hw_IsIdle(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XLzw_encoding_hw_IsReady(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XLzw_encoding_hw_Continue(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL) & 0x80;
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL, Data | 0x10);
}

void XLzw_encoding_hw_EnableAutoRestart(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL, 0x80);
}

void XLzw_encoding_hw_DisableAutoRestart(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_AP_CTRL, 0);
}

void XLzw_encoding_hw_Set_data_in(XLzw_encoding_hw *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_IN_DATA, (u32)(Data));
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_IN_DATA + 4, (u32)(Data >> 32));
}

u64 XLzw_encoding_hw_Get_data_in(XLzw_encoding_hw *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_IN_DATA);
    Data += (u64)XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_IN_DATA + 4) << 32;
    return Data;
}

void XLzw_encoding_hw_Set_len(XLzw_encoding_hw *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_LEN_DATA, Data);
}

u32 XLzw_encoding_hw_Get_len(XLzw_encoding_hw *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_LEN_DATA);
    return Data;
}

void XLzw_encoding_hw_Set_data_out(XLzw_encoding_hw *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_OUT_DATA, (u32)(Data));
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_OUT_DATA + 4, (u32)(Data >> 32));
}

u64 XLzw_encoding_hw_Get_data_out(XLzw_encoding_hw *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_OUT_DATA);
    Data += (u64)XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_DATA_OUT_DATA + 4) << 32;
    return Data;
}

void XLzw_encoding_hw_InterruptGlobalEnable(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_GIE, 1);
}

void XLzw_encoding_hw_InterruptGlobalDisable(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_GIE, 0);
}

void XLzw_encoding_hw_InterruptEnable(XLzw_encoding_hw *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_IER);
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_IER, Register | Mask);
}

void XLzw_encoding_hw_InterruptDisable(XLzw_encoding_hw *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_IER);
    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_IER, Register & (~Mask));
}

void XLzw_encoding_hw_InterruptClear(XLzw_encoding_hw *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLzw_encoding_hw_WriteReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_ISR, Mask);
}

u32 XLzw_encoding_hw_InterruptGetEnabled(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_IER);
}

u32 XLzw_encoding_hw_InterruptGetStatus(XLzw_encoding_hw *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLzw_encoding_hw_ReadReg(InstancePtr->Control_BaseAddress, XLZW_ENCODING_HW_CONTROL_ADDR_ISR);
}

