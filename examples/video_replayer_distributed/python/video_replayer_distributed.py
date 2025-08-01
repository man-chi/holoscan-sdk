"""
SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""  # noqa

import logging
import os

from holoscan.core import Application, Fragment
from holoscan.operators import HolovizOp, VideoStreamReplayerOp


class ReplayerFragment(Fragment):
    def __init__(self, app, name):
        super().__init__(app, name)

    def compose(self):
        # Set the video source
        video_path = self._get_input_path()
        logging.info(f"Using video from {video_path}")

        # Define the replayer and holoviz operators
        replayer = VideoStreamReplayerOp(
            self, name="replayer", directory=video_path, **self.kwargs("replayer")
        )

        self.add_operator(replayer)

    def _get_input_path(self):
        path = os.environ.get(
            "HOLOSCAN_INPUT_PATH", os.path.join(os.path.dirname(__file__), "data")
        )
        return os.path.join(path, "racerx")


class VisualizerFragment(Fragment):
    def compose(self):
        visualizer = HolovizOp(self, name="holoviz", **self.kwargs("holoviz"))

        self.add_operator(visualizer)


class DistributedVideoReplayerApp(Application):
    """Example of a distributed application that uses the fragments and operators defined above.

    This application has the following fragments:
    - ReplayerFragment
      - holding VideoStreamReplayerOp
    - VisualizerFragment
      - holding HolovizOp

    The VideoStreamReplayerOp reads a video file and sends the frames to the HolovizOp.
    The HolovizOp displays the frames.
    """

    def __init__(self):
        super().__init__()

    # def __init__(self, video_dir: Path):

    def compose(self):
        # Define the fragments
        fragment1 = ReplayerFragment(self, name="fragment1")
        fragment2 = VisualizerFragment(self, name="fragment2")

        # Define the workflow
        self.add_flow(fragment1, fragment2, {("replayer.output", "holoviz.receivers")})

        # Check if the YAML dual_window parameter is set and add a third fragment with a second
        # visualizer in that case.
        dual_window = self.kwargs("dual_window").get("dual_window", False)
        if dual_window:
            fragment3 = VisualizerFragment(self, name="fragment3")
            self.add_flow(fragment1, fragment3, {("replayer.output", "holoviz.receivers")})


def main():
    config_file_path = os.path.join(os.path.dirname(__file__), "video_replayer_distributed.yaml")

    logging.info(f"Reading application configuration from {config_file_path}")

    app = DistributedVideoReplayerApp()
    app.config(config_file_path)
    app.run()

    # If desired, input/output port mapping can be printed in human readable (YAML) format
    print("====== APPLICATION PORT MAPPING =======\n")
    print(app.fragment_graph.port_map_description())
    for fragment in app.fragment_graph.get_nodes():
        print(f"\n\n====== FRAGMENT '{fragment.name}' PORT MAPPING =======\n")
        print(fragment.graph.port_map_description())


if __name__ == "__main__":
    main()
