# Converged Sound Open FW - modules development

This repository contains three converged sound open firmware module examples
such as: aca_module, amplifier_module and downmixer_module and whole building
infrastructure (i.e. makefiles, source files and API headers) required to
build module examples and loadable libraries. Loadable library is composed by
a set of N precompiled loadable modules.
## Modules

Module implementation examples are provided in ``<repo_dir>/modules/``
directory.

### Module API
Module requirements:
- implements the *ProcessingModuleInterface* class;
- implements the *ProcessingModuleFactoryInterface* class;
- declares itself as "loadable" with help of the macro *DECLARE_LOADABLE_MODULE*.

The *ProcessingModuleInterface* defines methods that firmware will call to
configure and to start the stream processing. The *ProcessingModuleInterface*
header is available at:
``<repo_dir>/FW/src/intel_adsp/include/processing_module_interface.h``
There is also *ProcessingModule* class, which provided is as a partial default
implementation of the *ProcessingModuleInterface* class:
``<repo_dir>/FW/src/intel_adsp/include/processing_module.h``

#### ProcessingModule class
A custom module class needs to implement at least these three functions:
- **ProcessingModule::Process()**
Processes the stream buffers extracted from the input pins and produces the
resulting signal in the stream buffer of the output pins.

- **ProcessingModule::SetConfiguration()**
Applies the upcoming configuration message for the given configuration ID.

- **ProcessingModule::SetProcessingMode()**
Sets the processing mode for the module.

Optionally, the following methods can be overridden in *ProcessingModule*:
- **ProcessingModule::GetConfiguration()**
Retrieves the configuration message for the given configuration ID. 
 
- **ProcessingModule::Reset()**
Upon a call to this method, the firmware requires the module to reset its
internal state into a well-known initial value. Parameters that may have been
set through SetConfiguration() are expected to be left unchanged. 

#### ProcessingModuleFactory class
The *ProcessingModuleFactoryInterface* class handles the creation of the custom
processing module components. Following methods should be implemented:
- **ProcessingModuleFactory::Create()**
Creates a RegisteredProcessingModule instance. The custom implementation of the
Create method is expected to handle the initialization of the custom module
instances. 

- **ProcessingModuleFactory::GetPrerequisites()**
Indicates the prerequisites for module instance creation. Firmware calls this
method before each module instance creation.

### Modules Configuration
The module design configuration is placed in binamp file. The module examples
binmaps are placed in:
``<repo_dir>/FW/intel_common/module_binmaps``
Module binmaps contains information about several module parameters e.g. module
uuid, module version, input/output pins, number of channels, sample size,
schedule capabilities. Ultimately, binmaps should be customized according to
user needs.

#### Module name
Usage:
```
module module_type module_name
```
Example:
```
module o AMPLI
```
*module_type* can take following values:
- *o*   - optional module
- *b*   - built-in module
- *ba*  - built-in auto-init module
- *d*   - module packaged into separate file
- *da*  - auto-init module packaged into seperate file
- *l*   - internal common sections module

*module_name* is a string name o module. It does not have to be unique. Should
not exceed 8 characters.

#### Module UUID
Usage:
```
uuid uuid_number
```
Example:
```
uuid 8075F4F8-6214-4A61-8C08-884BE5D14FF8
```
*uuid_number* unique UUID number. See [UUID Wikipedia](https://en.wikipedia.org/wiki/Universally_unique_identifier)
for a description.

#### Module pretty name
Usage:
```
name pretty_name
```
Example:
```
name Aca module example
```
*pretty_name* does not have to be unique. It is not limited to 8 characters.
#### Module version
Usage:
```
version_major major_value
version_minor minor_value
version_hotfix hotfix_value
version_build build_value
```
Example: 
```
version_major 0x1
version_minor 0x0
version_hotfix 0x0
version_build 0x0
```
- *version_major* - shall be used only for vendor parameter structure change
(user decide how to use version parameter);
- *version_minor* - shall be used only for internal changes (optimizations,
bug fix);
- *version_hotfix* - shall be used during the bug fixing period;
- *version_build* - shall be incremented with new build.

#### Module affinity
Usage:
```
affinity_mask mask
```
Example:
```
affinity_mask MASTER_CORE_AFFINITY
```
*mask* is a bit-mask of cores allowed to execute module. Macros reffering this
mask are defined at:
``<repo_dir>/FW/portable/include/modules.h``

#### Module instance count
Usage:
```
instance_count count
```
Example:
```
instance_count +1
```
*instance_count* refers to number of module instance. The value should be
provided with "+" at beginning. It will be added to basic instance count.

#### Scheduler domain type:
Usage: 
```
domain_types type
```
Example
```
domain_types DP
```
Following domain types are available :
- *LL* - low latency domain is intended to be able transfer data through DSP
subsystem within 2ms. Processing modules used in that domain must be able to
work on limited number of samples. To meet 2ms pipeline latency on stream
sampled at 48 kHz stream needs to be processed in 1ms periods i.e. using maximum
48 sample blocks. 
- *DP* - Data processing domain is intended host all processing that does not
fit in definition of low latency domain. It is important that this domain is
still real time and latency sensitive.

#### Module type
Usage:
```
type class_name
```
Example:
```
type AudClassModule
```
*class_name* refers to module class type supported by firmware. Available
modules classes are placed in:
``<repo_dir>/FW/intel_common/module_binmaps/dsp_fw_common.binmap``
in ``# type <mod_func_type>`` section.

#### Module stack size
Usage:
```
stack_size size
```
Example:
```
stack_size 1000
```
*stack_size is size of stack that module instance requires for its task.
It refer only to DP scheduler domain.


#### Scheduling capabilities
Usage:
```
sched_caps scheduling_period multiples_supported
```
Example:
```
sched_caps 1 all
```
- *scheduling_period* - scheduling period should be given in samples per channel
(i.e. 1 sample = 1 sample per channel) as a hexadecimal value.
- *multiples_supported* - indicates the supported period multiples. Available
values are placed in:
``<repo_dir>/FW/intel_common/module_binmaps/dsp_fw_common.binmap``
in ``# multiples_supported <list_of_supported_multiples>`` section.

#### Module config
Usage:
```
mod_cfg PAR_0 PAR_1 PAR_2 PAR_3 IS_BYTES CPS IBS OBS MOD_FLAGS CPC OBLS
```
Example:
```
mod_cfg 0 0 0 0 4096 500000 180 180 0 5000 0
```
*mod_cfg* required following parameters:
- *PAR0-3* - any module parameters;
- *IS_BYTES* - actual size of instance .bss (pages);
- *CPS (Cycles Per Second)* - indicates the max count of cycles per second which
are granted to a certain module to complete the processing of its input and
output buffers;
- *IBS (Input Buffer Size)* - input buffer size (in bytes) that module shall
process (within ProcessingModuleInterface::Process()) from every connected
input pin;
- *OBS (Output Buffer Size)* - output buffer size (in bytes) that module shall
produce (within ProcessingModuleInterface::Process()) on every connected
output pin;
- *MOD_FLAGS (Module Flags)* - for future use
- *CPC (Cycles Per Chunk)* - indicates the max count of Cycles Per Chunk which
are granted to a certain module to complete the processing of its input and
output buffers;
- *OBLS (Output Block Size)* - for future use.

#### Input/Output pins
Usage:
```
pin in
stream_type type
sample_rates rates
sample_sizes sizes
sample_containers containers
channel_cfg ch_cfg
```
Example:
```
pin in
stream_type pcm
sample_rates 44.1k 48k
sample_sizes sample_16b sample_24b sample_32b
sample_containers container_16b container_32b
channel_cfg ch_mono ch_stereo
```

A pin is a connector that transmits PCM audio streaming: one pin can transmit
several channels (for example, a stereo signal requires only one pin). There are
two pin types: input (*in*) and output (*out*). Additional pin properties are:
- *stream_type*
- *sample_rates*
- *sample_sizes*
- *sample_containers*
- *channel_cfg*

Values for above properties are available in common binmap file:
``<repo_dir>/FW/intel_common/module_binmaps/dsp_fw_common.binmap``
in respectively: 
- ``# stream_type <type>``,
- ``# sample_rates <list_of_supported_sample_rates>``,
- ``# sample_sizes <list_of_supported_sample_sizes>``,
- ``# sample_containers <list_of_supported_sample_containers>``,
- ``# channel_cfg <list_of_supported_channel_configurations>``
sections.
 
### Modules Examples
#### Aca_module
Aca_module  is an implementation example of post-processing module which simply
amplifies the input stream by a constant gain value. The aca_module is a single
input-single output module.
#### Amplifier_module

Amplifier_module is a post-processing module (it should be used on the render
path). This is a very basic module implementation supporting one pin in input
and one pin in output. The module implementation doesn't include any compiled
library. The parameters set (four parameters) is defined on one configuration
ID. "target_gain" is the gain value that is applied in the module. If
"target_gain" exceeds the range defined with "min_gain" and "max_gain", the
applied value is replaced with the max/min limit. "smoothing_factor" parameter
is the convergence speed ramp to reach the new gain value.

#### Downmixer_module

Downmixer_module is a pre-processing module (it should be used on the capture
path). This is a more advanced module implementation as it supports two pins in
the input. The second input pin is used for the reference signal: when
performing a playback and a record at the same time, the playback signal is sent
on the second input pin as a reference, as needed for an echo canceller for
example. As for the amplifier, the parameters set is basic and defined on one
configuration ID. The output is the result of a mix between input and reference
signal balanced with "main_div" and "ref_div" parameter values:
``output_signal = input_signal/main_div + reference_signal/ref_div``

### Building toolchain from source

Build the xtensa cross-compilation toolchains with crosstoolg-ng for lx7hifi4
platform. Building instruction is available here: [build toolchains](https://thesofproject.github.io/latest/getting_started/build-guide/build-from-scratch.html#step-2-build-toolchains-from-source).

NOTE: Switch to branch https://github.com/thesofproject/crosstool-ng/tree/sof-gcc10x
for crosstool-ng and https://github.com/thesofproject/xtensa-overlay/tree/sof-gcc10.2
for xtensa-overlay repositories.

### Building modules

Module examples (aca_module, amplifier_module and downmixer_module) are placed
in ``<repo_dir>/modules/``. In order to build the example, enter the module
build directory ``<repo_dir>/modules/<module_example_name>/build``
(e.g. ``<repo_dir>/modules/aca_module/build``) and invoke make command i.e.:
```
make <platform_name> <config> TOOLSCHAIN=<toolchain_name> OVERLAY_TOOLS_DIR=<overlay_dir> XTENSA_TOOLS_DIR=<xtensa_tools_dir>

<platform_name>     refers to platform target e.g. lx7hifi4_plat / lx7hifi4-s_plat
<config>            release/debug config
<toolchain_name>    toolchain name
<overlay_dir>       path to toolchain bin directory of toolchain generated by crosstool-ng
<xtensa_tools_dir>  path to xtensa tools directory
```

Examples:
- build module using xtensa-lx7hifi4-elf toolchain (OVERLAT_TOOLS_DIR should point
to the toolchain bin directory)
```
make lx7hifi4_plat release TOOLSCHAIN="xtensa-lx7hifi4-elf" OVERLAY_TOOLS_DIR="~/lx7hifi4_xtensa_overlays/crosstool-ng/builds/xtensa-lx7hifi4-elf/bin/"
```
- build module using xtensa toolchain
```
make lx7hifi4_plat release TOOLSCHAIN="xtensa" XTENSA_TOOLS_DIR="C:\usr\xtensa\Xplorer-8.0.11-workspaces\install\tools"
```

Building artifacts will be placed in following directory:
``<repo_dir>/modules/<module_example_name>/build/out``

NOTE: At the moment building is available only for lx7hifi4_plat and lx7hifi4-s_plat 
platforms with "xtensa-lx7hifi4-elf" toolchain.
## Loadable library

Loadable library contains set of N precompiled loadable modules. In order
to build loadable library, firstly the specific module/modules should be
compiled (see [Building modules](#building-modules) section).

### Build_library.py script
In order to build loadable library the:
``<repo_dir>/scripts/build_library.py``
can be utilized. 

```
python3 build_library.py --help
usage: build_library.py [-h] [-o OUTPUT_PATH] [-c {release,debug}] [-a ATTR_FILE_PATH] [-t {xtensa,xtensa-lx7hifi4-elf}] platform_name library_name library_version modules_list

positional arguments:
  platform_name         name of target platform
  library_name          name of output library
  library_version       version of output library
  modules_list          modules to be included in output library (e.g. "module_1_name, module_2_name")

optional arguments:
  -h, --help            show this help message and exit
  -o OUTPUT_PATH, --output_path OUTPUT_PATH
                        path to output build folder (default: ./out)
  -c {release,debug}, --config {release,debug}
                        type of config (default: release)
  -a ATTR_FILE_PATH, --attr_file_path ATTR_FILE_PATH
                        path to xml attribute file (default: /mnt/c/conv-fdk/artifacts/dsp_fw_example_release.xml)
  -t {xtensa,xtensa-lx7hifi4-elf}, --toolschain {xtensa,xtensa-lx7hifi4-elf}
                        default: xtensa-lx7hifi4-elf
```
Before invoking ``build_library.py`` proper tools (e.g. meu tool, signing keys)
paths should be set in: ``<repo_dir>/scripts/lib_config.conf``

``build_library.py`` script builds loadable library containig modules
passed in ``modules_list`` argument. It searches modules binaries
(``<module_name>.dlm.a files``) in default modules build location i.e.:
``<repo_dir>/modules/<module_name>/build/out/lx7hifi4_plat/<config>/``
Above script also calculates the loadable library address using firmware memory
footprint files (memory footprint file example is available:
``<repo_dir>/artifacts/dsp_fw_example_release.xml`` and it is used by script by
default - it can be changed by ``-a`` flag). Ultimately, memory footprint file
will be shared as a part of a release package.

### Usage examples

Builds library called "aca_lib" containing "aca_module" for lx7hifi4_plat platform.
The library version is set to 1.8. Script uses release aca_module binary as
default. Build artifacts will be placed in out folder by default.
```
python3 build_library.py lx7hifi4_platorlake aca_lib 1.8 aca_module
```

Builds library called "example_lib" containing "aca_module" and
"amplifier_module" for lx7hifi4_plat platform. The library version is set to 1.8.
Script uses release aca_module binary as default. Build artifacts will be placed
in out folder by default.
```
python3 build_library.py lx7hifi4_plat example_lib 1.8 "aca_module amplifier_module"
```

## Getting started

1. Build the xtensa cross-compilation toolchains ([Building toolchain](building_toolchain_from_source) section).
2. Configure module - use default or modify binmap file for specific module ([Module configuration](modules_configuration) section).
3. Build module/modules ([Building modules](building_modules) section).
4. Build loadable library containig specific module/modules ([Loadable library](loadable_library) section).

## Repository content - summary

- ``<repo_dir>/linux/sw/adsp_fw_builder`` - adsp_fw_builder for linux.
adsp_fw_builder is a DSP firmware image creation tool used to create manifest.

- ``<repo_dir>/windows/sw/adsp_fw_builder`` - adsp_fw_builder for windows.

- ``<repo_dir>/modules/`` - directory containing module examples

- ``<repo_dir>/scripts/build_library.py`` - script for building loadable
library (see [Loadable library](#loadable-library) section)

- ``<repo_dir>/FW/src/intel_adsp/`` - directory containing building
infrastracture e.g. necessary source files, headers and makefiles. 

- ``<repo_dir>/FW/intel_common/module_binmaps`` - directory containig
modules and firmware binmaps i.e. files used for configuration (e.g. setting
input/output pins, supported sample rate etc.).

- ``<repo_dir>/artifacts/dsp_fw_example_release.xml`` - example of dsp memory
footprint file used in building process to calculate loadable library address

- ``<repo_dir>/docs/INTEL_ADSP_Module_API.chm`` - module API documentation