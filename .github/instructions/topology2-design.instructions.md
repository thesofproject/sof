---
description: "Agent instructions for designing and updating ALSA topology v2 files in SOF"
applyTo: 'tools/topology/topology2/**'
---

# Topology2 Design Instructions

Use this guidance when creating or modifying files under tools/topology/topology2.
These rules align with the topology2 README and capture expected design patterns for
class-based ALSA topology v2 authoring.

## Scope

* Applies to topology2 .conf definitions, platform overrides, pipeline and DAI class files, and topology2 CMake target lists
* Focuses on design consistency, ID safety, and maintainable reuse of existing class and object templates

## Core Model

* Use topology2 object model primitives consistently: Class.*, Object.*, Define, and IncludeByKey
* Prefer reusable classes in include/ over one-off duplicated object blocks
* Keep object instantiation explicit and readable so generated pipelines are traceable

## Top Level Topology Layout

For new top-level board .conf files, keep this order:

1. Search directories
2. Required class includes
3. Define block with defaults
4. IncludeByKey.PLATFORM overrides
5. Feature-gated IncludeByKey blocks
6. DAI, pipeline, and PCM objects
7. Route definitions

## Reuse Before New Base Files

* Prefer extending an existing base input .conf with variable overrides from CMake targets
* Add a new base .conf only when existing topologies cannot represent the use case cleanly
* When adding a new target, use this tuple structure. In CMake quoted strings, escape each semicolon as `\;`.

```text
Logical tuple format:
"input-conf;output-name;variable1=value1,variable2=value2"

CMake string form:
"input-conf\;output-name\;variable1=value1,variable2=value2"
```

## ID Conventions and Safety

* Keep PCM IDs unique within a single topology
* Keep pipeline indexes unique within a single topology
* Pair FE and BE pipelines as N and N+1 where applicable
* For SoundWire pipelines, follow index equals PCM ID times 10 unless a documented topology-specific exception exists
* For HDMI pipelines, keep stride-10 layout with host at N0 and DAI at N1
* When combining features such as SDW, PCH DMIC, HDMI, deep buffer, or compress, verify there are no ID collisions after overrides

## Routing Rules

* Connect FE mixin outputs to BE mixout inputs using Object.Base.route
* Keep route naming and widget references aligned with topology naming patterns
* Validate that each route endpoint maps to a declared widget in the same compiled topology

## Widget Naming

* Follow naming pattern type.pipeline-index.instance
* Keep naming stable and descriptive for easier graph inspection and debug

## Platform Overrides

* Use IncludeByKey.PLATFORM for platform-specific Define overrides
* Restrict platform-specific tuning to platform/intel/*.conf instead of duplicating board-level logic
* Ensure platform keys remain consistent with the authoritative in-tree overrides under tools/topology/topology2/platform/intel/*.conf; current examples include tgl, adl, mtl, lnl, ptl, and nvl

## CMake Target Placement

Register targets in the correct file for platform generation:

* Tiger Lake and Alder Lake: production/tplg-targets-cavs25.cmake
* Meteor Lake: production/tplg-targets-ace1.cmake
* Lunar Lake: production/tplg-targets-ace2.cmake
* Panther Lake: production/tplg-targets-ace3.cmake
* Nova Lake / sof-nvl-*: production/tplg-targets-ace4.cmake
* i.MX8 platforms: production/tplg-targets-imx8.cmake
* SDCA generic topologies: production/tplg-targets-sdca-generic.cmake
* HDA generic: production/tplg-targets-hda-generic.cmake
* Development and test topologies: development/tplg-targets.cmake

If a target family is not listed above, use the existing tplg-targets-*.cmake
file that already contains similar topologies as the source of truth, and keep new
targets grouped with the same platform or product family in either production/ or
development/.

## Validation Expectations

* Keep topology2 buildable through the topologies2 target
* Preserve compatibility with alsatplg pre-processing mode used by the build system
* Ensure topology edits remain synchronized with nearby architecture or README documentation when design behavior changes
