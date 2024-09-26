#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2024, Intel Corporation.

"""
For receiving, decoding and printing debug-stream records over debug window slot.
"""

import argparse
import ctypes
import time
import sys
import os

import logging

logging.basicConfig(
    format="%(filename)s:%(lineno)s %(funcName)s: %(message)s", level=logging.WARNING
)

DEBUG_STREAM_PAYLOAD_MAGIC = 0x1ED15EED
DEBUG_SLOT_SIZE = 4096

# TODO: python construct would probably be cleaner than ctypes structs

class DebugStreamHdr(ctypes.Structure):
    """
    Generic Debug-stream header
    """

    _fields_ = [
        ("magic", ctypes.c_uint),
        ("hdr_size", ctypes.c_uint),
    ]


class DebugStreamRecord(ctypes.Structure):
    """
    Debug Stream record for passing debug data
    """

    _fields_ = [
        ("id", ctypes.c_uint),
        ("seqno", ctypes.c_uint),
        ("size_words", ctypes.c_uint),
    ]


class CPUInfo(ctypes.Structure):
    """
    Thread Info record header
    """

    _pack_ = 1
    _fields_ = [
        ("hdr", DebugStreamRecord),
        ("load", ctypes.c_ubyte),
        ("thread_count", ctypes.c_ubyte),
    ]


class ThreadInfo(ctypes.Structure):
    """
    Thread specific data-record, the thread name string starts after name_len
    """

    _pack_ = 1
    _fields_ = [
        ("stack_usage", ctypes.c_ubyte),
        ("cpu_load", ctypes.c_ubyte),
        ("name_len", ctypes.c_ubyte),
    ]


WSIZE = ctypes.sizeof(ctypes.c_uint)


class RecordPrinter:
    """
    Debug Stream record decoder and printer class
    """

    RECORD_ID_UNINITIALIZED = 0
    RECORD_ID_THREAD_INFO = 1

    def print_record(self, record, cpu):
        """prints debug-stream record"""
        recp = ctypes.cast(record, ctypes.POINTER(DebugStreamRecord))
        logging.debug(
            "rec: %u %u %u", recp.contents.id, recp.contents.seqno, recp.contents.size_words
        )
        if recp.contents.id == self.RECORD_ID_THREAD_INFO:
            return self.print_thread_info(record, cpu)
        logging.warning("cpu %u: Unsupported recodrd type %u", cpu, recp.contents.id)
        return True

    def print_thread_info(self, record, cpu):
        """prints thread-info record"""
        remlen = len(record) - ctypes.sizeof(CPUInfo)
        if remlen < 0:
            logging.info("Buffer end reached, parsing failed")
            return False
        cpup = ctypes.cast(record, ctypes.POINTER(CPUInfo))
        print(
            "CPU %u: Load: %02.1f%% %u threads (seqno %u)"
            % (
                cpu,
                cpup.contents.load / 2.55,
                cpup.contents.thread_count,
                cpup.contents.hdr.seqno,
            )
        )
        remain = (ctypes.c_ubyte * (len(record) - ctypes.sizeof(CPUInfo))).from_address(
            ctypes.addressof(record) + ctypes.sizeof(CPUInfo)
        )
        for i in range(cpup.contents.thread_count):
            remlen = remlen - ctypes.sizeof(ThreadInfo)
            if remlen < 0:
                logging.info("Buffer end reached, parsing failed on %u thread field", i)
                return False
            threadp = ctypes.cast(remain, ctypes.POINTER(ThreadInfo))
            remain = (
                ctypes.c_ubyte * (len(remain) - ctypes.sizeof(ThreadInfo))
            ).from_address(ctypes.addressof(remain) + ctypes.sizeof(ThreadInfo))
            remlen = remlen - threadp.contents.name_len
            if remlen < 0:
                logging.info("Buffer end reached, parsing failed on %u thread field", i)
                return False
            name = bytearray(remain[: threadp.contents.name_len]).decode("utf-8")
            remain = (
                ctypes.c_ubyte * (len(remain) - threadp.contents.name_len)
            ).from_address(ctypes.addressof(remain) + threadp.contents.name_len)
            print(
                "    %-20s stack %02.1f%%\tload %02.1f%%"
                % (
                    name,
                    threadp.contents.stack_usage / 2.55,
                    threadp.contents.cpu_load / 2.55,
                )
            )
        return True


class DebugStreamSectionDescriptor(ctypes.Structure):
    """
    Describes CPU specific circular buffers
    """

    _fields_ = [
        ("core_id", ctypes.c_uint),
        ("buf_words", ctypes.c_uint),
        ("offset", ctypes.c_uint),
    ]


class DebugStreamSlotHdr(ctypes.Structure):
    """
    Debug Slot transport specific Debug Stream header, padded to cache line
    """

    _fields_ = [
        ("hdr", DebugStreamHdr),
        ("total_size", ctypes.c_uint),
        ("num_sections", ctypes.c_uint),
    ]


class CircularBufHdr(ctypes.Structure):
    """
    Live data header for CPU specific circular buffer
    """

    _fields_ = [
        ("next_seqno", ctypes.c_uint),
        ("w_ptr", ctypes.c_uint),
    ]


class CircularBufferDecoder:
    """
    Class for extracting records from circular buffer
    """

    desc = None
    boffset = None
    buf_words = None
    cpu = None
    printer = None
    prev_w_ptr = 0
    prev_seqno = None
    error_count = 0

    def __init__(self, desc, cpu, printer):
        self.desc = desc
        self.boffset = desc.offset + ctypes.sizeof(CircularBufHdr)
        self.buf_words = desc.buf_words
        self.cpu = cpu
        self.printer = printer
        logging.debug(
            "boffset %u buf_words %u cpu %u", self.boffset, self.buf_words, self.cpu
        )

    def get_hdr(self, slot, pos):
        """
        Get record header from position (handles circular buffer wrap)
        """
        if pos >= self.buf_words:
            logging.warning("Bad position %u", pos)
            return None
        hdr_size = ctypes.sizeof(DebugStreamRecord)
        hdr_words = hdr_size // WSIZE
        if pos + hdr_words > self.buf_words:
            hdr = (ctypes.c_ubyte * hdr_size)()
            size1 = (self.buf_words - pos) * WSIZE
            size2 = hdr_size - size1
            pos1 = self.boffset + pos * WSIZE
            pos2 = self.boffset
            logging.debug(
                "Wrapped header %u %u %u %u", pos, hdr_words, self.buf_words, size1
            )

            hdr[0:size1] = slot[pos1 : pos1 + size1]
            hdr[size1:hdr_size] = slot[pos2 : pos2 + size2]
            header = ctypes.cast(hdr, ctypes.POINTER(DebugStreamRecord)).contents
        else:
            header = ctypes.cast(
                slot[self.boffset + pos * WSIZE :], ctypes.POINTER(DebugStreamRecord)
            ).contents
        if header.id > 100 or header.size_words >= self.buf_words:
            logging.info(
                "Broken record id %u seqno %u size %u",
                header.id,
                header.seqno,
                header.size_words,
            )
            return None
        return header

    def get_record(self, slot, pos, seqno):
        """
        Get record from position
        """
        rec = self.get_hdr(slot, pos)
        if rec is None or rec.size_words == 0:
            return None
        logging.debug(
            "got header at pos %u rec %u %u %u", pos, rec.id, rec.seqno, rec.size_words
        )
        if seqno is not None and rec.seqno != seqno:
            logging.warning(
                "Record seqno mismatch %u != %u, pos %u size %u",
                rec.seqno,
                seqno,
                pos,
                rec.size_words,
            )
            self.error_count = self.error_count + 1
            return None
        rwords = rec.size_words
        rsize = rec.size_words * WSIZE
        if pos + rwords > self.buf_words:
            record = (ctypes.c_ubyte * rsize)()
            size1 = (self.buf_words - pos) * WSIZE
            size2 = rsize - size1
            pos1 = self.boffset + pos * WSIZE
            pos2 = self.boffset
            logging.debug(
                "Wrapped record %u %u %u %u", pos, rsize, self.buf_words, size1
            )

            record[0:size1] = slot[pos1 : pos1 + size1]
            record[size1:rsize] = slot[pos2 : pos2 + size2]
        else:
            record = (ctypes.c_ubyte * rsize).from_buffer_copy(
                slot, self.boffset + pos * WSIZE
            )
        logging.info("got %u", rec.seqno)
        self.error_count = 0
        return record

    def catch_up(self, slot):
        """
        Search backwards from w_ptr for valid records
        """
        circ = CircularBufHdr.from_buffer_copy(slot, self.desc.offset)
        if circ.next_seqno == 0 or circ.w_ptr >= self.buf_words:
            return
        self.decode_past_records(slot, circ.w_ptr, circ.next_seqno)
        self.prev_w_ptr = circ.w_ptr
        self.prev_seqno = circ.next_seqno - 1
        logging.debug("seqno %u w_ptr %u", self.prev_seqno, self.prev_w_ptr)

    def decode_past_records(self, slot, pos, seqno):
        """
        Decode records backwards from pos. Should not be called directly, use catch_up()
        """
        if self.prev_seqno is not None and self.prev_seqno >= seqno - 1:
            return
        if pos == 0:
            spos = self.buf_words - 1
        else:
            spos = pos - 1
        bsize = ctypes.cast(
            slot[self.boffset + spos * 4 :], ctypes.POINTER(ctypes.c_uint)
        ).contents.value
        bpos = pos - bsize
        if bpos < 0:
            bpos = self.buf_words + pos - bsize
        rec = self.get_hdr(slot, bpos)
        if bsize != rec.size_words:
            return
        if seqno is not None:
            if rec.seqno != seqno - 1:
                return
        else:
            seqno = rec.seqno + 1

        self.decode_past_records(slot, bpos, seqno - 1)

        record = self.get_record(slot, bpos, seqno - 1)
        if record is not None:
            if not self.printer.print_record(record, self.cpu):
                logging.info("Parse failed on record %u", seqno - 1)
            logging.info("Printing %u success", seqno - 1)
        else:
            logging.info("Broken record %u", seqno - 1)

    def get_next_record(self, slot):
        """
        Get next record from the circular buffer and print it, returns True if a record
        with expected seqno number was found and successfully decoded and printed.
        """
        if self.prev_seqno is not None:
            record = self.get_record(slot, self.prev_w_ptr, self.prev_seqno + 1)
        else:
            record = self.get_record(slot, self.prev_w_ptr, None)
        if record is not None:
            print_success = self.printer.print_record(record, self.cpu)
            if print_success:
                recp = ctypes.cast(record, ctypes.POINTER(DebugStreamRecord))
                self.prev_w_ptr = (
                    self.prev_w_ptr + recp.contents.size_words
                ) % self.buf_words
                self.prev_seqno = recp.contents.seqno
            else:
                logging.info("Parse failed on record %u", self.prev_seqno + 1)
            return print_success
        self.error_count = self.error_count + 1
        logging.info("Record decoding failed %u", self.error_count)
        return False

    def poll_buffer(self, slot):
        """
        Poll for new records. If there were new records return True
        """
        circ = CircularBufHdr.from_buffer_copy(slot, self.desc.offset)
        if self.prev_w_ptr == circ.w_ptr:
            return False
        got_record = True
        while self.prev_w_ptr != circ.w_ptr and got_record:
            got_record = self.get_next_record(slot)
        return True

    def check_error_count(self):
        """
        Check if maximum error count was reached
        """
        if self.error_count > 3:
            return True
        return False


class DebugStreamDecoder:
    """
    Class for decoding debug-stream slot contents
    """

    file_size = DEBUG_SLOT_SIZE
    file = None
    slot = None
    descs = []
    circdec = []
    rec_printer = RecordPrinter()

    def set_file(self, file):
        """
        Ser file to read the slot contents from
        """
        self.file = file

    def update_slot(self):
        """
        Update slot contents
        """
        self.file.seek(0)
        self.slot = self.file.read(self.file_size)

    def set_slot(self, buf):
        """
        Update slot contents
        """
        self.slot = buf

    def get_descriptors(self):
        """
        Read the core specific descriptors and initialize core
        specific circular buffer decoders.

        """
        if self.slot is None:
            return False
        hdr = ctypes.cast(self.slot, ctypes.POINTER(DebugStreamSlotHdr))
        if hdr.contents.hdr.magic != DEBUG_STREAM_PAYLOAD_MAGIC:
            logging.info("Debug Slot has bad magic 0x%08x", hdr.contents.hdr.magic)
            return False
        num_sections = hdr.contents.num_sections
        if num_sections == len(self.descs):
            return True
        if num_sections > 32:
            logging.info("Suspiciously many sections %u", num_sections)
            return False
        hsize = ctypes.sizeof(DebugStreamSlotHdr)
        self.descs = (DebugStreamSectionDescriptor * num_sections).from_buffer_copy(
            self.slot, hsize
        )
        for i in range(len(self.descs)):
            if (self.descs[i].core_id > 32 or
                self.descs[i].buf_words > DEBUG_SLOT_SIZE // WSIZE or
                self.descs[i].offset > DEBUG_SLOT_SIZE):
             logging.info("Suspicious descriptor %u values %u %u %u", i,
                          self.descs[i].core_id, self.descs[i].buf_words,
                          self.descs[i].offset)
             return False
        self.circdec = [
            CircularBufferDecoder(self.descs[i], i, self.rec_printer)
            for i in range(len(self.descs))
        ]
        logging.info(
            "Header hdr_size %u total_size %u num_sections %u",
            hdr.contents.hdr.hdr_size,
            hdr.contents.total_size,
            hdr.contents.num_sections,
        )
        return True

    def catch_up_all(self):
        """
        Checks all circular buffers if there is records behind w_ptr. If there
        is valid seqno number available the decoding stops when there previous
        printed record is reached.
        """
        if len(self.descs) == 0 or self.slot is None:
            return
        for i in range(len(self.descs)):
            self.circdec[i].catch_up(self.slot)

    def poll(self):
        """
        Poll all circular buffers for more records. Returns True if nothing new
        was found and its time to go to sleep.
        """
        if len(self.descs) == 0 or self.slot is None:
            return False
        sleep = True
        for i in range(len(self.descs)):
            if self.circdec[i].poll_buffer(self.slot):
                sleep = False
        return sleep

    def check_slot(self):
        """
        Check if previously updated slot contents is valid
        """
        hdr = ctypes.cast(self.slot, ctypes.POINTER(DebugStreamSlotHdr))
        if hdr.contents.hdr.magic != DEBUG_STREAM_PAYLOAD_MAGIC:
            self.slot = None
            return False
        if hdr.contents.num_sections != len(self.descs):
            self.slot = None
            return False
        for i in range(len(self.descs)):
            if self.circdec[i].check_error_count():
                self.circdec[i] = CircularBufferDecoder(
                    self.descs[i], i, self.rec_printer
                )
        return True

    def reset(self):
        """
        Reset decoder
        """
        self.file = None
        self.slot = None

def cavstool_main_loop(my_args):
    import cavstool
    try:
        (hda, sd, dsp, hda_ostream_id) = cavstool.map_regs(True)
    except Exception as e:
        logging.error("Could not map device in sysfs; run as root?")
        logging.error(e)
        sys.exit(1)
    ADSP_DW_SLOT_DEBUG_STREAM = 0x53523134
    decoder = DebugStreamDecoder()
    while True:
        if not cavstool.fw_is_alive(dsp):
            cavstool.wait_fw_entered(dsp, timeout_s=None)
        if my_args.direct_access_slot < 0:
            offset = cavstool.debug_slot_offset_by_type(ADSP_DW_SLOT_DEBUG_STREAM)
            if offset is None:
                logging.error("Could not find debug_stream slot")
                sys.exit(1)
            logging.info("Got offset 0x%08x by type 0x%08x", offset,
                         ADSP_DW_SLOT_DEBUG_STREAM)
        else:
            offset = cavstool.debug_slot_offset(my_args.direct_access_slot)
        buf = cavstool.win_read(offset, 0, DEBUG_SLOT_SIZE)
        decoder.set_slot(buf)
        if not decoder.get_descriptors():
            time.sleep(my_args.update_interval)
            continue
        decoder.catch_up_all()
        while True:
            if decoder.poll():
                time.sleep(my_args.update_interval)
            buf = cavstool.win_read(offset, 0, DEBUG_SLOT_SIZE)
            decoder.set_slot(buf)
            if not decoder.check_slot():
                break

def main_f(my_args):
    """
    Open debug stream slot file and pass it to decoder

    This tool and the protocol it implements is for debugging and optimized
    to interfere with SOF DSP as little as possible, but not worrying too much
    about the host CPU load. That is why there where no synchronous mechanism
    done and the host simply polls for new records.
    """
    if my_args.direct_access_slot >= 0 or not os.path.isfile(my_args.debugstream_file):
	    return cavstool_main_loop(my_args)
    decoder = DebugStreamDecoder()
    prev_error = None
    while True:
        try:
            with open(my_args.debugstream_file, "rb") as file:
                decoder.set_file(file)
                decoder.update_slot()
                if not decoder.get_descriptors():
                    time.sleep(my_args.update_interval)
                    continue
                decoder.catch_up_all()
                while True:
                    if decoder.poll():
                        time.sleep(my_args.update_interval)
                    decoder.update_slot()
                    if not decoder.check_slot():
                        break

        except FileNotFoundError:
            print(f"File {my_args.debugstream_file} not found!")
            break
        except OSError as err:
            if str(err) != prev_error:
                print(f"Open {my_args.debugstream_file} failed '{err}'")
                prev_error = str(err)
        decoder.reset()
        time.sleep(args.update_interval)


def parse_params():
    """Parses parameters"""
    parser = argparse.ArgumentParser(description="SOF DebugStream thread info client. ")
    parser.add_argument(
        "-t",
        "--update-interval",
        type=float,
        help="Telemetry2 window polling interval in seconds, default 1",
        default=0.01,
    )
    parser.add_argument(
        "-f",
        "--debugstream-file",
        help="File to read the DebugStream data from, default /sys/kernel/debug/sof/debug_stream",
        default="/sys/kernel/debug/sof/debug_stream",
    )
    parser.add_argument(
        "-c",
        "--direct-access-slot",
        help="Access specified debug window slot directly, no need for debugfs file",
        type=int,
        default=-1,
    )
    parsed_args = parser.parse_args()
    return parsed_args


if __name__ == "__main__":
    args = parse_params()
    main_f(args)
