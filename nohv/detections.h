#pragma once

// cpuid.cpp
bool cpuid_detected_1();

// msr.cpp
bool msr_detected_1();
bool msr_detected_2();

// cr0.cpp
bool cr0_detected_1();
bool cr0_detected_2();
bool cr0_detected_3();

// cr3.cpp
bool cr3_detected_1();
bool cr3_detected_2();
bool cr3_detected_3();

// cr4.cpp
bool cr4_detected_1();
bool cr4_detected_2();
bool cr4_detected_3();
bool cr4_detected_4();

// xsetbv.cpp
bool xsetbv_detected_1();
bool xsetbv_detected_2();
bool xsetbv_detected_3();
bool xsetbv_detected_4();
bool xsetbv_detected_5();

// timing.cpp
bool timing_detected_1();
bool timing_detected_2();
bool timing_detected_3();
bool timing_detected_4();
bool timing_detected_5();
bool timing_detected_6();
bool timing_detected_7();

// debug.cpp
bool debug_detected_1();
bool debug_detected_2();

// vmx.cpp
bool vmx_detected_1();
bool vmx_detected_2();
bool vmx_detected_3();

