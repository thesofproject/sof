# SOF Tensorflow Micro Integration

The TFLM module integrates tensorflow micro into SOF as an audio classifiction and in the future an audio processing module. This module is currently still work in progress, and needs a few features to be completed before its ready for production.

## Building

Please run ```./scripts/tensorflow-clone.sh``` before building as this will clone all dependencies required to build tensor flow micro with optimized xtensa kernels.

Please select the following in Kconfig
```
CONFIG_CPP=y
CONFIG_STD_CPP17=y
CONFIG_SOF_STAGING=y
```

To build as built-in, select ```CONFIG_COMP_TENSORFLOW=y``` in Kconfig and build.

To build as a llext module, select ```CONFIG_COMP_TENSORFLOW=m``` in Kconfig and the build.

## Running

The TFLM SOF module needs features created by the MFCC module as its input. This step is still in progress. The following pipeline will be used

``` Mic --> MFCC --> TFLM --> Classification ```

## TODO

1) Create MFCC pipeline and align with TFLM micro_speech audio feature extraction configuration.

2) Support compressed output PCM with Classification results.

3) Load tflite models via binary kcontrol.

4) Support audio processing via TFLM module.