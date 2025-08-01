/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <gtest/gtest.h>
#include <stdlib.h>

#include <tuple>
#include <vector>

#include <holoviz/holoviz.hpp>
#include "test_fixture.hpp"

namespace viz = holoscan::viz;

class SurfaceFormat : public TestWindow {};

TEST_F(SurfaceFormat, Set) {
  uint32_t surface_format_count = 0;
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, nullptr));
  EXPECT_GT(surface_format_count, 0);
  std::vector<viz::SurfaceFormat> surface_formats(surface_format_count);
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, surface_formats.data()));
  EXPECT_GE(surface_format_count, surface_formats.size());

  EXPECT_NO_THROW(viz::SetSurfaceFormat(surface_formats[0]));
}

TEST_F(SurfaceFormat, Get) {
  uint32_t surface_format_count = 0;
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, nullptr));
  EXPECT_GT(surface_format_count, 0);
  std::vector<viz::SurfaceFormat> surface_formats(surface_format_count);
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, surface_formats.data()));
  EXPECT_GE(surface_format_count, surface_formats.size());

  // B8G8R8A8_UNORM is always supported
  bool found_b8g8r8a8 = false;
  for (auto&& surface_format : surface_formats) {
    if (surface_format.image_format_ == viz::ImageFormat::B8G8R8A8_UNORM) {
      found_b8g8r8a8 = true;
      break;
    }
  }
  EXPECT_TRUE(found_b8g8r8a8);
}

TEST_F(SurfaceFormat, Errors) {
  // it's an error to call GetSurfaceFormats with an invalid surface_format_count
  EXPECT_THROW(viz::GetSurfaceFormats(nullptr, nullptr), std::invalid_argument);

  // it's an error to call GetSurfaceFormats with a surface_format_count != 0 and an invalid
  // surface_formats
  uint32_t surface_format_count = 1;
  EXPECT_THROW(viz::GetSurfaceFormats(&surface_format_count, nullptr), std::invalid_argument);
}

TEST(SurfaceFormatNoInit, Errors) {
  // it's an error to call GetSurfaceFormats without calling `viz::Init()` before
  uint32_t surface_format_count = 0;
  EXPECT_THROW(viz::GetSurfaceFormats(&surface_format_count, nullptr), std::runtime_error);
}

class SurfaceFormatBeforeInit : public TestWindow {
  void SetUp() override {
    EXPECT_NO_THROW(
        viz::SetSurfaceFormat({viz::ImageFormat::B8G8R8A8_SRGB, viz::ColorSpace::SRGB_NONLINEAR}));
    // call base class
    TestWindow::SetUp();
  }
};

TEST_F(SurfaceFormatBeforeInit, Pass) {}

class SurfaceFormatColorSpace : public TestWindow,
                                public testing::WithParamInterface<std::tuple<viz::ColorSpace>> {};

TEST_P(SurfaceFormatColorSpace, ColorSpace) {
  const viz::ColorSpace color_space = std::get<0>(GetParam());

  uint32_t surface_format_count = 0;
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, nullptr));
  std::vector<viz::SurfaceFormat> surface_formats(surface_format_count);
  EXPECT_NO_THROW(viz::GetSurfaceFormats(&surface_format_count, surface_formats.data()));

  bool found = false;
  for (auto&& surface_format : surface_formats) {
    if (surface_format.color_space_ == color_space) {
      found = true;
      break;
    }
  }

  if (!found) {
    GTEST_SKIP() << "Color space " << int(color_space) << " not supported";
  }

  // draw something to make sure the Vulkan pipeline is executed
  EXPECT_NO_THROW(viz::Begin());
  EXPECT_NO_THROW(viz::BeginImageLayer());
  uint32_t data = 0xFFFFFFFF;
  EXPECT_NO_THROW(viz::ImageHost(1, 1, viz::ImageFormat::R8G8B8A8_UNORM, &data));
  EXPECT_NO_THROW(viz::EndLayer());
  EXPECT_NO_THROW(viz::End());

  // the image can't be validated since the color space conversion happens when the image
  // is displayed on the screen, would need to take a screenshot.
}

INSTANTIATE_TEST_SUITE_P(
    ColorSpace, SurfaceFormatColorSpace,
    testing::Values(viz::ColorSpace::SRGB_NONLINEAR, viz::ColorSpace::EXTENDED_SRGB_LINEAR,
                    viz::ColorSpace::BT2020_LINEAR, viz::ColorSpace::HDR10_ST2084,
                    viz::ColorSpace::PASS_THROUGH, viz::ColorSpace::BT709_LINEAR));
