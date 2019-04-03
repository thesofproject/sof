#!/usr/bin/env python3

# Tool for processing FW stack dumps.
# For more detailed useage, use --help option.

from __future__  import print_function
import argparse
import struct
import os
import sys
import itertools
import re
import shutil
import time
from ctypes      import LittleEndianStructure, BigEndianStructure, c_uint32, c_char
from collections import namedtuple
from operator    import attrgetter
from functools   import partial

def stderr_print(*args, **kwargs):
	print(*args, file=sys.stderr, **kwargs)

def stdout_print(*args, **kwargs):
	print(*args, file=sys.stdout, **kwargs)

try:
	from sty import fg, bg, ef, rs, Rule, Render
	CAN_COLOUR=True
except:
	CAN_COLOUR=False

ArchDefinition = namedtuple('ArchDefinition', ['name', 'bitness', 'endianness'])
VALID_ARCHS = {}
[VALID_ARCHS.update({archdef.name : archdef})
	for archdef in [ArchDefinition(*tup) for tup in
	[
		( 'LE32bit',
			32,
			LittleEndianStructure, ),
		( 'LE64bit',
			64,
			LittleEndianStructure, ),
#		( 'BE32bit', #untested, treat as not implemented
#			32,
#			BigEndianStructure, ),
#		( 'BE64bit', #untested, treat as not implemented
#			64,
#			BigEndianStructure, ),
	]]
]

def valid_archs_print():
	archs = ''.join("{0}, ".format(x) for x in VALID_ARCHS)
	archs = archs[:len(archs)-2]
	return "{0}.".format(archs)

TERM_SIZE = shutil.get_terminal_size((120, 20))
AR_WINDOW_WIDTH = 4
IS_COLOUR=False

class argparse_readable_file( argparse.Action):
	def raise_error(self, filepath, reason):
		raise argparse.ArgumentTypeError(
			"is_readable_file:{0} {1}".format(
				filepath,
				reason
			))
	def is_readable_file(self, filepath):
		if not os.path.isfile(filepath):
			self.raise_error(filepath, "is not a valid path")
		if os.access(filepath, os.R_OK):
			return True
		else:
			self.raise_error(filepath, "is not a readable file")
		return False
	def __call__(self, parser, namespace, values, option_string=None):
		filepath = values[0]
		if (self.is_readable_file(filepath)):
			setattr(namespace, self.dest, filepath)

class argparse_writeable_file(argparse.Action):
	def raise_error(self, filepath, reason):
		raise argparse.ArgumentTypeError(
			"is_writeable_file:{0} {1}".format(
				filepath,
				reason
			))
	def is_writeable_file(self, filepath):
		absdir = os.path.abspath(os.path.dirname(filepath))
		if os.path.isdir(absdir) and not os.path.exists(filepath):
			return True
		else:
			if not os.path.isfile(filepath):
				self.raise_error(filepath, "is not a valid path")
			if os.access(filepath, os.W_OK):
				return True
			else:
				self.raise_error(filepath, "is not a writeable file")
		return False
	def __call__(self, parser, namespace, values, option_string=None):
		filepath = values[0]
		if (self.is_writeable_file(filepath)):
			setattr(namespace, self.dest, filepath)
		else:
			self.raise_error(
				filepath,
				"failed to determine whether file is writeable"
			)

class argparse_architecture(  argparse.Action):
	def raise_error(self, value, reason):
		raise argparse.ArgumentTypeError(
			"architecture: {0} {1}".format(
				value,
				reason
			))
	def is_valid_architecture(self, value):
		if value in VALID_ARCHS:
			return True
		return False
	def __call__(self, parser, namespace, values, option_string=None):
		value = values[0]
		if (self.is_valid_architecture(value)):
			setattr(namespace, self.dest, VALID_ARCHS[value])
		else:
			self.raise_error(
				value,
				"is invalid architecture. Valid architectures are: {0}".format(valid_archs_print())
			)

def parse_params():
	parser = argparse.ArgumentParser(
		description="Tool for processing FW stack dumps."
			+" In verbose mode it prints DSP registers, and function call"
			+" addresses in stack up to that which caused core dump."
			+" It then prints either to file or to stdin all gdb"
			+" commands unwrapping those function call addresses to"
			+" function calls in  human readable format."
	)
	ArgTuple = namedtuple('ArgTuple', ['name', 'optionals', 'parent'])
	ArgTuple.__new__.__defaults__ = ((), {}, parser)

	# below cannot be empty once declared
	outputMethod = parser.add_mutually_exclusive_group(required=True)
	inputMethod  = parser.add_mutually_exclusive_group()
	[parent.add_argument(*name, **optionals)
		for name, optionals, parent in sorted([ArgTuple(*x) for x in
			[
				( ( '-a', '--arch'       , ), {
						'type'   : str,
						'help'   :'determine architecture of dump file; valid archs are:  {0}'
							.format(valid_archs_print()),
						'action' : argparse_architecture,
						'nargs'  : 1,
						'default': VALID_ARCHS['LE64bit'],
					},),
				( ( '-v', '--verbose'    , ), {
						'help'  :'increase output verbosity',
						'action':'store_true',
					},),
				( (       '--stdin'      , ), {
						'help'   :'input is from stdin',
						'action' : 'store_true',
					},
					inputMethod),
				( ( '-o', '--outfile'    , ), {
						'type'   : str,
						'help'   :'output is to FILE',
						'action' : argparse_writeable_file,
						'nargs'  : 1,
					},
					outputMethod),
				( (       '--stdout'     , ), {
						'help'   :'output is to stdout',
						'action' :'store_true',
					},
					outputMethod),
				( ( '-c', '--colour', ), {
						'help'  :'set output to be colourful!',
						'action':'store_true',
					},),
				( ( '-l', '--columncount', ), {
						'type'  : int,
						'help'  :'set how many colums to group the output in, ignored without -v',
						'action':'store',
						'nargs' : 1,
					},),
				( ( '-i', '--infile'     , ), {
						'type'   : str,
						'help'   :'path to sys dump bin',
						'action' : argparse_readable_file,
						'nargs'  : 1,
					},
					inputMethod),
			]],
			key=lambda argtup: (argtup.parent.__hash__(), argtup.name)
		)
	]

	parsed = parser.parse_args()

	if parsed.columncount and not parsed.verbose:
		stderr_print("INFO: -l option will be ignored without -v")

	return parsed

def chunks(l, n):
	return [l[i:i + n] for i in range(0, len(l), n)]

def flaten(l):
	return [item for sublist in l for item in sublist]

def raiseIfArchNotValid(arch):
	if arch not in VALID_ARCHS.values():
		raise ValueError(
			"CoreDumpFactory: {0} not in valid architectures: {1}"
				.format(arch, valid_archs_print())
		)
	endiannesses = [arch.endianness for arch in VALID_ARCHS.values()]
	if arch.endianness not in endiannesses:
		raise ValueError(
			"CoreDumpFactory: {0} not in valid endiannesses: {1}"
				.format(endianness, endiannesses)
		)

def FileInfoFactory(arch, filename_length):
	raiseIfArchNotValid(arch)
	class FileInfo(arch.endianness):
		_fields_ = [
			("filename", filename_length * c_char),
			("line_no",  c_uint32)
		]
		def __str__(self):
			return "{}:{:d}".format(self.filename.decode(), self.line_no)
	if FileInfo is None:
		raise RuntimeError(
			"FileInfoFactory: failed to produce FileInfo({0})"
				.format(arch.name)
		)
	return FileInfo

class Colourer():
	#TODO: Add detection of 8bit/24bit terminal
	#      Add 8bit/24bit colours (with flag, maybe --colour=24bit)
	#      Use this below as fallback only
	__print = partial(stdout_print)
	if CAN_COLOUR == True:
		__style = {
			'o' : fg.red,
			'O' : fg.yellow,
			'y' : fg.blue,
			'Y' : fg.cyan,
			'D' : fg.white + bg.red,
		}
	matchings          = []
	matchingsInParenth = []
	matchingsInStderr  = []
	def __init__(self):
		if CAN_COLOUR == True:
			self.matchings = [
				(
					lambda x: self.enstyleNumHex(x.group()),
					re.compile(r'\b([A-Fa-f0-9]{8})\b')
				),
				(
					lambda x: '0x' + self.enstyleNumHex(x.group(2)),
					re.compile(r'(0x)([A-Fa-f0-9]{1,})')
				),
				(
					lambda x: self.enstyleNumBin(x.group()),
					re.compile(r'\b(b[01]+)\b')
				),
				(
					r'\1' +
					r'\2' +
					self.enstyle(           fg.green , r'\3') ,
					re.compile(r'(\#)(ar)([0-9]+)\b')
				),
				(
					self.enstyle(bg.green            , r'\1') +
					rs.all + '\n',
					re.compile(r'(\(xt-gdb\)\ *)')
				),
				(
					r'\1' +
					r'\2' +
					self.enstyle(           fg.green   , r'\3') +
					r':'  +
					self.enstyle(           fg.magenta , r'\4') ,
					re.compile(r'(\bat\b\ *)(.+/)?(.*):([0-9]+)')
				),
				(
					lambda x:\
						'in '+
						self.enstyle(fg.green,  x.group(2))+
						self.enstyleFuncParenth(x.group(3)),
					re.compile(r'(\bin\b\ *)([^\ ]+)\ *(\(.*\))')
				),
			]
			self.matchingsInParenth = [
				(
					self.enstyle(           fg.yellow   , r'\1') +
					self.enstyle(           fg.magenta  , r'=' ) +
					r'\2',
					re.compile(r'([\-_a-zA-Z0-9]+)\ *=\ *([^,]+)')
				),
				(
					self.enstyle(           fg.magenta  , r'\1') ,
					re.compile(r'(\ *[\(\)]\ *)')
				),
				(
					self.enstyle(           fg.magenta  , r', ') ,
					re.compile(r'(\ *,\ *)')
				),
			]
			self.matchingsInStderr = [
				(
					self.enstyle(bg.yellow  + fg.black    , r'\1') ,
					re.compile(r'([Ww]arning)')
				),
				(
					self.enstyle(bg.red     + fg.black    , r'\1') ,
					re.compile(r'([Ee]rror)')
				),
				(
					self.enstyle(bg.magenta + fg.black    , r'\1') ,
					re.compile(r'([Ff]atal)')
				),
			]

	def toGroup(self, txt):
		return [ (label, sum(1 for _ in group))
				for label, group in itertools.groupby(txt) ]

	def leadingZero(self, txt, char):
		result = ""
		groups = self.toGroup(txt)
		lead = 0
		if groups[0][0] == '0':
			lead = min(4, groups[0][1])
			result += char.lower() *    lead
		result +=     char.upper() * (4-lead)
		return result

	def findSub(self, txt, mask, sub, char):
		pos = txt.find(sub)
		if pos >= 0:
			return mask[:pos] + char * len(sub) + mask[(len(sub)+pos):]
		else:
			return mask

	def enstyleFuncParenth(self, txt):
		result = txt
		for repl, regex in self.matchingsInParenth:
			result = re.sub(regex, repl, result)
		return result 
		
	def enstyleNumBin(self, txt):
		result = rs.all + bg.magenta + "b"
		prev = ""
		for c in txt[1:]:
			if prev != c:
				prev = c
				result += rs.all
				if c == "0":
					result += fg.red
			result += c
		result += rs.all
		return result

	def enstyleNumHex(self, txt):
		if len(txt) < 8:
			txt = (8-len(txt))*'0' + txt
		p1 = 'o'
		p2 = 'y'
		if txt == "00000000":
			styleMask = p1 * 8
		elif txt.lower() == "deadbeef":
			styleMask = "DDDDDDDD"
		else:
			styleMask = "".join(
				[self.leadingZero(string, style)
				for string, style in [
					(txt[:4], p1),
					(txt[4:], p2),
			]])
			styleMask = "".join(
				[self.findSub(txt, styleMask, string, style)
				for string, style in [
					('dead', 'D'),
			]])

		result = ""
		thisstyle = ''
		for iter, style in enumerate(styleMask):
			if thisstyle != style:
				thisstyle = style
				result += rs.all + self.__style[thisstyle]
			result += txt[iter]
		result += rs.all
		return result

	def enstyleStderr(self, txt):
		if txt is None:
			return ''
		result = txt
		for repl, regex in self.matchingsInStderr:
			result = re.sub(regex, repl, result)
		for repl, regex in self.matchings:
			result = re.sub(regex, repl, result)
		return fg.red + result + rs.all

	def enstyle(self, style, txt):
		return style + txt + rs.all

	def produce_string(self, txt):
		result = txt
		for repl, regex in self.matchings:
			result = re.sub(regex, repl, result)
		return result

	def print(self, txt):
		self.__print(self.produce_string(txt))

def CoreDumpFactory(dsp_arch):
	raiseIfArchNotValid(dsp_arch)
	class CoreDump(dsp_arch.endianness):
		_fields_ = [(x, c_uint32) for x in
				[
					# struct sof_ipc_dsp_oops_arch_hdr {
					"arch",
					"totalsize",
					# }
					# struct sof_ipc_dsp_oops_plat_hdr {
					"configidhi",
					"configidlo",
					"numaregs",
					"stackoffset",
					# }
					"exccause",
					"excvaddr",
					"ps"
				]
				+ ["epc" + str(x) for x in range(1,7+1)]
				+ ["eps" + str(x) for x in range(2,7+1)]
				+ [
					"depc",
					"intenable",
					"interrupt",
					"sar",
					"debugcause",
					"windowbase",
					"windowstart",
					"excsave1" # to
				]
			] + [
				("a", dsp_arch.bitness * c_uint32)
			]

		def __init__(self, columncount):
			self.dsp_arch = dsp_arch
			self._fields_
			self.ar_regex = re.compile(r'ar[0-9]+')
			# below: smart column count
			self._longest_field = len(max([x[0] for x in self._fields_], key=len))
			if columncount is not None:
				self.columncount = max (1, int(columncount[0]))
			else:
				self.columncount = max(1,
					int(TERM_SIZE[0]/(self._longest_field + 2 + 2 * AR_WINDOW_WIDTH + 2))
				)
			self.columncount_ar = (
				self.columncount
					if self.columncount <= AR_WINDOW_WIDTH else
				AR_WINDOW_WIDTH * int(self.columncount/AR_WINDOW_WIDTH)
			)

		def __windowbase_shift(self, iter, direction):
			return (iter + self.windowbase * AR_WINDOW_WIDTH * direction)\
				% self.dsp_arch.bitness
		def windowbase_shift_left(self, iter):
			return self.__windowbase_shift(iter, -1)
		def windowbase_shift_right(self, iter):
			return self.__windowbase_shift(iter,  1)

		def reg_from_string(self, string):
			if self.ar_regex.fullmatch(string):
				return self.a[self.windowbase_shift_left(int(string[2:]))]
			else:
				return self.__getattribute__(string)

		def __str__(self):
			string = ""
			string += "exccause"
			return string

		def to_string(self, is_gdb):
			# in case windowbase in dump has invalid value
			windowbase_shift = min(
				self.windowbase * AR_WINDOW_WIDTH,
				self.dsp_arch.bitness
			)
			# flatten + chunk enable to smartly print in N columns
			string = ''.join([self.fmt(is_gdb, x)
				for x in flaten(
					[chunks(word, self.columncount) for word in [
						["arch", "totalsize", "stackoffset"],
						["configidhi", "configidlo", "numaregs"],
						["exccause", "excvaddr", "ps"],
						["epc" + str(x) for x in range(1,7+1)],
						["eps" + str(x) for x in range(2,7+1)],
						["depc", "intenable", "interrupt", "sar", "debugcause"],
						["windowbase", "windowstart"],
						["excsave1"],
					]] +
					[chunks(word, self.columncount_ar) for word in [
						["ar" + str(x) for x in itertools.chain(
							range(   windowbase_shift, self.dsp_arch.bitness),
							range(0, windowbase_shift),
						)]
					]]
				)
			])
			if not is_gdb:
				string += "\n"
			return string

		def fmt_gdb_command(self):
			return "set ${}=0x{:08x}\n"

		def fmt_pretty_form(self, separator = "|"):
			return separator + "{:" + str(self._longest_field) + "} {:08x} "

		def fmt_separator(self, name):
			separator = "# "
			return separator

		def fmt_pretty_auto(self, name):
			return self.fmt_pretty_form(self.fmt_separator(name))

		def fmt(self, is_gdb, names):
			if is_gdb:
				fmtr = lambda name: self.fmt_gdb_command()
			else:
				fmtr = lambda name: self.fmt_pretty_auto(name)

			string = ""
			for name in names:
				string += fmtr(name).format(
					name, self.reg_from_string(name)
				)
			if not is_gdb:
				string += "\n"
			return string

		def windowstart_process(self):
			string = ""
			binary = "b{0:b}".format(self.windowstart)
			fnc_num = 0
			header = " "
			for it, c in enumerate(binary[1:]):
				if c != "0":
					header  += str(fnc_num)
					fnc_num += 1
				else:
					header += " "
			string += "#              {0}\n".format(header)
			string += "# windowstart: {0}\n".format(binary)

			fnc_num = 0
			for iter, digit in enumerate(binary[1:]):
				if (digit == '1'):
					reg = "ar{0}".format(
						self.windowbase_shift_right(AR_WINDOW_WIDTH * -iter)
					)
					string  += "# {0:2d} ".format(++fnc_num)
					string  += self.fmt_pretty_auto(reg).format(
						reg, self.reg_from_string(reg)
					) + "\n"
					fnc_num += 1
			return string

	if CoreDump is None:
		raise RuntimeError(
			"CoreDumpFactory: failed to produce CoreDump({0})"
				.format(dsp_arch.name)
		)
	return CoreDump

class CoreDumpReader(object):
	def __init__(self, args):
		self.core_dump    = CoreDumpFactory(args.arch)(
			args.columncount
		)
		self.file_info    = FileInfoFactory(args.arch, 32)()

		if IS_COLOUR:
			colourer = Colourer()
		if args.verbose:
			verbosePrint =\
					colourer.print\
				if IS_COLOUR else\
					stdout_print
		else:
			verbosePrint = lambda *discard_this: None

		if   args.stdout:
			stdoutOpen  = lambda: None
			stdoutPrint = print
			stdoutClose = lambda: None
		elif args.outfile:
			#TODO: open file in stdOutOpen
			stdoutDest  = open(args.outfile, "w")
			stdoutOpen  = lambda: None
			stdoutPrint = stdoutDest.write
			stdoutClose = stdoutDest.close
		else:
			raise RuntimeError("CoreDumpReader: No output method.") 

		if   args.stdin:
			inStream = lambda: sys.stdin.buffer
		elif args.infile:
			inStream = lambda: open(args.infile, "rb")
		else:
			raise RuntimeError("CoreDumpReader: No input method.") 

		with inStream() as cd_file:
			[cd_file.readinto(x) for x in [
				self.core_dump,
				self.file_info
			]]
			self.stack = cd_file.read()

		verbosePrint(self.core_dump.to_string(0))

		verbosePrint(self.core_dump.windowstart_process())

		verbosePrint("# Location: " + str(self.file_info));
		stack_base = self.core_dump.a[1] + 16
		stack_dw_num = int(len(self.stack)/AR_WINDOW_WIDTH)
		verbosePrint("# Stack dumped from {:08x} dwords num {:d}"
			.format(stack_base, stack_dw_num))

		stdoutOpen()
		stdoutPrint("break *0xbefe0000\nrun\n")
		stdoutPrint(self.core_dump.to_string(1))

		#TODO: make this elegant
		for dw, addr in [(
			struct.unpack("I", self.stack[i*AR_WINDOW_WIDTH : (i+1)*AR_WINDOW_WIDTH])[0],
			stack_base + i*AR_WINDOW_WIDTH
		) for i in range(0, stack_dw_num)]:
			stdoutPrint("set *0x{:08x}=0x{:08x}\n"
				.format(addr, dw))

		# TODO: if excsave1 is not empty, pc should be set to that value
		# (exception mode, not forced panic mode)
			# dodac sobie przyklad
			# ustawiac pc z
		stdoutPrint("set $pc=&arch_dump_regs_a\nbacktrace\n")
		stdoutClose()

if __name__ == "__main__":
	args = parse_params()
	if args.colour:
		if CAN_COLOUR:
			IS_COLOUR=True
		else:
			stderr_print("Cannot color the output: module 'sty' not found!")
	CoreDumpReader(args)

