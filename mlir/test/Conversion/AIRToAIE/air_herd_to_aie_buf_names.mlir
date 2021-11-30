// (c) Copyright 2021 Xilinx Inc.

// RUN: air-opt -air-to-aie %s | FileCheck %s

// CHECK-LABEL: module @aie.herd_0
// CHECK: buf8
// CHECK: scratch_2_2
// ...
// CHECK: buf0
// CHECK: scratch_0_0
func @launch(%arg0: i32) {
  %cst2 = constant 3 : index
  air.launch_herd tile (%x, %y) in (%sx=%cst2, %sy=%cst2) {
    %buf0 = memref.alloc() {sym_name = "scratch"} : memref<10xindex,2>
    %buf1 = memref.alloc() : memref<10xindex,2>
    air.herd_terminator
  }
  return
}
