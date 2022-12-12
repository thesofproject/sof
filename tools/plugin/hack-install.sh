#!/bin/bash

# hacky installler for modules on host to ALSA plugin dir

DEST_DIR="/usr/lib/x86_64-linux-gnu/alsa-lib"

rm ${DEST_DIR}/libsof-*.so

#cp ./sof_ep/install/lib/libsof_drc_avx2.so

cp ./sof_ep/install/lib/libsof_mixer_avx2.so \
	${DEST_DIR}/libsof-37c006bcaa127c419a9789282e321a76.so

#cp ./sof_ep/install/lib/libsof_asrc_avx2.so
#cp ./sof_ep/install/lib/libsof_crossover_avx2.so
#cp ./sof_ep/install/lib/libsof_tdfb_avx2.so

# SRC /* c1c5326d-8390-46b4-aa47-95c3beca6550 */
cp ./sof_ep/install/lib/libsof_src_avx2.so \
	${DEST_DIR}/libsof-6d32c5c19083b446aa4795c3beca6550.so

#cp ./sof_ep/install/lib/libsof_dcblock_avx2.so

# FIR /* 43a90ce7-f3a5-41df-ac06-ba98651ae6a3 */
cp ./sof_ep/install/lib/libsof_eq-fir_avx2.so \
	${DEST_DIR}/libsof-e70ca943a5f3df4106acba98651ae6a3.so

cp ./sof_ep/install/lib/libsof_volume_avx2.so \
	${DEST_DIR}/libsof-7e677eb7f45f8841af14fba8bdbf8682.so

#cp ./sof_ep/install/lib/libsof_multiband_drc_avx2.so

# IIR /* 5150c0e6-27f9-4ec8-8351-c705b642d12f */
cp ./sof_ep/install/lib/libsof_eq-iir_avx2.so \
	${DEST_DIR}/libsof-e6c05051f927c84e5183c705b642d12f.so

# file read and write is same module
# fileread /* bfc7488c-75aa-4ce8-9bde-d8da08a698c2 */
# filewrite /* f599ca2c-15ac-11ed-a969-5329b9cdfd2e */
cp ./modules/libsof_mod_file.so ${DEST_DIR}/libsof-8c48c7bfaa75e84cde9bd8da08a698c2.so
ln -s ${DEST_DIR}/libsof-8c48c7bfaa75e84cde9bd8da08a698c2.so \
	  ${DEST_DIR}/libsof-2cca99f5ac15ed1169a95329b9cdfd2e.so

# ALSA arecord and playback is same module
# arecord /* 66def9f0-39f2-11ed-89f7-af98a6440cc4 */
# aplay /* 72cee996-39f2-11ed-a08f-97fcc42eaaeb */

cp ./modules/libsof_mod_alsa.so \
	${DEST_DIR}/libsof-96e9ce72f239ed11a08f97fcc42eaaeb.so

ln -s ${DEST_DIR}/libsof-96e9ce72f239ed11a08f97fcc42eaaeb.so \
	  ${DEST_DIR}/libsof-f0f9de66f239ed11f789af98a6440cc4.so
	  
# SHM arecord and playback is same module
# shm read /* dabe8814-47e8-11ed-8ba5-b309974fecce */
# shm write /* e2b6031c-47e8-11ed-807f-07a91b6efa6c */ 

cp ./modules/libsof_mod_shm.so \
	${DEST_DIR}/libsof-1c03b6e2e847ed1107a97f801b6efa6c.so

ln -s ${DEST_DIR}/libsof-1c03b6e2e847ed1107a97f801b6efa6c.so \
	  ${DEST_DIR}/libsof-1488bedae847ed11a58bb309974fecce.so

	  
ls -l ${DEST_DIR}/libsof*