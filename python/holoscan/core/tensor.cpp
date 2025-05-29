/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cuda_runtime.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "dl_converter.hpp"
#include "gxf/std/dlpack_utils.hpp"  // DLDeviceFromPointer, DLDataTypeFromTypeString
#include "holoscan/core/app_driver.hpp"
#include "holoscan/core/domain/tensor.hpp"
#include "holoscan/utils/cuda_macros.hpp"
#include "kwarg_handling.hpp"
#include "tensor.hpp"
#include "tensor_pydoc.hpp"

using pybind11::literals::operator""_a;  // NOLINT(misc-unused-using-decls)

namespace py = pybind11;

namespace holoscan {

void init_tensor(py::module_& m) {
  // DLPack data structures
  py::enum_<DLDeviceType>(m, "DLDeviceType", py::module_local())  //
      .value("DLCPU", kDLCPU)                                     //
      .value("DLCUDA", kDLCUDA)                                   //
      .value("DLCUDAHOST", kDLCUDAHost)                           //
      .value("DLCUDAMANAGED", kDLCUDAManaged);

  py::class_<DLDevice>(m, "DLDevice", py::module_local(), doc::DLDevice::doc_DLDevice)
      .def(py::init<DLDeviceType, int32_t>())
      .def_readonly("device_type", &DLDevice::device_type, doc::DLDevice::doc_device_type)
      .def_readonly("device_id", &DLDevice::device_id, doc::DLDevice::doc_device_id)
      .def(
          "__repr__",
          [](const DLDevice& device) {
            return fmt::format("<DLDevice device_type:{} device_id:{}>",
                               static_cast<int>(device.device_type),
                               device.device_id);
          },
          R"doc(Return repr(self).)doc");

  // Tensor Class
  py::class_<Tensor, std::shared_ptr<Tensor>>(
      m, "Tensor", py::dynamic_attr(), doc::Tensor::doc_Tensor)
      .def(py::init<>(), doc::Tensor::doc_Tensor)
      .def_property_readonly("ndim", &PyTensor::ndim, doc::Tensor::doc_ndim)
      .def_property_readonly(
          "shape",
          [](const Tensor& tensor) { return vector2pytuple<py::int_>(tensor.shape()); },
          doc::Tensor::doc_shape)
      .def_property_readonly(
          "strides",
          [](const Tensor& tensor) { return vector2pytuple<py::int_>(tensor.strides()); },
          doc::Tensor::doc_strides)
      .def_property_readonly("size", &PyTensor::size, doc::Tensor::doc_size)
      .def_property_readonly("dtype", &PyTensor::dtype, doc::Tensor::doc_dtype)
      .def_property_readonly("itemsize", &PyTensor::itemsize, doc::Tensor::doc_itemsize)
      .def_property_readonly("nbytes", &PyTensor::nbytes, doc::Tensor::doc_nbytes)
      .def_property_readonly(
          "data",
          [](const Tensor& t) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return static_cast<int64_t>(reinterpret_cast<uintptr_t>(t.data()));
          },
          doc::Tensor::doc_data)
      .def_property_readonly("device", &PyTensor::device, doc::Tensor::doc_device)
      .def("is_contiguous", &PyTensor::is_contiguous, doc::Tensor::doc_is_contiguous)
      // DLPack protocol
      .def(
          "__dlpack__",
          [](const py::object& obj,
             py::object stream,
             py::object max_version,
             py::object dl_device,
             py::object copy) {
            // Convert Python objects to C++ types
            std::optional<std::tuple<int, int>> cpp_max_version;
            std::optional<std::tuple<DLDeviceType, int>> cpp_dl_device;
            std::optional<bool> cpp_copy;

            // Handle max_version
            if (!max_version.is_none()) {
              if (!py::isinstance<py::tuple>(max_version)) {
                throw py::value_error("max_version must be a tuple of (major, minor)");
              }
              try {
                py::tuple version = max_version.cast<py::tuple>();
                if (version.size() == 2) {
                  int major = version[0].cast<int>();
                  int minor = version[1].cast<int>();
                  cpp_max_version = std::make_tuple(major, minor);
                } else {
                  throw py::value_error("max_version must be a tuple of (major, minor)");
                }
              } catch (const py::cast_error& e) {
                throw py::value_error("max_version must be a tuple of (major, minor)");
              }
            }

            // Handle dl_device
            if (!dl_device.is_none()) {
              if (!py::isinstance<py::tuple>(dl_device)) {
                throw py::value_error("dl_device must be a tuple of (DLDeviceType, device_id)");
              }

              try {
                py::tuple device = dl_device.cast<py::tuple>();
                if (device.size() == 2) {
                  DLDeviceType device_type = device[0].cast<DLDeviceType>();
                  int device_id = device[1].cast<int>();
                  cpp_dl_device = std::make_tuple(device_type, device_id);
                } else {
                  throw py::value_error("dl_device must be a tuple of (DLDeviceType, device_id)");
                }
              } catch (const py::cast_error& e) {
                throw py::value_error("dl_device must be a tuple of (DLDeviceType, device_id)");
              }
            }

            // Handle copy
            if (!copy.is_none()) {
              try {
                cpp_copy = copy.cast<bool>();
              } catch (const py::cast_error& e) { throw py::value_error("copy must be a boolean"); }
            }

            // Call the C++ function with the converted parameters
            return PyTensor::dlpack(
                obj, std::move(stream), cpp_max_version, cpp_dl_device, cpp_copy);
          },
          py::kw_only(),
          "stream"_a = py::none(),
          "max_version"_a = py::none(),
          "dl_device"_a = py::none(),
          "copy"_a = py::none(),
          doc::Tensor::doc_dlpack)
      .def("__dlpack_device__", &PyTensor::dlpack_device, doc::Tensor::doc_dlpack_device);

  py::class_<PyTensor, Tensor, std::shared_ptr<PyTensor>>(m, "PyTensor", doc::Tensor::doc_Tensor)
      .def_static("as_tensor", &PyTensor::as_tensor, "obj"_a, doc::Tensor::doc_as_tensor)
      .def_static("from_dlpack",
                  &PyTensor::from_dlpack_pyobj,
                  "obj"_a,
                  "device"_a = py::none(),
                  "copy"_a = py::none(),
                  doc::Tensor::doc_from_dlpack);

  py::enum_<DLDataTypeCode>(m, "DLDataTypeCode", py::module_local())
      .value("DLINT", kDLInt)
      .value("DLUINT", kDLUInt)
      .value("DLFLOAT", kDLFloat)
      .value("DLOPAQUEHANDLE", kDLOpaqueHandle)
      .value("DLBFLOAT", kDLBfloat)
      .value("DLCOMPLEX", kDLComplex);

  py::class_<DLDataType, std::shared_ptr<DLDataType>>(m, "DLDataType", py::module_local())
      .def_readwrite("code", &DLDataType::code)
      .def_readwrite("bits", &DLDataType::bits)
      .def_readwrite("lanes", &DLDataType::lanes)
      .def(
          "__repr__",
          [](const DLDataType& dtype) {
            return fmt::format("<DLDataType: code={}, bits={}, lanes={}>",
                               dldatatypecode_namemap.at(static_cast<DLDataTypeCode>(dtype.code)),
                               dtype.bits,
                               dtype.lanes);
          },
          R"doc(Return repr(self).)doc");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PyDLManagedMemoryBuffer definition
////////////////////////////////////////////////////////////////////////////////////////////////////

PyDLManagedMemoryBuffer::PyDLManagedMemoryBuffer(DLManagedTensor* self) : self_(self) {}

PyDLManagedMemoryBuffer::~PyDLManagedMemoryBuffer() {
  // Add the DLManagedTensor pointer to the queue for asynchronous deletion.
  // Without this, the deleter function will be called immediately, which can cause deadlock
  // when the deleter function is called from another non-python thread with GXF runtime mutex
  // acquired (issue 4293741).
  LazyDLManagedTensorDeleter::add(self_);
}

PyDLManagedMemoryBufferVersioned::PyDLManagedMemoryBufferVersioned(DLManagedTensorVersioned* self)
    : self_(self) {}

PyDLManagedMemoryBufferVersioned::~PyDLManagedMemoryBufferVersioned() {
  // Add the DLManagedTensorVersioned pointer to the queue for asynchronous deletion.
  // Without this, the deleter function will be called immediately, which can cause deadlock
  // when the deleter function is called from another non-python thread with GXF runtime mutex
  // acquired (issue 4293741).
  LazyDLManagedTensorDeleter::add(self_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// LazyDLManagedTensorDeleter
////////////////////////////////////////////////////////////////////////////////////////////////////

LazyDLManagedTensorDeleter::LazyDLManagedTensorDeleter() {
  // Use std::memory_order_relaxed because there are no other memory operations that need to be
  // synchronized with the fetch_add operation.
  if (s_instance_count.fetch_add(1, std::memory_order_relaxed) == 0) {
    // Wait until both s_stop and s_is_running are false (busy-waiting).
    // s_stop being true indicates that the previous deleter thread is still in the process
    // of deleting the object.
    while (true) {
      {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_stop && !s_is_running) { break; }
      }
      // Yield to other threads
      std::this_thread::yield();
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    // Register pthread_atfork() and std::atexit() handlers (registered only once)
    //
    // Note: Issue 4318040
    // When fork() is called in a multi-threaded program, the child process will only have
    // the thread that called fork().
    // Other threads from the parent process won't be running in the child.
    // This can lead to deadlocks if a condition variable or mutex was being waited upon by another
    // thread at the time of the fork.
    // To avoid this, we register pthread_atfork() handlers to acquire all necessary locks in
    // the pre-fork handler and release them in both post-fork handlers, ensuring no mutex or
    // condition variable remains locked in the child.
    if (!s_pthread_atfork_registered) {
      pthread_atfork(on_fork_prepare, on_fork_parent, on_fork_child);
      s_pthread_atfork_registered = true;
      // Register on_exit() to be called when the application exits.
      // Note that the child process will not call on_exit() when fork() is called and exit() is
      // called in the child process.
      if (std::atexit(on_exit) != 0) {
        HOLOSCAN_LOG_ERROR("Failed to register exit handler for LazyDLManagedTensorDeleter");
        // std::exit(EXIT_FAILURE);
      }
    }

    s_is_running = true;
    s_thread = std::thread(run);
    // Detach the thread so that it can be stopped when the application exits
    //
    // Note: Issue 4318040
    // According to the C++ Core Guidelines in CP.24 and CP.26
    // (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), std::detach() is generally
    // discouraged.
    // In C++ 20, std::jthread will be introduced to replace std::thread, and std::thread::detach()
    // will be deprecated.
    // However, std::jthread is not available in C++ 17 and we need to use std::thread::detach()
    // for now, with a synchronization mechanism to wait for the thread to finish itself,
    // instead of introducing a new dependency like https://github.com/martinmoene/jthread-lite.
    s_thread.detach();
  }
}

LazyDLManagedTensorDeleter::~LazyDLManagedTensorDeleter() {
  try {
    release();
  } catch (const std::exception& e) {}  // ignore potential fmt::v8::format_error
}

void LazyDLManagedTensorDeleter::add(DLManagedTensor* dl_managed_tensor_ptr) {
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_dlmanaged_tensors_queue.push(dl_managed_tensor_ptr);
  }
  s_cv.notify_all();
}

void LazyDLManagedTensorDeleter::add(DLManagedTensorVersioned* dl_managed_tensor_ver_ptr) {
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_dlmanaged_tensors_queue.push(dl_managed_tensor_ver_ptr);
  }
  s_cv.notify_all();
}

void LazyDLManagedTensorDeleter::run() {
  while (true) {
    std::unique_lock<std::mutex> lock(s_mutex);

    s_cv.wait(lock, [] {
      return s_stop || !s_dlmanaged_tensors_queue.empty() || s_cv_do_not_wait_thread;
    });

    // Check if the thread should stop. If queue is not empty, process the queue.
    if (s_stop && s_dlmanaged_tensors_queue.empty()) { break; }

    // Check if the condition variable should not wait for the thread so that fork() can be called
    // without deadlock.
    if (s_cv_do_not_wait_thread) { continue; }

    // move queue onto the local stack before releasing the lock
    std::queue<TensorPtr> local_queue;
    local_queue.swap(s_dlmanaged_tensors_queue);

    lock.unlock();
    // Call the deleter function for each pointer in the queue
    while (!local_queue.empty()) {
      auto tensor_ptr = local_queue.front();

      // Use a visitor to handle both types of pointers
      std::visit(
          [](auto* ptr) {
            // Note: the deleter function can be nullptr (e.g. when the tensor is created from
            // __cuda_array_interface__ protocol)
            if (ptr != nullptr && ptr->deleter != nullptr) {
              // Call the deleter function with GIL acquired
              py::gil_scoped_acquire scope_guard;
              ptr->deleter(ptr);
            }
          },
          tensor_ptr);

      local_queue.pop();
    }
  }

  // Set the flag to indicate that the thread has stopped
  s_is_running = false;

  HOLOSCAN_LOG_DEBUG("LazyDLManagedTensorDeleter thread finished");
}

void LazyDLManagedTensorDeleter::release() {
  // Use std::memory_order_relaxed because there are no other memory operations that need to be
  // synchronized with the fetch_sub operation.
  if (s_instance_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
    {
      std::lock_guard<std::mutex> lock(s_mutex);
      s_stop = true;
    }
    s_cv.notify_all();
    HOLOSCAN_LOG_DEBUG("Waiting for LazyDLManagedTensorDeleter thread to stop");
    // Wait until the thread has stopped
    while (true) {
      {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_is_running) { break; }
      }
      // Yield to other threads
      std::this_thread::yield();
    }
    HOLOSCAN_LOG_DEBUG("LazyDLManagedTensorDeleter thread stopped");
    {
      std::lock_guard<std::mutex> lock(s_mutex);
      s_stop = false;
    }
  }
}

void LazyDLManagedTensorDeleter::on_exit() {
  HOLOSCAN_LOG_DEBUG("LazyDLManagedTensorDeleter::on_exit() called");
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_stop = true;
  }
  s_cv.notify_all();
}

void LazyDLManagedTensorDeleter::on_fork_prepare() {
  s_mutex.lock();
  LazyDLManagedTensorDeleter::s_cv_do_not_wait_thread = true;
  s_cv.notify_all();
}

void LazyDLManagedTensorDeleter::on_fork_parent() {
  s_mutex.unlock();
  LazyDLManagedTensorDeleter::s_cv_do_not_wait_thread = false;
}

void LazyDLManagedTensorDeleter::on_fork_child() {
  s_mutex.unlock();
  LazyDLManagedTensorDeleter::s_cv_do_not_wait_thread = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PyTensor definition
////////////////////////////////////////////////////////////////////////////////////////////////////

PyTensor::PyTensor(std::shared_ptr<DLManagedTensorContext>& ctx) : Tensor(ctx) {}

PyTensor::PyTensor(DLManagedTensor* dl_managed_tensor_ptr) {
  dl_ctx_ = std::make_shared<DLManagedTensorContext>();
  // Create PyDLManagedMemoryBuffer to hold the DLManagedTensor and acquire GIL before calling
  // the deleter function
  dl_ctx_->memory_ref = std::make_shared<PyDLManagedMemoryBuffer>(dl_managed_tensor_ptr);

  auto& dl_managed_tensor = dl_ctx_->tensor;
  dl_managed_tensor = *dl_managed_tensor_ptr;
}

PyTensor::PyTensor(DLManagedTensorVersioned* dl_managed_tensor_ver_ptr) {
  dl_ctx_ = std::make_shared<DLManagedTensorContext>();
  dl_ctx_->memory_ref =
      std::make_shared<PyDLManagedMemoryBufferVersioned>(dl_managed_tensor_ver_ptr);
  // DLManagedTensorContext uses unversioned tensor, so any version
  // information and flags from DLPack >= 1.0 are discarded.
  dl_ctx_->tensor.dl_tensor = dl_managed_tensor_ver_ptr->dl_tensor;
  dl_ctx_->tensor.manager_ctx = dl_managed_tensor_ver_ptr->manager_ctx;
  dl_ctx_->tensor.deleter = nullptr;
}

py::object PyTensor::as_tensor(const py::object& obj) {
  // This method could have been used as a constructor for the PyTensor class, but it was not
  // possible to get the py::object to be passed to the constructor. Instead, this method is used
  // to create a py::object from PyTensor object and set array interface on it.
  //
  //    // Note: this does not work, as the py::object is not passed to the constructor
  //    .def(py::init(&PyTensor::py_create), doc::Tensor::doc_Tensor);
  //
  //       include/pybind11/detail/init.h:86:19: error: static assertion failed: pybind11::init():
  //       init function must return a compatible pointer, holder, or value
  //       86 |     static_assert(!std::is_same<Class, Class>::value /* always false */,
  //
  //    // See https://github.com/pybind/pybind11/issues/2984 for more details
  std::shared_ptr<PyTensor> tensor;

  if (py::hasattr(obj, "__cuda_array_interface__")) {
    tensor = PyTensor::from_cuda_array_interface(obj);
  } else if (py::hasattr(obj, "__dlpack__") && py::hasattr(obj, "__dlpack_device__")) {
    // Note: not currently exposing device or copy kwargs to as_tensor
    tensor = PyTensor::from_dlpack(obj);
  } else if (py::hasattr(obj, "__array_interface__")) {
    tensor = PyTensor::from_array_interface(obj);
  } else {
    throw std::runtime_error("Unsupported Python object type");
  }
  py::object py_tensor = py::cast(tensor);

  // Set array interface attributes
  set_array_interface(py_tensor, tensor->dl_ctx());
  return py_tensor;
}

py::object PyTensor::from_dlpack_pyobj(const py::object& obj, const py::object& device,
                                       const py::object& copy) {
  std::shared_ptr<PyTensor> tensor;
  if (py::hasattr(obj, "__dlpack__") && py::hasattr(obj, "__dlpack_device__")) {
    tensor = PyTensor::from_dlpack(obj, device, copy);
  } else {
    throw std::runtime_error("Unsupported Python object type");
  }
  py::object py_tensor = py::cast(tensor);

  // Set array interface attributes
  set_array_interface(py_tensor, tensor->dl_ctx());
  return py_tensor;
}

DLTensor init_dl_tensor_from_interface(
    const std::shared_ptr<ArrayInterfaceMemoryBuffer>& memory_buf, const py::dict& array_interface,
    bool cuda) {
  // Process mandatory entries
  memory_buf->dl_shape = array_interface["shape"].cast<std::vector<int64_t>>();
  auto& shape = memory_buf->dl_shape;
  auto typestr = array_interface["typestr"].cast<std::string>();
  if (!cuda) {
    if (!array_interface.contains("data")) {
      throw std::runtime_error(
          "Array interface data entry is missing (buffer interface) which is not supported ");
    }
    auto data_obj = array_interface["data"];
    if (data_obj.is_none()) {
      throw std::runtime_error(
          "Array interface data entry is None (buffer interface) which is not supported");
    }
    if (!py::isinstance<py::tuple>(data_obj)) {
      throw std::runtime_error(
          "Array interface data entry is not a tuple (buffer interface) which is not supported");
    }
  }
  auto data_array = array_interface["data"].cast<std::vector<int64_t>>();
  // NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast)
  auto* data_ptr = reinterpret_cast<void*>(data_array[0]);
  // bool data_readonly = data_array[1] > 0;
  // auto version = array_interface["version"].cast<int64_t>();

  auto maybe_dldatatype = nvidia::gxf::DLDataTypeFromTypeString(typestr);
  if (!maybe_dldatatype) {
    throw std::runtime_error("Unable to determine DLDataType from NumPy typestr");
  }
  auto maybe_device = nvidia::gxf::DLDeviceFromPointer(data_ptr);
  if (!maybe_device) { throw std::runtime_error("Unable to determine DLDevice from data pointer"); }
  DLTensor local_dl_tensor{
      .data = data_ptr,
      .device = maybe_device.value(),
      .ndim = static_cast<int32_t>(shape.size()),
      .dtype = maybe_dldatatype.value(),
      .shape = shape.data(),
      .strides = nullptr,
      .byte_offset = 0,
  };

  // Process 'optional' entries
  py::object strides_obj = py::none();
  if (array_interface.contains("strides")) { strides_obj = array_interface["strides"]; }
  auto& strides = memory_buf->dl_strides;
  if (strides_obj.is_none()) {
    calc_strides(local_dl_tensor, strides, true);
  } else {
    strides = strides_obj.cast<std::vector<int64_t>>();
    // The array interface's stride is using bytes, not element size, so we need to divide it by
    // the element size.
    int64_t elem_size = local_dl_tensor.dtype.bits / 8;
    for (auto& stride : strides) { stride /= elem_size; }
  }
  local_dl_tensor.strides = strides.data();

  // We do not process 'descr', 'mask', and 'offset' entries
  return local_dl_tensor;
}

void process_array_interface_stream(const py::object& stream_obj) {
  int64_t stream_id = 1;  // legacy default stream
  cudaStream_t stream_ptr = nullptr;
  if (stream_obj.is_none()) {
    stream_id = -1;
  } else {
    stream_id = stream_obj.cast<int64_t>();
  }
  if (stream_id < -1) {
    throw std::runtime_error(
        "Invalid stream, valid stream should be  None (no synchronization), 1 (legacy default "
        "stream), 2 "
        "(per-thread defaultstream), or a positive integer (stream pointer)");
  }
  if (stream_id > 2) {
    // NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast)
    stream_ptr = reinterpret_cast<cudaStream_t>(stream_id);
  }

  // Wait for the current stream to finish before the provided stream starts consuming the memory.
  cudaStream_t curr_stream_ptr = nullptr;  // legacy stream
  if (stream_id >= 0 && curr_stream_ptr != stream_ptr) {
    synchronize_streams(curr_stream_ptr, stream_ptr);
  }
}

std::shared_ptr<PyTensor> PyTensor::from_array_interface(const py::object& obj, bool cuda) {
  auto memory_buf = std::make_shared<ArrayInterfaceMemoryBuffer>();
  memory_buf->obj_ref = obj;  // hold obj to prevent it from being garbage collected

  const char* interface_name = cuda ? "__cuda_array_interface__" : "__array_interface__";
  auto array_interface = obj.attr(interface_name).cast<py::dict>();

  DLTensor local_dl_tensor = init_dl_tensor_from_interface(memory_buf, array_interface, cuda);
  if (cuda) {
    // determine stream and synchronize it with the default stream if necessary
    py::object stream_obj = py::none();
    if (array_interface.contains("stream")) {
      stream_obj = array_interface["stream"];
      static bool sync_streams =
          AppDriver::get_bool_env_var("HOLOSCAN_CUDA_ARRAY_INTERFACE_SYNC", true);
      if (sync_streams) { process_array_interface_stream(stream_obj); }
    }
  }
  // Create DLManagedTensor object
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto* dl_managed_tensor_ctx = new DLManagedTensorContext;
  auto& dl_managed_tensor = dl_managed_tensor_ctx->tensor;

  dl_managed_tensor_ctx->memory_ref = memory_buf;

  dl_managed_tensor.manager_ctx = dl_managed_tensor_ctx;
  dl_managed_tensor.deleter = [](DLManagedTensor* self) {
    auto* dl_managed_tensor_ctx = static_cast<DLManagedTensorContext*>(self->manager_ctx);
    // Note: since 'memory_ref' is maintaining python object reference, we should acquire GIL in
    // case this function is called from another non-python thread, before releasing 'memory_ref'.
    py::gil_scoped_acquire scope_guard;
    dl_managed_tensor_ctx->memory_ref.reset();
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete dl_managed_tensor_ctx;
  };

  // Copy the DLTensor struct data
  DLTensor& dl_tensor = dl_managed_tensor.dl_tensor;
  dl_tensor = local_dl_tensor;

  // Create PyTensor
  std::shared_ptr<PyTensor> tensor = std::make_shared<PyTensor>(&dl_managed_tensor);

  return tensor;
}

std::shared_ptr<PyTensor> PyTensor::from_dlpack(const py::object& obj, const py::object& device,
                                                const py::object& copy) {
  // Pybind11 doesn't have a way to get/set a pointer with a name so we have to use the C API
  // for efficiency.
  // auto dlpack_capsule = py::reinterpret_borrow<py::capsule>(obj.attr("__dlpack__")());

  if (!device.is_none()) {
    // Might support the user picking a specific GPU in the future
    throw pybind11::value_error("from_dlpack() does not support non-default device kwarg yet.");
  }
  auto dlpack_device_func = obj.attr("__dlpack_device__");
  // We don't handle backward compatibility with older versions of DLPack
  if (dlpack_device_func.is_none()) { throw std::runtime_error("DLPack device is not set"); }

  auto dlpack_device = py::cast<py::tuple>(dlpack_device_func());
  // https://dmlc.github.io/dlpack/latest/c_api.html#_CPPv48DLDevice
  DLDeviceType device_type = static_cast<DLDeviceType>(dlpack_device[0].cast<int>());
  auto device_id = dlpack_device[1].cast<int32_t>();

  DLDevice dl_device = {device_type, device_id};

  auto dlpack_func = obj.attr("__dlpack__");
  py::capsule dlpack_capsule;

  // TOIMPROVE: need to get current stream pointer and call with the stream
  // https://github.com/dmlc/dlpack/issues/57 this thread was good to understand the differences
  // between __cuda_array_interface__ and __dlpack__ on life cycle/stream handling.
  // In DLPack, the client of the memory notify to the producer that the client will use the
  // client stream (`stream_ptr`) to consume the memory. It's the producer's responsibility to
  // make sure that the client stream wait for the producer stream to finish producing the memory.
  // The producer stream is the stream that the producer used to produce the memory. The producer
  // can then use this information to decide whether to use the same stream to produce the memory
  // or to use a different stream.
  // In __cuda_array_interface__, both producer and consumer are responsible for managing the
  // streams. The producer can use the `stream` field to specify the stream that the producer used
  // to produce the memory. The consumer can use the `stream` field to synchronize with the
  // producer stream. (please see
  // https://numba.readthedocs.io/en/latest/cuda/cuda_array_interface.html#synchronization)
  switch (device_type) {
    case kDLCUDA:
    case kDLCUDAManaged: {
      py::int_ stream_ptr(1);  // legacy stream
      try {
        // TODO: update to use user-provided copy kwarg
        dlpack_capsule = py::reinterpret_borrow<py::capsule>(
            dlpack_func("stream"_a = stream_ptr,
                        "max_version"_a = py::make_tuple(HOLOSCAN_DLPACK_IMPL_VERSION_MAJOR,
                                                         HOLOSCAN_DLPACK_IMPL_VERSION_MINOR),
                        "copy"_a = copy.is_none() ? copy : py::bool_(copy.cast<bool>())));
      } catch (const py::error_already_set& e) {
        // retry without v1.0 kwargs like max_version and copy if TypeError was raised
        if (e.matches(PyExc_TypeError)) {
          PyErr_Clear();  // Clear any residual error state
          if (!copy.is_none()) { throw pybind11::type_error("copy kwarg not supported"); }
          dlpack_capsule =
              py::reinterpret_borrow<py::capsule>(dlpack_func("stream"_a = stream_ptr));
        } else {
          throw;  // Re-throw other exceptions
        }
      }
      break;
    }
    case kDLCPU:
    case kDLCUDAHost: {
      try {
        // TODO: update to use user-provided copy kwarg
        dlpack_capsule = py::reinterpret_borrow<py::capsule>(
            dlpack_func("max_version"_a = py::make_tuple(HOLOSCAN_DLPACK_IMPL_VERSION_MAJOR,
                                                         HOLOSCAN_DLPACK_IMPL_VERSION_MINOR),
                        "copy"_a = copy.is_none() ? copy : py::bool_(copy.cast<bool>())));
      } catch (const py::error_already_set& e) {
        // retry without v1.0 kwargs like max_version and copy if TypeError was raised
        if (e.matches(PyExc_TypeError)) {
          PyErr_Clear();  // Clear any residual error state
          if (!copy.is_none()) { throw pybind11::type_error("copy kwarg not supported"); }
          dlpack_capsule = py::reinterpret_borrow<py::capsule>(dlpack_func());
        } else {
          throw;  // Re-throw other exceptions
        }
      }
      break;
    }
    default:
      throw std::runtime_error(
          fmt::format("Unsupported device type: {}", static_cast<int>(device_type)));
  }

  // Note: we should keep the reference to the capsule object (`dlpack_obj`) while working with
  // PyObject* pointer. Otherwise, the capsule can be deleted and the pointers will be invalid.
  py::object dlpack_obj = dlpack_func();

  PyObject* dlpack_capsule_ptr = dlpack_obj.ptr();

  // Check for both versioned and unversioned capsules
  bool is_versioned = PyCapsule_IsValid(dlpack_capsule_ptr, dlpack_versioned_capsule_name) != 0;
  const char* valid_name = is_versioned ? dlpack_versioned_capsule_name : dlpack_capsule_name;
  const char* used_name =
      is_versioned ? used_dlpack_versioned_capsule_name : used_dlpack_capsule_name;

  if (!is_versioned && PyCapsule_IsValid(dlpack_capsule_ptr, valid_name) == 0) {
    const char* capsule_name = PyCapsule_GetName(dlpack_capsule_ptr);
    throw std::runtime_error(
        fmt::format("Received an invalid DLPack capsule ('{}'). You might have already consumed "
                    "the DLPack capsule.",
                    capsule_name));
  }

  void* tensor_ptr = PyCapsule_GetPointer(dlpack_capsule_ptr, valid_name);
  std::shared_ptr<PyTensor> tensor;

  if (is_versioned) {
    auto* dl_managed_tensor_ver = static_cast<DLManagedTensorVersioned*>(tensor_ptr);
    // Check version compatibility
    if (dl_managed_tensor_ver->version.major > DLPACK_MAJOR_VERSION) {
      throw std::runtime_error("DLPack version not supported");
    }
    // Set device
    dl_managed_tensor_ver->dl_tensor.device = dl_device;
    tensor = std::make_shared<PyTensor>(dl_managed_tensor_ver);
  } else {
    auto* dl_managed_tensor = static_cast<DLManagedTensor*>(tensor_ptr);
    dl_managed_tensor->dl_tensor.device = dl_device;
    tensor = std::make_shared<PyTensor>(dl_managed_tensor);
  }

  // Mark capsule as used
  PyCapsule_SetName(dlpack_capsule_ptr, used_name);
  PyCapsule_SetDestructor(dlpack_capsule_ptr, nullptr);

  return tensor;
}

py::capsule PyTensor::dlpack(const py::object& obj, py::object stream,
                             std::optional<std::tuple<int, int>> max_version,
                             std::optional<std::tuple<DLDeviceType, int>> dl_device,
                             std::optional<bool> copy) {
  auto tensor = py::cast<std::shared_ptr<Tensor>>(obj);
  if (!tensor) { throw std::runtime_error("Failed to cast to Tensor"); }

  // Call the new py_dlpack function with separate parameters
  return py_dlpack(tensor.get(), std::move(stream), max_version, dl_device, copy);
}

py::tuple PyTensor::dlpack_device(const py::object& obj) {
  auto tensor = py::cast<std::shared_ptr<Tensor>>(obj);
  if (!tensor) { throw std::runtime_error("Failed to cast to Tensor"); }
  // Do not copy 'obj' or a shared pointer here in the lambda expression's initializer, otherwise
  // the refcount of it will be increased by 1 and prevent the object from being destructed. Use a
  // raw pointer here instead.
  return py_dlpack_device(tensor.get());
}

bool is_tensor_like(const py::object& value) {
  return ((py::hasattr(value, "__dlpack__") && py::hasattr(value, "__dlpack_device__")) ||
          py::isinstance<holoscan::PyTensor>(value) ||
          py::hasattr(value, "__cuda_array_interface__") ||
          py::hasattr(value, "__array_interface__"));
}

}  // namespace holoscan
