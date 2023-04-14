//===- Resource.cpp ---------------------------------------------*- C++ -*-===//
//
// Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef AIR_UTIL_RUNNER_RESOURCE
#define AIR_UTIL_RUNNER_RESOURCE

#include "air/Util/Runner.h"

namespace xilinx {
namespace air {

// Resource node entry for resource model
class resource {

public:
  std::string name;
  resource *parent;
  bool isReserved;

  void set_name(llvm::json::Object *nameObject) {
    if (nameObject)
      this->set_name(nameObject->getString("devicename").value().str());
    else
      this->set_name("");
  }

  void set_name(std::string name) { this->name = name; }

  void set_parent(resource *parent) { this->parent = parent; }

  void reset_reservation() { this->isReserved = false; }

  resource(std::string name = "", resource *parent = nullptr)
      : name(name), parent(parent) {
    this->reset_reservation();
  }

  ~resource() {}

private:
};

class port : public resource {
public:
  double data_rate;
  std::map<std::string, port> connected_ports;

  port() {}

  port(llvm::json::Object &json_template) {
    name = json_template.getString("type").value().str() + "_" +
           json_template.getString("idx").value().str();
    data_rate = json_template.getInteger("bytes_per_cycle").value();
    this->reset_reservation();
  }

  port(port *base) {
    name = base->name;
    data_rate = base->data_rate;
    this->reset_reservation();
    // TODO: Copy port Connections as well
    // May be challenging due to hierarchy --
    // need to copy while copying regions/tiles etc.
  }

  port(resource *parent, unsigned src, unsigned dst, double data_rate,
       unsigned idx) {
    this->set_name("L" + std::to_string(src) + "_to_" + "L" +
                   std::to_string(dst) + "_" + std::to_string(idx));
    this->set_data_rate(data_rate);
    this->set_parent(parent);
    this->reset_reservation();
  }

  port(resource *parent, std::string memory_space,
       std::optional<double> bytes_per_second, unsigned idx) {
    if (bytes_per_second) {
      this->set_name(memory_space + "_" + std::to_string(idx));
      this->set_data_rate(*bytes_per_second);
      this->set_parent(parent);
      this->reset_reservation();
    }
  }

  ~port() {}

  void add_connection(port *p) { connected_ports[p->name] = p; }

  void set_data_rate(long bytes_per_cycle) {
    this->data_rate = bytes_per_cycle;
  }

private:
}; // port

class streamPort : port { // Do Ports need Master/Slave assignment? Probably...
public:
private:
}; // streamPort

class MMPort : port {
public:
private:
}; // MMPort

class kernel : public resource {

public:
  double efficiency;
  int ops_per_core_per_cycle;
  // Key: datatype name; mapped: pair <efficiency, ops_per_core_per_cycle>
  std::map<std::string, std::pair<double, int>> datatypes;

  void push_to_datatypes(std::string datatype_name, std::optional<double> eff,
                         std::optional<int> vectorSize) {
    if (vectorSize && eff) {
      this->datatypes.insert(
          std::make_pair(datatype_name, std::make_pair(*eff, *vectorSize)));
    } else
      this->datatypes.insert(
          std::make_pair(datatype_name, std::make_pair(0, 0)));
  }

  kernel() {}

  kernel(resource *parent, llvm::json::Object *kernelObject) {
    this->name = kernelObject->getString("name").value().str();
    this->parent = parent;
    auto datatypeObjects = kernelObject->getObject("datatypes");

    for (auto it = datatypeObjects->begin(), ie = datatypeObjects->end();
         it != ie; ++it) {
      llvm::json::Object *datatypeObject = it->second.getAsObject();
      if (datatypeObject) {
        auto datatype_name = it->first.str();
        this->push_to_datatypes(
            datatype_name, datatypeObject->getNumber("efficiency"),
            datatypeObject->getInteger("ops_per_core_per_cycle"));
      }
      this->reset_reservation();
    }
    // this->num_fmt = kernelObject->getString("format").value().str();
    // this->total_ops = kernelObject->getInteger("ops").value();
    // for(llvm::json::Value rgn : *kernelObject->getArray("supported_regions"))
    // {
    //   llvm::json::Object rgn_obj = *rgn.getAsObject();
    //   this->efficiency[rgn_obj.getString("region").value().str()]
    //     = rgn_obj.getInteger("efficiency").value();
    // }
  }

  ~kernel() {}

private:
}; // kernel

class memory : public resource {

public:
  unsigned memory_space;
  double bytes;
  double bytes_used;

  void set_memory_space(unsigned ms) { this->memory_space = ms; }

  void set_bytes(double b) { this->bytes = b; }

  void reset_usage() { this->bytes_used = 0; }

  memory(unsigned ms, double b) {
    this->set_memory_space(ms);
    this->set_bytes(b);
    this->reset_reservation();
    this->reset_usage();
  }

  ~memory() {}

private:
}; // memory

} // namespace air
} // namespace xilinx

#endif // AIR_UTIL_RUNNER_RESOURCE