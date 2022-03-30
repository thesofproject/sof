This directory can contain additional west manifest files. Any files
in this directory will be included in the main west.yml file sorted by
filename.

For more details about how this works, see this part of the west
documentation:

https://docs.zephyrproject.org/latest/guides/west/manifest.html#example-2-2-downstream-with-directory-of-manifest-files

Note: do not delete this file if there is no other content in submanifests directory.
Submanifests directory must exist for west update command to work (even if does not contain any actual manifests).
This is a known bug, see: https://github.com/zephyrproject-rtos/west/issues/594
