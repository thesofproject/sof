# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof rtnr.c)

sof_add_static_library(SOF_RTK_MA_API rtklib/tgl/libSOF_RTK_MA_API.a)
sof_add_static_library(Suite_rename rtklib/tgl/libSuite_rename.a)
sof_add_static_library(Net rtklib/tgl/libNet.a)
sof_add_static_library(Preset rtklib/tgl/libPreset.a)
