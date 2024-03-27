#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2024, Intel Corporation.

import argparse
import mmap
import ctypes
import time
import sys

TELEMETRY2_PAYLOAD_MAGIC = 0x1ED15EED

TELEMETRY2_CHUNK_ID_EMPTY = 0
TELEMETRY2_ID_THREAD_INFO = 1

TELEMETRY2_PAYLOAD_V0_0 = 0

#telemetry2 payload
class Telemetry2PayloadHdr(ctypes.Structure):
	_fields_ = [
		("magic", ctypes.c_uint),
		("hdr_size", ctypes.c_uint),
		("total_size", ctypes.c_uint),
		("abi", ctypes.c_uint),
		("tstamp", ctypes.c_ulonglong)
	]

# telemetry2 header struct
class Telemetry2ChunkHdr(ctypes.Structure):
	_fields_ = [("id", ctypes.c_uint), ("size", ctypes.c_uint)]

def get_telemetry2_payload_ok(slot):
	payload_hdr = ctypes.cast(slot,
				  ctypes.POINTER(Telemetry2PayloadHdr))
	if payload_hdr.contents.magic != TELEMETRY2_PAYLOAD_MAGIC:
		print("Telemetry2 payload header bad magic 0x%x\n" %
		      payload_hdr.contents.magic)
		return False
	if payload_hdr.contents.abi != TELEMETRY2_PAYLOAD_V0_0:
		print("Unknown Telemetry2 abi version %u\n" %
		      payload_hdr.contents.abi)
	return True

def get_telemetry2_chunk_offset(id, slot):
	"""
	Generic function to get telmetry2 chunk offset by type
	"""
	offset = 0
	while True:
		struct_ptr = ctypes.cast(slot[offset:],
					 ctypes.POINTER(Telemetry2ChunkHdr))
		#print("Checking %u id 0x%x size %u\n" %
		#      (offset, struct_ptr.contents.id, struct_ptr.contents.size))
		if struct_ptr.contents.id == id:
			return offset
		if struct_ptr.contents.size == 0:
			return -1
		offset = offset + struct_ptr.contents.size

class ThreadInfo(ctypes.Structure):
	_pack_ = 1
	_fields_ = [
		("name", ctypes.c_char * 14),
		("stack_usage", ctypes.c_ubyte),
		("cpu_usage", ctypes.c_ubyte),
	]

class CPUInfo(ctypes.Structure):
	_pack_ = 1
	_fields_ = [
		("state", ctypes.c_ubyte),
		("counter", ctypes.c_ubyte),
		("load", ctypes.c_ubyte),
		("thread_count", ctypes.c_ubyte),
		("thread", ThreadInfo * 16),
	]

class ThreadInfoChunk(ctypes.Structure):
	_pack_ = 1
	_fields_ = [
		("hdr", Telemetry2ChunkHdr ),
		("core_count", ctypes.c_ushort),
		("core_offsets", ctypes.c_ushort * 16),
	]

class CoreData:
	""" Class for tracking thread analyzer info for one core """
	counter = 0
	offset = 0
	core = 0
	STATE_UNINITIALIZED = 0
	STATE_BEING_UPDATED = 1
	STATE_UPTODATE = 2

	def __init__(self, offset, core):
		self.counter = 0
		self.offset = offset
		self.core = core

	def print(self, slot):
		cpu_info = ctypes.cast(slot[self.offset:],
				       ctypes.POINTER(CPUInfo))
		if self.counter == cpu_info.contents.counter:
			return
		if cpu_info.contents.state != self.STATE_UPTODATE:
			return
		self.counter = cpu_info.contents.counter
		print("Core %d load %02.1f%%" %
		      (self.core, cpu_info.contents.load / 2.55))
		for i in range(cpu_info.contents.thread_count):
			thread = cpu_info.contents.thread[i]
			print("\t%-20s cpu %02.1f%% stack %02.1f%%" %
			       (thread.name.decode('utf-8'), thread.cpu_usage / 2.55,
				thread.stack_usage / 2.55));

class Telemetry2ThreadAnalyzerDecoder:
	"""
	Class for finding thread analyzer chuck and initializing CoreData objects.
	"""
	file_size = 4096 # ADSP debug slot size
	f = None
	chunk_offset = -1
	chunk = None
	core_data = []

	def set_file(self, f):
		self.f = f

	def decode_chunk(self):
		if self.chunk != None:
			return
		self.f.seek(0)
		slot = self.f.read(self.file_size)
		if not get_telemetry2_payload_ok(slot):
			return
		self.chunk_offset = get_telemetry2_chunk_offset(TELEMETRY2_ID_THREAD_INFO, slot)
		if self.chunk_offset < 0:
			print("Thread info chunk not found")
			self.chunk = None
			return
		self.chunk = ctypes.cast(slot[self.chunk_offset:],
					 ctypes.POINTER(ThreadInfoChunk))
		hdr = self.chunk.contents.hdr
		#print(f"Chunk at {self.chunk_offset} id: {hdr.id} size: {hdr.size}")
		print(f"core_count: {self.chunk.contents.core_count}")

	def init_core_data(self):
		if self.chunk == None:
			return
		if len(self.core_data) == self.chunk.contents.core_count:
			return
		self.core_data = [None] * self.chunk.contents.core_count
		for i in range(self.chunk.contents.core_count):
			offset = self.chunk_offset + self.chunk.contents.core_offsets[i]
			self.core_data[i] = CoreData(offset, i)
			#print(f"core_offsets {i}: {self.chunk.contents.core_offsets[i]}")

	def loop_cores(self):
		if len(self.core_data) == 0:
			return
		self.f.seek(0)
		slot = self.f.read(self.file_size)
		for i in range(len(self.core_data)):
			self.core_data[i].print(slot)

	def reset(self):
		self.f = None
		self.chunk = None

	def print_status(self, tag):
		print(f"{tag} f {self.f} offset {self.chunk_offset} chunk {self.chunk} cores {len(self.core_data)}")

def main_f(args):
	"""
	Map telemetry2 slot into memorey and find the chunk we are interested in
	"""
	decoder = Telemetry2ThreadAnalyzerDecoder()
	prev_error = None
	while True:
		try:
			with open(args.telemetry2_file, "rb") as f:
				decoder.set_file(f)
				decoder.decode_chunk()
				decoder.init_core_data()
				while True:
					decoder.loop_cores()
					time.sleep(args.update_interval)
		except FileNotFoundError:
			print(f"File {args.telemetry2_file} not found!")
			break
		except OSError as e:
			if str(e) != prev_error:
				print(f"Open {args.telemetry2_file} failed '{e}'")
				prev_error = str(e)
			decoder.reset()
		time.sleep(args.update_interval)

def parse_params():
	""" Parses parameters
	"""
	parser = argparse.ArgumentParser(description=
					 "SOF Telemetry2 thread info client. "+
					 "Opens telemetry2 slot debugfs file, looks"+
					 " for thread info chunk, and decodes its "+
					 "contentes into stdout periodically.")
	parser.add_argument('-t', '--update-interval', type=float,
			    help='Telemetry2 window polling interval in seconds, default 1',
                            default=1)
	parser.add_argument('-f', '--telemetry2-file',
			    help='File to read the telemetry2 data from, default /sys/kernel/debug/sof/telemetry2',
                            default="/sys/kernel/debug/sof/telemetry2")
	parsed_args = parser.parse_args()
	return parsed_args

args = parse_params()
main_f(args)
