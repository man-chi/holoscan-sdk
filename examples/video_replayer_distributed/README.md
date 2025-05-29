# Distributed Video Replayer

Minimal example to demonstrate the use of the video stream replayer operator to load video from disk in a distributed manner.

The video frames need to have been converted to a gxf entity format to use as input. You can use the `convert_video_to_gxf_entities.py` script installed in `/opt/nvidia/holoscan/bin` or available [on GitHub](https://github.com/nvidia-holoscan/holoscan-sdk/tree/main/scripts#convert_video_to_gxf_entitiespy) (tensors will be loaded on the GPU).

By default there are two fragments, each of which has only a single operator:

- ReplayerFragment : uses VideoReplayerOp to read video frames
- VisualizerFragment : receives video frames and displays them

Playback continues (looping the short video) until the user closes the display window (or presses Esc to close it). Alternatively, a finite frame `count` can be set in the `replayer` section of `video_replayer_distributed.yaml`.

If the user sets `dual_window: true` in `video_replayer_distributed.yaml`, a three-fragment version of the app will be launched. In this version there are two visualizer fragments. In the C++ version, the second visualizer fragment uses a modified version of `HolovizOp`:

- ReplayerFragment : uses VideoReplayerOp to read video frames
- VisualizerFragment : receives video frames and displays them
- VisualizerFragment2 : receives video frames and displays them (In the C++ version, this uses a modified `HolovizOp` with its compute method overridden so that the window is automatically closed after 30 frames are displayed)

This dual window C++ variant is used to test the shutdown behavior of distributed applications (there
should be no errors logged on shutdown). Clean shutdown via a leaf node (`VisualizerFragment2` in this case) is achieved by calling `Application::initiate_distributed_app_shutdown()` from within the [HolovizOp::compute](https://github.com/nvidia-holoscan/holoscan-sdk/blob/v3.2.0/src/operators/holoviz/holoviz.cpp) method. This will tell the application driver to initiate an orderly shutdown from root to leaf nodes, preventing error messages from being logged during the shutdown process. Prior to the introduction of this method in Holoscan v3.2, the leaf fragment would begin shutdown without informing the root node first, resulting in some residual errors being logged (e.g. the `ReplayerFragment` would try to send a video frame which could not be delivered since the leaf fragment had already shut down).

> Note: Support for H264 stream support is in progress and can be found on [HoloHub](https://github.com/nvidia-holoscan/holohub)

*Visit the [SDK User Guide](https://docs.nvidia.com/holoscan/sdk-user-guide/holoscan_create_distributed_app.html) to learn more about distributed applications.*

## Data

The following dataset is used by this example:
[📦️ (NGC) Sample RacerX Video Data](https://catalog.ngc.nvidia.com/orgs/nvidia/teams/clara-holoscan/resources/
holoscan_racerx_video/files?version=20231009).

## C++ Run instructions

Please refer to the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/examples/video_replayer_distributed.html#running-the-application) for instructions on how to run the application in a distributed manner.

### Prerequisites

* **using deb package install**:
  ```bash
  sudo /opt/nvidia/holoscan/examples/download_example_data
  export HOLOSCAN_INPUT_PATH=/opt/nvidia/holoscan/data

  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/video_replayer_distributed/cpp
  ```

* **from NGC container**:
  ```bash
  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/video_replayer_distributed/cpp
  ```
* **source (dev container)**:
  ```bash
  ./run launch # optional: append `install` for install tree (default: `build`)

  # Set the application folder
  APP_DIR=./examples/video_replayer_distributed/cpp
  ```
* **source (local env)**:
  ```bash
  # Set the application folder
  APP_DIR=${BUILD_OR_INSTALL_DIR}/examples/video_replayer_distributed/cpp
  ```

### Run the application

```bash
# 1. The following commands will start a driver and one worker (`fragment1` that reads video files)
#    in one machine (e.g. IP address `10.2.34.56`) using the port number `10000`,
#    and another worker (`fragment2` that renders video to display) in another machine.
#    If `--fragments` is not specified, any fragment in the application will be chosen to run.
# 1a. In the first machine (e.g. `10.2.34.56`):
${APP_DIR}/video_replayer_distributed --driver --worker --address 10.2.34.56:10000 --fragments fragment1
# 1b. In the second machine:
${APP_DIR}/video_replayer_distributed --worker --address 10.2.34.56:10000 --fragments fragment2

# 2. The following command will start the distributed app in a single process
${APP_DIR}/video_replayer_distributed
```

## Python Run instructions

Please refer to the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/examples/video_replayer_distributed.html#running-the-application) for instructions on how to run the application in a distributed manner.

### Prerequisites

* **using python wheel**:
  ```bash
  # [Prerequisite] Download NGC dataset above to `DATA_DIR`
  export HOLOSCAN_INPUT_PATH=<DATA_DIR>
  # [Prerequisite] Download example .py file below to `APP_DIR`
  # [Optional] Start the virtualenv where holoscan is installed

  # Set the application folder
  APP_DIR=<APP_DIR>
  ```
* **from NGC container**:
  ```bash
  # Set the application folder
  APP_DIR=/opt/nvidia/holoscan/examples/video_replayer_distributed/python
  ```
* **source (dev container)**:
  ```bash
  ./run launch # optional: append `install` for install tree (default: `build`)

  # Set the application folder
  APP_DIR=./examples/video_replayer_distributed/python
  ```
* **source (local env)**:
  ```bash
  export HOLOSCAN_INPUT_PATH=${SRC_DIR}/data
  export PYTHONPATH=${BUILD_OR_INSTALL_DIR}/python/lib

  # Set the application folder
  APP_DIR=${BUILD_OR_INSTALL_DIR}/examples/video_replayer_distributed/python
  ```

### Run the application

```bash
# 1. The following commands will start a driver and one worker (`fragment1` that reads video files)
#    in one machine (e.g. IP address `10.2.34.56`) using the port number `10000`,
#    and another worker (`fragment2` that renders video to display) in another machine.
#    If `--fragments` is not specified, any fragment in the application will be chosen to run.
# 1a. In the first machine (e.g. `10.2.34.56`):
python3 ${APP_DIR}/video_replayer_distributed.py --driver --worker --address 10.2.34.56:10000 --fragments fragment1
# 1b. In the second machine:
python3 ${APP_DIR}/video_replayer_distributed.py --worker --address 10.2.34.56:10000 --fragments fragment2

# 2. The following command will start the distributed app in a single process
python3 ${APP_DIR}/video_replayer_distributed.py
```

## Package Instructions

Follow the instructions below to package the Distributed Video Replayer application into a [HAP-compliant](https://docs.nvidia.com/holoscan/sdk-user-guide/cli/hap.html) container.

### Setup the Holoscan CLI

Refer to the documentation in the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/holoscan_packager.html).

### Run the packager

Example of packaging the application for x64 systems:

```bash
cd ./examples/video_replayer_distributed/cpp # for C++ version of Hello World
cd ./examples/video_replayer_distributed/python # or Python version

holoscan package -t video_replayer_distributed --platform x86_64  --config video_replayer_distributed.yaml --includes holoviz  .
```

### Run the containerized application

Refer to the documentation in the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/cli/cli/run.html).

```bash
# 1. The following commands will start a driver and one worker (`fragment1` that reads video files)
#    in one machine (e.g. IP address `10.2.34.56`) using the port number `10000`,
#    and another worker (`fragment2` that renders video to display) in another machine.
#    If `--fragments` is not specified, any fragment in the application will be chosen to run.
#    The `--nic <network-interface>` argument is required when running a distributed application
#    across multiple nodes; it instructs the application to use the specified network
#    interface for communicating with other application nodes.
#
#    note: use the following command to get a list of available network interface name and its assigned IP address.
ip -o -4 addr show | awk '{print $2, $4}'
#
# 1a. In the first machine (e.g. using network interface `eno1` with assigned IP address `10.2.34.56` ):
holoscan run -i ${SRC_DIR}/data --driver --worker --nic eno1 --address 10.2.34.56:10000 --fragments fragment1 video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image>
# 1b. In the second machine (e.g. to connect to the first machine at `10.2.34.56:10000` via network interface `eth0`.
#     `--render` is required for `fragment2` to render the video to the display):
holoscan run --render --worker --nic eth0 --address 10.2.34.56:10000 --fragments fragment2 video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image>


# 2. The following commands start a driver and two workers on the same machine in a
#    Docker network that is isolated from the host.
#
# 2a. The first command starts the application as a driver that listens on port 10000 and uses
#     --name argument to set the host name of the container to "driver" so
#     next command can reference it. This command also starts a worker to run "fragment1" that reads
#     the video files specified in the -i argument.
holoscan run -i ${SRC_DIR}/data --driver --worker --address :10000 --name driver --fragments fragment1 video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image>

# 2b. The second command starts another worker to run "fragment2" to render the video to the display
#     with the --render argument and connects to the driver via driver:10000.
holoscan run --render --worker --address driver:10000 --fragments fragment2 video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image>

# 3. The following commands start a driver in one terminal and a worker to run all fragments in
#    another terminal.
#
# 3a. The first command starts the driver that listens on port 10000 with the hostname set to "driver".
#
#     Note that even though the driver does not execute fragments, it still calls the `compose()` method
#     of the fragments to determine the number of connections between the fragments.
#     This is a current limitation of the Holoscan SDK and will be removed in the future.
holoscan run --driver --address :10000 --name driver video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image> # in one terminal

# 3b. The second command runs all fragments defined in the application via "--fragments all" argument
#     which is why the -i and the --render arguments are required.
holoscan run -i ${SRC_DIR}/data --render --worker --address driver:10000 --fragments all video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image> # in another terminal

# 4. The following command starts the distributed app in a single process
holoscan run -i ${SRC_DIR}/data --render video_replayer_distributed-x64-workstation-dgpu-linux-amd64:<version-of-image>
```

Please refer to the [user guide](https://docs.nvidia.com/holoscan/sdk-user-guide/cli/cli.html) for additional options for the Holoscan CLI.
