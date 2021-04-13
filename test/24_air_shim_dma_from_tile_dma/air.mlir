module {

func @graph(%arg0 : memref<256xi32>, %arg1 : memref<256xi32>) -> () {
  %herd_cols = constant 1 : index
  %herd_rows = constant 1 : index
  air.launch_herd tile(%tx, %ty) in (%size_x = %herd_cols, %size_y = %herd_rows) args(%ext0 = %arg0, %ext1 = %arg1) : memref<256xi32>, memref<256xi32> attributes { } {
    %c0 = constant 0 : index
    %c256 = constant 256 : index
    %buf0 = alloc() : memref<256xi32, 2>
    air.dma_memcpy (%ext1, %buf0, [%c0], [%c0], %c256) : (memref<256xi32>, memref<256xi32, 2>, [index], [index], index) -> ()
    air.herd_terminator
  }
  return
}

}