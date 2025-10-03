(clara_holoscan_setup)=

# SDK Installation

The section below refers to the installation of the Holoscan SDK referred to as the **development stack**, designed for NVIDIA Developer Kits (arm64), and for x86_64 Linux compute platforms, ideal for development and testing of the SDK.

:::{note}
For Holoscan Developer Kits such as the [IGX Orin Developer Kit](https://www.nvidia.com/en-us/edge-computing/products/igx/), an alternative option is the [deployment stack](./deployment_stack.md), based on [OpenEmbedded](https://www.openembedded.org/wiki/Main_Page) ([Yocto](https://www.yoctoproject.org/) build system) instead of Ubuntu. This is recommended to limit your stack to the software components strictly required to run your Holoscan application. The runtime Board Support Package (BSP) can be optimized with respect to memory usage, speed, security and power requirements.
:::

## Prerequisites

`````{tab-set}
````{tab-item} NVIDIA Developer Kits

Setup your developer kit:

Developer Kit | User Guide | OS | GPU Mode
------------- | ---------- | --- | ---
[NVIDIA Jetson AGX Thor][jetson-thor] | [Guide][jetson-thor-guide] | [Jetpack][jp] 7.0 | dGPU
[NVIDIA IGX Orin][igx] | [Guide][igx-guide] | [IGX Software][igx-sw] 1.1.1 Production Release | iGPU **or*** dGPU
[NVIDIA Jetson AGX Orin and Orin Nano][jetson-orin] | [Guide][jetson-guide] | [JetPack][jp] 6.2.1 | iGPU
[NVIDIA Clara AGX][clara-agx]<br>_Only supporting the NGC container_ | [Guide][clara-guide] | [HoloPack][sdkm] 1.2<br>_[Upgrade to 535+ drivers required][cagx-upgrade]_ | dGPU

[clara-agx]: https://www.nvidia.com/en-gb/clara/intelligent-medical-instruments
[clara-guide]: https://github.com/nvidia-holoscan/holoscan-docs/blob/main/devkits/clara-agx/clara_agx_user_guide.md
[cagx-upgrade]: https://github.com/nvidia-holoscan/holoscan-docs/blob/main/devkits/clara-agx/clara_agx_user_guide.md#update-nvidia-drivers
[sdkm]: https://developer.nvidia.com/drive/sdk-manager
[igx]: https://www.nvidia.com/en-us/edge-computing/products/igx/
[igx-guide]: https://developer.nvidia.com/igx-orin-developer-kit-user-guide
[igx-sw]: https://developer.nvidia.com/igx-downloads
[meta-tegra]: https://github.com/nvidia-holoscan/meta-tegra-holoscan
[jetson-orin]: https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/
[jetson-thor]: https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-thor/
[jetson-guide]: https://developer.nvidia.com/embedded/learn/jetson-agx-orin-devkit-user-guide/index.html
[jetson-thor-guide]: https://docs.nvidia.com/jetson/agx-thor-devkit/user-guide/latest/index.html
[jp]: https://developer.nvidia.com/embedded/jetpack

<sup>_* iGPU and dGPU can be used concurrently on a single developer kit in dGPU mode. See [details here](./use_igpu_with_dgpu.md)._</sup>

````
````{tab-item} NVIDIA SuperChips

This version of the Holoscan SDK was tested on the Grace-Hopper SuperChip (GH200) with Ubuntu 22.04. Follow setup instructions [here](https://docs.nvidia.com/grace-ubuntu-install-guide.pdf).

:::{attention}
Display is not supported on SBSA/superchips. You can however do headless rendering with [HoloViz](./visualization.md#holoviz-operator) for example.
:::

````
````{tab-item} x86_64 Workstations

Supported x86_64 distributions:

OS | NGC Container | Debian/RPM package | Python wheel | Conda package | Build from source
-- | ------------- | -------------- | ------------ | -----------------
**Ubuntu 22.04** | Yes | Yes | Yes | Yes | Yes
**Ubuntu 24.04** | Yes | Yes | Yes | Yes | Yes
**RHEL 9.x** | Yes | No | No | No | No¹
**Other Linux distros** | No² | No | No³ | No | No¹

¹ <sup>Not formally tested or supported, but expected to work if building bare metal with the adequate dependencies.</sup><br>
² <sup>Not formally tested or supported, but expected to work if [supported by the NVIDIA container-toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/supported-platforms.html).</sup><br>
³ <sup>Not formally tested or supported, but expected to work if the glibc version of the distribution is 2.35 or above.</sup><br>

NVIDIA discrete GPU (dGPU) requirements:
- Ampere or above recommended for best performance
- [Quadro/NVIDIA RTX](https://www.nvidia.com/en-gb/design-visualization/desktop-graphics/) necessary for GPUDirect RDMA support
* Tested with [NVIDIA RTX A6000](https://www.nvidia.com/en-us/design-visualization/rtx-a6000/) and [NVIDIA RTX ADA 6000](https://www.nvidia.com/en-us/products/workstations/rtx-6000/)
- [NVIDIA dGPU drivers](https://docs.nvidia.com/datacenter/tesla/tesla-installation-notes): 535 or above.
    * x86 workstations: Tested with [OpenRM drivers](https://github.com/NVIDIA/open-gpu-kernel-modules) >= R550.
    - *Several Holoscan SDK components support [CUDA Green Contexts](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__GREEN__CONTEXTS.html). Drivers 560 or above are required to use Green Context features.*

````
`````

- For RDMA Support, follow the instructions in the [Enabling RDMA](./set_up_gpudirect_rdma.md) section.
- Additional software dependencies might be needed based on how you choose to install the SDK (see section below).
- Refer to the [Additional Setup](./additional_setup.md) and [Third-Party Hardware Setup](./third_party_hw_setup.md) sections for additional prerequisites.

## Install the SDK

We provide multiple ways to install and run the Holoscan SDK:

### Instructions

`````{tab-set}
````{tab-item} NGC Container
- **CUDA 13** (x86_64, Jetson Thor)
   ```bash
   docker pull nvcr.io/nvidia/clara-holoscan/holoscan:v3.7.0-cuda13
   ```
- **CUDA 12 dGPU** (x86_64, IGX Orin dGPU, Clara AGX dGPU, GH200)
   ```bash
   docker pull nvcr.io/nvidia/clara-holoscan/holoscan:v3.7.0-cuda12-dgpu
   ```
- **CUDA 12 iGPU** (Jetson Orin, IGX Orin iGPU, Clara AGX iGPU)
   ```bash
   docker pull nvcr.io/nvidia/clara-holoscan/holoscan:v3.7.0-cuda12-igpu
   ```
See details and usage instructions on [NGC][container].
````
````{tab-item} Debian package

Try the following to install the holoscan SDK:
```sh
sudo apt update
```
- **CUDA 13**
   - x86_64, GB200
      ```bash
      sudo apt install holoscan-cuda-13
      ```
   - Jetson Thor
      ```bash
      sudo apt install holoscan
      ```
- **CUDA 12 **
   - x86_64, GH200
      ```bash
      sudo apt install holoscan-cuda-12
      ```
   - IGX Orin, Jetson Orin
      ```bash
      sudo apt install holoscan
      ```

:::{attention}
This will not install dependencies needed for the Torch nor ONNXRuntime inference backends. To do so, install transitive dependencies by adding the `--install-suggests` flag to `apt install holoscan`, and refer to the support matrix below for links to install libtorch and onnxruntime.
:::

#### Troubleshooting

**If `holoscan` is not found with apt:**

```text
E: Unable to locate package holoscan
```

Try the following before repeating the installation steps above:

- **IGX Orin**: Ensure the [compute stack is properly installed](https://docs.nvidia.com/igx-orin/user-guide/latest/base-os.html#installing-the-compute-stack) which should configure the L4T repository source. If you still cannot install the Holoscan SDK, use the [`arm64-sbsa` installer](https://developer.nvidia.com/holoscan-downloads?target_os=Linux&target_arch=arm64-sbsa&Compilation=Native&Distribution=Ubuntu&target_version=22.04&target_type=deb_network) from the CUDA repository.
- **Jetson**: Ensure [JetPack is properly installed](https://developer.nvidia.com/embedded/jetpack) which should configure the L4T repository source.  If you still cannot install the Holoscan SDK, use the [`aarch64-jetson` installer](https://developer.nvidia.com/holoscan-downloads?target_os=Linux&target_arch=aarch64-jetson&Compilation=Native&Distribution=Ubuntu&target_version=22.04&target_type=deb_network) from the CUDA repository.
- **GH200**: Use the [`arm64-sbsa` installer](https://developer.nvidia.com/holoscan-downloads?target_os=Linux&target_arch=arm64-sbsa&Compilation=Native&Distribution=Ubuntu&target_version=22.04&target_type=deb_network) from the CUDA repository.
- **x86_64**: Use the [`x86_64` installer](https://developer.nvidia.com/holoscan-downloads?target_os=Linux&target_arch=x86_64&Distribution=Ubuntu&target_version=22.04&target_type=deb_network) from the CUDA repository.

---

**If you get missing CUDA libraries at runtime like below:**

```text
ImportError: libcudart.so.12: cannot open shared object file: No such file or directory
```

This could happen if your system has multiple CUDA Toolkit component versions installed. Find the path of the missing CUDA library (`libcudart.so.12` here) using `find /usr/local/cuda* -name libcudart.so.12` and select that path in `sudo update-alternatives --config cuda`. If that library is not found, or other cuda toolkit libraries become missing afterwards, you could try a clean reinstall of the full CUDA Toolkit:

```bash
sudo apt update && sudo apt install -y cuda-toolkit-12-6
```

---

**If you get missing CUDA headers at compile time like below:**

```text
the link interface contains: CUDA::nppidei but the target was not found. [...] fatal error: npp.h: No such file or directory
```

Generally the same issue as above due from mixing CUDA Toolkit component versions in your environment. Confirm the path of the missing CUDA header (`npp.h` here) with `find /usr/local/cuda-* -name npp.h` and follow the same instructions as above.

---

**If you get missing TensorRT libraries at runtime like below:**

```
Error: libnvinfer.so.8: cannot open shared object file: No such file or directory
...
Error: libnvonnxparser.so.8: cannot open shared object file: No such file or directory
```

This could happen if your system has a different major version installed than version 8. Try to reinstall TensorRT 8 with:

```bash
sudo apt update && sudo apt install -y libnvinfer-bin="8.6.*"
```

---

**If you cannot import the holoscan Python module:**

```sh
ModuleNotFoundError: No module named 'holoscan'
```

Python support is removed from the Holoscan SDK Debian package as of v3.0.0. Please install the Holoscan Python wheel.

````
````{tab-item} Python wheel
**CUDA 13** (x86_64, Jetson Thor)
```bash
pip install holoscan-cu13
```
**CUDA 12** (x86_64, IGX Orin dGPU, Clara AGX dGPU, GH200, Jetson Orin, IGX Orin iGPU, Clara AGX iGPU)
```bash
pip install holoscan-cu12
```
See details and troubleshooting on [PyPI][pypi].

:::{note}
For x86_64, ensure that the [CUDA Toolkit is installed](https://developer.nvidia.com/cuda-12-6-3-download-archive?target_os=Linux&target_arch=x86_64&Distribution=Ubuntu&target_version=22.04&target_type=deb_network).
:::

````
````{tab-item} Conda package
```bash
conda install holoscan cuda-version=12.6 -c conda-forge
```
:::{note}
Conda package support is available only for CUDA 12.x environments at this time.
:::

See details and troubleshooting on [conda-forge][conda-forge].
````

`````

[container]: https://catalog.ngc.nvidia.com/orgs/nvidia/teams/clara-holoscan/containers/holoscan
[pypi]: https://pypi.org/project/holoscan
[conda-forge]: https://anaconda.org/conda-forge/holoscan

### Not sure what to choose?

- The [**Holoscan container image on NGC**][container] it the safest way to ensure all the dependencies are present with the expected versions (including Torch and ONNX Runtime), and should work on most Linux distributions. It is the simplest way to run the embedded examples, while still allowing you to create your own C++ and Python Holoscan application on top of it. These benefits come at a cost:
  - large image size from the numerous (some of them optional) dependencies. If you need a lean runtime image, see {ref}`section below<runtime-container>`.
  - standard inconvenience that exist when using Docker, such as more complex run instructions for proper configuration.
- If you are confident in your ability to manage dependencies on your own in your host environment, the **Holoscan Debian package** should provide all the capabilities needed to use the Holoscan SDK, assuming you are on Ubuntu 22.04 or Ubuntu 24.04.
- If you are not interested in the C++ API but just need to work in Python, you can use the [**Holoscan python wheels**][pypi] on PyPI. While they are the easiest solution to install the SDK, it might require the most work to setup your environment with extra dependencies based on your needs. Finally, they are only formally supported on Ubuntu 22.04 and Ubuntu 24.04, though should support other linux distributions with glibc 2.35 or above.
- If you are developing in both C++ and Python, the **Holoscan Conda package** should provide all capabilities needed to use the Holoscan SDK.

|  | NGC dev Container | Debian Package | Python Wheels |
|---|:---:|:---:|:---:|
| Runtime libraries | **Included** | **Included** | **Included** |
| Python module | 3.12 | N/A | **3.10 to 3.13** |
| C++ headers and<br>CMake config | **Included** | **Included** | N/A |
| Examples (+ source) | **Included** | **Included** | [retrieve from<br>GitHub][examples] |
| Sample datasets | **Included** | [retrieve from<br>NGC][data] | [retrieve from<br>NGC][data] |
| CUDA runtime [^1] | **Included** | automatically [^2]<br>installed | require manual<br>installation |
| [NPP][npp] support [^3] | **Included** | automatically [^2]<br>installed | require manual<br>installation |
| [TensorRT][trt] support [^4] | **Included** | automatically [^2]<br>installed | require manual<br>installation |
| [Vulkan][vulkan] support [^5] | **Included** | automatically [^2]<br>installed | require manual<br>installation |
| [V4L2][v4l2] support [^6] | **Included** | automatically [^2]<br>installed | require manual<br>installation |
| [Torch][torch] support [^7] | **Included** | require manual [^8]<br>installation | require manual [^8]<br>installation |
| [ONNX Runtime][ort] support [^9] | **Included** | require manual [^10]<br>installation | require manual [^10]<br>installation |
| [ConnectX][connectx] support [^11] | **User space included** <br>Install kernel drivers on the host | require manual <br>installation | require manual <br>installation |
| [CLI] support | [require manual installation](./holoscan_packager.md#cli-installation) | [require manual installation](./holoscan_packager.md#cli-installation) | [require manual installation](./holoscan_packager.md#cli-installation) |

[examples]: https://github.com/nvidia-holoscan/holoscan-sdk/blob/main/examples#readme
[data]: https://catalog.ngc.nvidia.com/orgs/nvidia/teams/clara-holoscan/collections/clara_holoscan
[npp]: https://developer.nvidia.com/npp
[trt]: https://developer.nvidia.com/tensorrt
[vulkan]: https://developer.nvidia.com/vulkan
[v4l2]: https://en.wikipedia.org/wiki/Video4Linux
[torch]: https://pytorch.org/
[ort]: https://onnxruntime.ai/
[connectx]: https://www.nvidia.com/en-us/networking/ethernet-adapters/
[cli]: ./holoscan_packager.md
[^1]: [CUDA 12](https://docs.nvidia.com/cuda/archive/12.6.3/cuda-installation-guide-linux/index.html) is required. Already installed on NVIDIA developer kits with IGX Software and JetPack.
[^2]: Debian installation on x86_64 requires the [latest cuda-keyring package](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/#network-repo-installation-for-ubuntu) to automatically install all dependencies.
[^3]: NPP 12 needed for the FormatConverter and BayerDemosaic operators. Already installed on NVIDIA developer kits with IGX Software and JetPack.
[^4]: TensorRT 10.3+ needed for the Inference operator. Already installed on NVIDIA developer kits with IGX Software and JetPack.
[^5]: Vulkan 1.3.204+ loader needed for the HoloViz operator (+ libegl1 for headless rendering). Already installed on NVIDIA developer kits with IGX Software and JetPack.
[^6]: V4L2 1.22+ needed for the V4L2 operator. Already installed on NVIDIA developer kits with IGX Software and JetPack.  V4L2 also requires libjpeg.
[^7]: Torch support tested with LibTorch 2.8.0, OpenBLAS 0.3.20+ (aarch64 iGPU only), NVIDIA Performance Libraries (aarch64 dGPU only).
[^8]: To install LibTorch on baremetal, either build it from source, from the python wheel, or extract it from the holoscan container (in `/opt/libtorch/`). See instructions in the [Inference](./inference.md) section.
[^9]: Tested with ONNXRuntime 1.22.0. Note that ONNX models are also supported through the TensorRT backend of the Inference Operator.
[^10]: To install ONNXRuntime on baremetal, either build it from source, download our [pre-built package](https://edge.urm.nvidia.com/artifactory/sw-holoscan-thirdparty-generic-local/onnxruntime/) with CUDA 12 and TensorRT execution provider support, or extract it from the holoscan container (in `/opt/onnxruntime/`).
[^11]: Tested with DOCA 3.0.0.

### Need more control over the SDK?

The [Holoscan SDK source repository](https://github.com/nvidia-holoscan/holoscan-sdk) is **open-source** and provides reference implementations, as well as infrastructure, for building the SDK yourself.

:::{attention}
We only recommend building the SDK from source if you need to build it with debug symbols or other options not used as part of the published packages. If you want to write your own operator or application, you can use the SDK as a dependency (and contribute to [HoloHub](https://github.com/nvidia-holoscan/holohub)). If you need to make other modifications to the SDK, [file a feature or bug request](https://forums.developer.nvidia.com/c/healthcare/holoscan-sdk/320/all).
:::
