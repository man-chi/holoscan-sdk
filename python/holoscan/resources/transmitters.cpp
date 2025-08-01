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

#include <pybind11/pybind11.h>

#include <cstdint>
#include <memory>
#include <string>

#include "./transmitters_pydoc.hpp"
#include "holoscan/core/component_spec.hpp"
#include "holoscan/core/fragment.hpp"
#include "holoscan/core/gxf/gxf_resource.hpp"
#include "holoscan/core/resources/gxf/async_buffer_transmitter.hpp"
#include "holoscan/core/resources/gxf/double_buffer_transmitter.hpp"
#include "holoscan/core/resources/gxf/transmitter.hpp"
#include "holoscan/core/resources/gxf/ucx_transmitter.hpp"

using std::string_literals::operator""s;  // NOLINT(misc-unused-using-decls)
using pybind11::literals::operator""_a;   // NOLINT(misc-unused-using-decls)

namespace py = pybind11;

namespace holoscan {

class PyDoubleBufferTransmitter : public DoubleBufferTransmitter {
 public:
  /* Inherit the constructors */
  using DoubleBufferTransmitter::DoubleBufferTransmitter;

  // Define a constructor that fully initializes the object.
  explicit PyDoubleBufferTransmitter(Fragment* fragment, uint64_t capacity = 1UL,
                                     uint64_t policy = 2UL,
                                     const std::string& name = "double_buffer_transmitter")
      : DoubleBufferTransmitter(ArgList{Arg{"capacity", capacity}, Arg{"policy", policy}}) {
    name_ = name;
    fragment_ = fragment;
    spec_ = std::make_shared<ComponentSpec>(fragment);
    setup(*spec_);
  }
};

class PyUcxTransmitter : public UcxTransmitter {
 public:
  /* Inherit the constructors */
  using UcxTransmitter::UcxTransmitter;

  // Define a constructor that fully initializes the object.
  explicit PyUcxTransmitter(Fragment* fragment,
                            std::shared_ptr<UcxSerializationBuffer> buffer = nullptr,
                            uint64_t capacity = 1UL, uint64_t policy = 2UL,
                            const std::string& receiver_address = std::string("0.0.0.0"),
                            const std::string& local_address = std::string("0.0.0.0"),
                            uint32_t port = kDefaultUcxPort, uint32_t local_port = 0,
                            uint32_t maximum_connection_retries = 10,
                            const std::string& name = "ucx_transmitter")
      : UcxTransmitter(ArgList{Arg{"capacity", capacity},
                               Arg{"policy", policy},
                               Arg{"receiver_address", receiver_address},
                               Arg{"local_address", local_address},
                               Arg{"port", port},
                               Arg{"local_port", local_port},
                               Arg{"maximum_connection_retries", maximum_connection_retries}}) {
    if (buffer) {
      this->add_arg(Arg{"buffer", buffer});
    }
    name_ = name;
    fragment_ = fragment;
    spec_ = std::make_shared<ComponentSpec>(fragment);
    setup(*spec_);
  }
};

class PyAsyncBufferTransmitter : public AsyncBufferTransmitter {
 public:
  /* Inherit the constructors */
  using AsyncBufferTransmitter::AsyncBufferTransmitter;

  // Define a constructor that fully initializes the object.
  explicit PyAsyncBufferTransmitter(Fragment* fragment,
                                    const std::string& name = "async_buffer_transmitter")
      : AsyncBufferTransmitter() {
    name_ = name;
    fragment_ = fragment;
    spec_ = std::make_shared<ComponentSpec>(fragment);
    setup(*spec_);
  }
};

void init_transmitters(py::module_& m) {
  py::class_<Transmitter, gxf::GXFResource, std::shared_ptr<Transmitter>>(
      m, "Transmitter", doc::Transmitter::doc_Transmitter)
      .def(py::init<>(), doc::Transmitter::doc_Transmitter)
      .def_property_readonly("capacity", &Transmitter::capacity, doc::Transmitter::doc_capacity)
      .def_property_readonly("size", &Transmitter::size, doc::Transmitter::doc_size)
      .def_property_readonly("back_size", &Transmitter::back_size, doc::Transmitter::doc_back_size);

  py::class_<DoubleBufferTransmitter,
             PyDoubleBufferTransmitter,
             Transmitter,
             std::shared_ptr<DoubleBufferTransmitter>>(
      m, "DoubleBufferTransmitter", doc::DoubleBufferTransmitter::doc_DoubleBufferTransmitter)
      .def(py::init<Fragment*, uint64_t, uint64_t, const std::string&>(),
           "fragment"_a,
           "capacity"_a = 1UL,
           "policy"_a = 2UL,
           "name"_a = "double_buffer_transmitter"s,
           doc::DoubleBufferTransmitter::doc_DoubleBufferTransmitter);

  py::class_<AsyncBufferTransmitter,
             PyAsyncBufferTransmitter,
             Transmitter,
             std::shared_ptr<AsyncBufferTransmitter>>(
      m, "AsyncBufferTransmitter", doc::AsyncBufferTransmitter::doc_AsyncBufferTransmitter)
      .def(py::init<Fragment*, const std::string&>(),
           "fragment"_a,
           "name"_a = "async_buffer_transmitter"s,
           doc::AsyncBufferTransmitter::doc_AsyncBufferTransmitter);

  py::class_<UcxTransmitter, PyUcxTransmitter, Transmitter, std::shared_ptr<UcxTransmitter>>(
      m, "UcxTransmitter", doc::UcxTransmitter::doc_UcxTransmitter)
      .def(py::init<Fragment*,
                    std::shared_ptr<UcxSerializationBuffer>,
                    uint64_t,
                    uint64_t,
                    const std::string&,
                    const std::string&,
                    uint32_t,
                    uint32_t,
                    uint32_t,
                    const std::string&>(),
           "fragment"_a,
           "buffer"_a = nullptr,
           "capacity"_a = 1UL,
           "policy"_a = 2UL,
           "receiver_address"_a = std::string("0.0.0.0"),
           "local_address"_a = std::string("0.0.0.0"),
           "port"_a = kDefaultUcxPort,
           "local_port"_a = static_cast<uint32_t>(0),
           "maximum_connection_retries"_a = 10,
           "name"_a = "ucx_transmitter"s,
           doc::UcxTransmitter::doc_UcxTransmitter);
}
}  // namespace holoscan
