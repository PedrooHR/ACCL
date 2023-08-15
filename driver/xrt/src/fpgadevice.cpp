/*******************************************************************************
#  Copyright (C) 2022 Xilinx, Inc
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
*******************************************************************************/

#include "accl/fpgadevice.hpp"
#include "accl/common.hpp"

#include <cassert>

static void finish_fpga_request(ACCL::FPGARequest *req) {
  req->wait_kernel();
  ACCL::FPGADevice *cclo = reinterpret_cast<ACCL::FPGADevice *>(req->cclo());
  // get ret code before notifying waiting theads
  req->set_retcode(cclo->read(ACCL::RETCODE_OFFSET));
  req->set_status(ACCL::operationStatus::COMPLETED);
  req->notify();
  cclo->complete_request(req);
}

namespace ACCL {
void FPGARequest::start() {
  assert(this->get_status() ==  operationStatus::EXECUTING);

  int function, arg_id = 0;

  if (options.scenario == operation::config) {
    function = static_cast<int>(options.cfg_function);
  } else {
    function = static_cast<int>(options.reduce_function);
  }
  run.set_arg(arg_id++, static_cast<uint32_t>(options.scenario));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.count));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.comm));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.root_src_dst));
  run.set_arg(arg_id++, static_cast<uint32_t>(function));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.tag));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.arithcfg_addr));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.compression_flags));
  run.set_arg(arg_id++, static_cast<uint32_t>(options.stream_flags));
  run.set_arg(arg_id++, static_cast<uint64_t>(options.addr_0->physical_address()));
  run.set_arg(arg_id++, static_cast<uint64_t>(options.addr_1->physical_address()));
  run.set_arg(arg_id++, static_cast<uint64_t>(options.addr_2->physical_address()));
  
  auto f = std::async(std::launch::async, finish_fpga_request, this);

  run.start();  
}

ACCLRequest *FPGADevice::start(const Options &options) {
  ACCLRequest *request = new ACCLRequest;

  if (options.waitfor.size() != 0) {
    throw std::runtime_error("FPGADevice does not support chaining");
  }

  FPGARequest *fpga_handle =
      new FPGARequest(reinterpret_cast<void *>(this), hostctrl, options);

  *request = queue.push(fpga_handle);
  fpga_handle->set_status(operationStatus::QUEUED);

  request_map.emplace(std::make_pair(*request, fpga_handle));

  launch_request();

  return request;
}

FPGADevice::FPGADevice(xrt::ip &cclo_ip, xrt::kernel &hostctrl_ip)
    : cclo(cclo_ip), hostctrl(hostctrl_ip) {}

void FPGADevice::wait(ACCLRequest *request) { 
  auto fpga_handle = request_map.find(*request);
  if (fpga_handle != request_map.end())
    fpga_handle->second->wait(); 
}

timeoutStatus FPGADevice::wait(ACCLRequest *request,
                               std::chrono::milliseconds timeout) {
  auto fpga_handle = request_map.find(*request);

  if (fpga_handle == request_map.end() || fpga_handle->second->wait(timeout))
    return timeoutStatus::no_timeout;

  return timeoutStatus::timeout;
}

bool FPGADevice::test(ACCLRequest *request) {
  auto fpga_handle = request_map.find(*request);

  if (fpga_handle == request_map.end())
    return true;

  return fpga_handle->second->get_status() == operationStatus::COMPLETED;
}

void FPGADevice::free_request(ACCLRequest *request) {
  auto fpga_handle = request_map.find(*request);

  if (fpga_handle != request_map.end()) {
    delete fpga_handle->second;
    request_map.erase(fpga_handle);
  }
}

ACCLRequest *FPGADevice::call(const Options &options) {
  ACCLRequest *req = start(options);
  wait(req);

  // internal use only
  return req;
}

val_t FPGADevice::get_retcode(ACCLRequest *request) {
  auto fpga_handle = request_map.find(*request);

  if (fpga_handle != request_map.end())
    return 0;
  
  return fpga_handle->second->get_retcode();
}

val_t FPGADevice::read(addr_t offset) { return cclo.read_register(offset); }

void FPGADevice::write(addr_t offset, val_t val) {
  return cclo.write_register(offset, val);
}

void FPGADevice::launch_request() {
  // This guarantees permission to only one thread trying to start an operation
  if (queue.run()) {
    FPGARequest *req = queue.front();
    assert(req->get_status() == operationStatus::QUEUED);
    req->set_status(operationStatus::EXECUTING);
    req->start();
  }
}

void FPGADevice::complete_request(FPGARequest *request) {
  if (request->get_status() == operationStatus::COMPLETED) {
    queue.pop();
    launch_request();
  }
}
} // namespace ACCL
