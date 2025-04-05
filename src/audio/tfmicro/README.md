# SOF Tensorflow Micro Integration

The TFLM module integrates tensorflow micro into SOF as an audio classifiction and in the future an audio processing module. This module is currently still work in progress, and needs a few features to be completed before its ready for production.

## Building

Please run ```./scripts/tensorflow-clone.sh``` before building as this will clone all dependecies required to build tensor flow micro with optimized xtensa kernels.

To build as built-in, select ```CONFIG_COMP_TFMICRO=y``` in Kconfig and build.

To build as a module, select ```CONFIG_COMP_TFMICRO=m``` in Kconfig and the build. This will build the llext module but currently this module will be missing symbols from tfmicro and nnlib archives. A manual step needs done to link in those symbols. i.e.

```cd build-mtl/zephyr/tfmicro```

```xt-clang++ -flto -r tfmicro.llext -nostdlib -nodefaultlibs -ffreestanding -mno-generate-flix -L . -ltfmicro_lib -lnn_hifi_lib -lc++ -lc -lm -fno-rtti -ffunction-sections -fdata-sections -lgcc -o tftest -Wl,--gc-sections -Wl,--entry=sys_comp_module_tfmicro_interface_init```

```xt-objcopy --remove-section=.xt.*  tftest```

This will create a llext module called ```tftest``` that will contain all the nessessary symbols. Please note the commands above are WIP and will be optimized as development continues.

## Running

The TFLM SOF module needs features craeted by the MFCC module as its input. This step is still in progress. The following pipeline will be used

``` Mic --> MFCC --> TFLM --> Clasification ```

## TODO

1) Create MFCC pipeline and align with TFLM micro_speech audio feature extraction configuration.

2) Support compressed output PCM with classification results.

3) Load tflite models via binary kcontrol.

4) Support audio processing via TFLM module.