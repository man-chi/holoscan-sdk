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

#ifndef HOLOSCAN_CORE_GXF_GXF_SCHEDULER_HPP
#define HOLOSCAN_CORE_GXF_GXF_SCHEDULER_HPP

#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>
#include <utility>

#include "../resources/gxf/clock.hpp"
#include "../scheduler.hpp"
#include "./gxf_component.hpp"
#include "gxf/std/clock.hpp"

namespace holoscan::gxf {

// note: in GXF there is also a System class that inherits from Component
//       and is the parent of Scheduler
class GXFScheduler : public holoscan::Scheduler, public GXFComponent {
 public:
  HOLOSCAN_SCHEDULER_FORWARD_ARGS_SUPER(GXFScheduler, holoscan::Scheduler)
  GXFScheduler() = default;

  /**
   * @brief Get the type name of the GXF scheduler.
   *
   * The returned string is the type name of the GXF scheduler and is used to
   * create the GXF scheduler.
   *
   * Example: "nvidia::holoscan::GreedyScheduler"
   *
   * @return The type name of the GXF scheduler.
   */
  virtual const char* gxf_typename() const = 0;

  /**
   * @brief Get the GXF Clock pointer.
   *
   * @return The GXF clock pointer used by the scheduler.
   */
  virtual nvidia::gxf::Clock* gxf_clock();

  /**
   * @brief Get a YAML representation of the scheduler.
   *
   * @return YAML node including type, specs, and resources of the scheduler in addition
   * to the base component properties.
   */
  YAML::Node to_yaml_node() const override;

  /// Reset any backend-specific objects
  void reset_backend_objects() override;

  /// Set the parameters based on defaults (sets GXF parameters for GXF operators)
  void set_parameters() override;

 private:
  // raw pointer to the nvidia::gxf::Clock instance used by the scheduler
  virtual void* clock_gxf_cptr() const = 0;
};

}  // namespace holoscan::gxf

#endif /* HOLOSCAN_CORE_GXF_GXF_SCHEDULER_HPP */
