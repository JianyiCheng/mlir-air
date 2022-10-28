//===- AIRHerdToJSON.cpp ---------------------------------------*- C++ -*-===//
//
// Copyright (C) 2019-2022, Xilinx Inc.
// Copyright (C) 2022, Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
//===----------------------------------------------------------------------===//
#include "air/Dialect/AIR/AIRDialect.h"
#include "air/Transform/AIRDependency.h"
#include "air/Util/Dependency.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Utils/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Transforms/Transforms.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Transforms/InliningUtils.h"
#include "mlir/Transforms/RegionUtils.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "air/Util/Util.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define DEBUG_TYPE "air-herds-to-json"

using namespace mlir;
using namespace xilinx;
using namespace xilinx::air;

namespace {

class Herd {

public:
  Herd(int32_t numRows, int32_t numCols, int32_t locX, int32_t locY,
       int32_t number, std::string name)
      : numRows(numRows), numCols(numCols), locX(locX), locY(locY),
        number(number), name(name) {
    convertHerdToList();
  }

  int32_t getNumRows() const { return numRows; }
  int32_t getNumCols() const { return numCols; }
  std::string getName() const { return name; }
  int32_t getNumber() const { return number; }
  int32_t getLocX() const { return locX; }
  int32_t getLocY() const { return locY; }

  void printHerd() const {
    llvm::outs() << "name: " << name << ", numRows: " << numRows
                 << ", numCols: " << numCols << ", x_loc: " << locX
                 << ", y_loc: " << locY << "\n";
    return;
  }

  std::string generateHerdString() {
    std::ostringstream herdString;
    herdString << "[" << number << ", "
               << "\"" << name << "\"";
    for (uint32_t i = 0; i < herdList.size(); i++) {
      herdString << ", [" << herdList[i][0] << ", " << herdList[i][1] << "]";
    }
    herdString << "]";
    return herdString.str();
  }

private:
  int32_t numRows;
  int32_t numCols;
  int32_t locX;
  int32_t locY;
  int32_t number;
  std::string name;
  std::vector<std::vector<int32_t>> herdList;

  void convertHerdToList() {
    for (int32_t i = 0; i < numRows; i++) {
      for (int32_t j = 0; j < numCols; j++) {
        int32_t rowCoord = locY - i;
        int32_t colCoord = locX + j;
        std::vector<int32_t> coords = {rowCoord, colCoord};
        herdList.push_back(coords);
      }
    }
    return;
  }
};

mlir::LogicalResult AIRHerdsToJSONTranslate(mlir::ModuleOp module,
                                            llvm::raw_ostream &outStream) {
  std::vector<std::unique_ptr<Herd>> herdOps;
  int32_t number = 0;
  auto status = success();
  for (auto f : module.getOps<func::FuncOp>()) {
    f.walk([&](Operation *op) {
      if (auto herd = dyn_cast<xilinx::air::HerdOp>(op)) {
        std::string name = "herd";
        if (auto attr = herd->getAttrOfType<StringAttr>(
                SymbolTable::getSymbolAttrName())) {
          name = attr.getValue().str();
        }
        SmallVector<Value, 2> herd_size = herd.getSizeOperands();
        if (!isa<arith::ConstantIndexOp>(herd_size[0].getDefiningOp()) ||
            !isa<arith::ConstantIndexOp>(herd_size[1].getDefiningOp())) {
          llvm::errs() << "Only constant sized herds are supported";
          status = failure();
        }
        int32_t herd_size_x =
            cast<arith::ConstantIndexOp>(herd_size[0].getDefiningOp()).value();
        int32_t herd_size_y =
            cast<arith::ConstantIndexOp>(herd_size[1].getDefiningOp()).value();

        int32_t x_loc = -1;
        int32_t y_loc = -1;

        if (auto attr = herd->getAttrOfType<IntegerAttr>("x_loc")) {
          x_loc = attr.getInt();
        }
        if (auto attr = herd->getAttrOfType<IntegerAttr>("y_loc")) {
          y_loc = attr.getInt();
        }

        if (x_loc < 0 || y_loc < 0) {
          llvm::errs() << "Invalid x or y location";
          status = failure();
        }
        auto herdPtr = std::make_unique<Herd>(herd_size_x, herd_size_y, x_loc,
                                              y_loc, number, name);
        herdOps.push_back(std::move(herdPtr));

        number++;
      }
    });
  }

  for (uint32_t i = 0; i < herdOps.size(); i++) {
    outStream << herdOps[i]->generateHerdString();
    if (i != herdOps.size() - 1) {
      outStream << ",\n\t\t\t\t   ";
    }
  }
  outStream << "\n\t]"
            << "\n}\n";
  return status;

}; // end class

} // end namespace

namespace xilinx {
namespace air {

mlir::LogicalResult AIRHerdsToJSON(ModuleOp module, raw_ostream &outStream) {
  return AIRHerdsToJSONTranslate(module, outStream);
}

} // namespace air
} // namespace xilinx