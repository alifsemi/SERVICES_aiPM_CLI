# Baremetal Test Application
This software is delivered for Alif Ensemble Devkits (Devkit-e7, Devkit-e8, and Devkit-e1c).

- The solution consists of following projects:
  - **app** is the core Baremetal test application delivered by this software package
  - **blinky** is a bare bone LED blinker
  - **hello** demonstrates retargeting printf() to UART
  - **hello_rtt** demonstrates retargeting printf() to SEGGER RTT

- Arm GNU toolchain is used as a default. There are build-type options for IAR and ARM compiler armclang for reference.
  - You can find the compiler specific settings in `cdefault.yaml`
  - **TIP:** The tools loaded by Arm Environment Manager are configured in `vcpkg-configuration.json`.
  - To download armclang you can add "arm:compilers/arm/armclang": "^6.22.0" to the "requires" object.

## Note about Ensemble silicon revisions
  - Devkit-e7: only silicon rev. B4 is supported
  - Devkit-e8: only silicon rev. A0 is supported
  - Devkit-e1c: only silicon rev. A5 or A6 is supported

## Quick start
First clone the template project repository
```
git clone https://github.com/alifsemi/SERVICES_aiPM_CLI.git
cd SERVICES_aiPM_CLI
git submodule update --init
```
OR
```
git clone --recursive https://github.com/alifsemi/SERVICES_aiPM_CLI.git
```

The required software setup consists of *VS Code*, *Git*, *CMake*, *Ninja build system*, *cmsis-toolbox*, *Arm GNU toolchain* and *Alif SE tools*. By default the template project uses J-link so *J-link software* is required for debugging. In addition to build tools the VS Code extensions and CMSIS packs will be downloaded automatically during the process.

To make environment setup easier this project uses *Arm Environment Manager* for downloading and configuring most of the tool dependencies.
Basically only VS Code, Alif SE tools and J-Link software need to be downloaded and installed manually.

**NOTE:** Please see the Readme_Arm_Environment_Manager.txt file first if you would like to manage where the tools are installed.

Opening the project folder with VS Code automatically suggests installing the extensions needed by this project:
- Arm Environment Manager
- Arm CMSIS csolution
- Cortex-Debug
- Microsoft C/C++ Extension Pack

After setting up the environment you can just click the CMSIS icon and then the *context* and *build* icon to get going.

For Alif SE tools and J-link debugging support add the following entries to VS Code user settings.json (Press F1 and start typing 'User')
```
{
    "alif.setools.root" : "C:/alif-se-tools/app-release-exec",
    "cortex-debug.JLinkGDBServerPath": "C:/Program Files/SEGGER/JLink/JLinkGDBServerCL.exe"
}
```

## More detailed getting started guide
Please refer to the [Getting started guide](doc/getting_started.md)

# Building the binaries
Open the SERVICES_aiPM_CLI directory using VS Code and switch to the CMSIS View (CTRL+SHIFT+ALT+S). Click the gear icon labeled "Manage Solution Settings". Here you will choose the Active Target. Use this Manage Solution tab to switch between Target Types and use the hammer icon in the CMSIS View to build the application.

# Programming the binaries
This program can run on both the RTSS-HE and RTSS-HP cores at the same time or one at a time. Use the built in tasks to program the binary to the device. This is done by following the "Programming the target with Alif Security Toolkit" step as described in the above "Getting started guide".
