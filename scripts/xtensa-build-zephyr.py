#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# Too much noise for now, these can be re-enabled after they've been
# fixed (if that does not break `git blame` too much)

# W0311, W0312, W0603
# pylint:disable=bad-indentation
# pylint:disable=mixed-indentation
# pylint:disable=global-statement

# C0103, C0114, C0116
# pylint:disable=invalid-name
# pylint:disable=missing-module-docstring
# pylint:disable=missing-function-docstring

# Non-indentation whitespace has been removed from newer pylint. It does
# not hurt to keep them for older versions. The recommendation is to use
# a formatter like `black` instead, unfortunately this would totally
# destroy git blame, git revert, etc.

# C0326, C0330
# pylint:disable=bad-whitespace
# pylint:disable=bad-continuation

import argparse
import shlex
import subprocess
import pathlib
import errno
import platform as py_platform
import sys
import shutil
import os
import warnings
import fnmatch
import hashlib
import json
import gzip
import dataclasses
import concurrent.futures as concurrent

from west import configuration as west_config

# anytree module is defined in Zephyr build requirements
from anytree import AnyNode, RenderTree, render
from packaging import version

# https://chrisyeh96.github.io/2017/08/08/definitive-guide-python-imports.html#case-3-importing-from-parent-directory
sys.path.insert(1, os.path.join(sys.path[0], '..'))
from tools.sof_ri_info import sof_ri_info

MIN_PYTHON_VERSION = 3, 8
assert sys.version_info >= MIN_PYTHON_VERSION, \
	f"Python {MIN_PYTHON_VERSION} or above is required."

# Version of this script matching Major.Minor.Patch style.
VERSION = version.Version("2.0.0")

# Constant value resolves SOF_TOP directory as: "this script directory/.."
SOF_TOP = pathlib.Path(__file__).parents[1].resolve()
west_top = pathlib.Path(SOF_TOP, "..").resolve()

sof_fw_version = None

if py_platform.system() == "Windows":
	xtensa_tools_version_postfix = "-win32"
elif py_platform.system() == "Linux":
	xtensa_tools_version_postfix = "-linux"
else:
	xtensa_tools_version_postfix = "-unsupportedOS"
	warnings.warn(f"Your operating system: {py_platform.system()} is not supported")


@dataclasses.dataclass
# pylint:disable=too-many-instance-attributes
class PlatformConfig:
	"Product parameters"
	name: str
	PLAT_CONFIG: str
	XTENSA_TOOLS_VERSION: str
	XTENSA_CORE: str
	DEFAULT_TOOLCHAIN_VARIANT: str = "xt-clang"
	RIMAGE_KEY: pathlib.Path = pathlib.Path(SOF_TOP, "keys", "otc_private_key_3k.pem")
	IPC4_RIMAGE_DESC: str = None
	IPC4_CONFIG_OVERLAY: str = "ipc4_overlay.conf"
	aliases: list = dataclasses.field(default_factory=list)

platform_configs = {
	#  Intel platforms
	"tgl" : PlatformConfig(
		"tgl", "intel_adsp_cavs25",
		f"RG-2017.8{xtensa_tools_version_postfix}",
		"cavs2x_LX6HiFi3_2017_8",
		"xcc",
		IPC4_RIMAGE_DESC = "tgl-cavs.toml",
		aliases = ['adl', 'ehl']
	),
	"tgl-h" : PlatformConfig(
		"tgl-h", "intel_adsp_cavs25_tgph",
		f"RG-2017.8{xtensa_tools_version_postfix}",
		"cavs2x_LX6HiFi3_2017_8",
		"xcc",
		IPC4_RIMAGE_DESC = "tgl-h-cavs.toml",
		aliases = ['adl-s']
	),
	"mtl" : PlatformConfig(
		"mtl", "intel_adsp_ace15_mtpm",
		f"RI-2022.10{xtensa_tools_version_postfix}",
		"ace10_LX7HiFi4_2022_10",
	),
	"lnl" : PlatformConfig(
		"lnl", "intel_adsp_ace20_lnl",
		f"RI-2022.10{xtensa_tools_version_postfix}",
		"ace10_LX7HiFi4_2022_10",
	),

	#  NXP platforms
	"imx8" : PlatformConfig(
		"imx8", "nxp_adsp_imx8",
		f"RI-2023.11{xtensa_tools_version_postfix}",
		"hifi4_nxp_v5_3_1_prod",
		RIMAGE_KEY = "key param ignored by imx8",
	),
	"imx8x" : PlatformConfig(
		"imx8x", "nxp_adsp_imx8x",
		f"RI-2023.11{xtensa_tools_version_postfix}",
		"hifi4_nxp_v5_3_1_prod",
		RIMAGE_KEY = "key param ignored by imx8x"
	),
	"imx8m" : PlatformConfig(
		"imx8m", "nxp_adsp_imx8m",
		f"RI-2023.11{xtensa_tools_version_postfix}",
		"hifi4_mscale_v2_0_2_prod",
		RIMAGE_KEY = "key param ignored by imx8m"
	),
}

platform_names = list(platform_configs)

class validate_platforms_arguments(argparse.Action):
	"""Validates positional platform arguments whether provided platform name is supported."""
	def __call__(self, parser, namespace, values, option_string=None):
		if values:
			for value in values:
				if value not in platform_names:
					raise argparse.ArgumentError(self, f"Unsupported platform: {value}")
		setattr(namespace, "platforms", values)

args = None
def parse_args():
	global args
	global west_top
	parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter,
			epilog=("This script supports XtensaTools but only when installed in a specific\n" +
				"directory structure, example:\n" +
				"myXtensa/\n" +
				"└── install/\n" +
				"	├── builds/\n" +
				"	│   ├── RD-2012.5{}/\n".format(xtensa_tools_version_postfix) +
				"	│   │   └── Intel_HiFiEP/\n" +
				"	│   └── RG-2017.8{}/\n".format(xtensa_tools_version_postfix) +
				"	│  		├── LX4_langwell_audio_17_8/\n" +
				"	│   	└── X4H3I16w2D48w3a_2017_8/\n" +
				"	└── tools/\n" +
				"			├── RD-2012.5{}/\n".format(xtensa_tools_version_postfix) +
				"			│   └── XtensaTools/\n" +
				"			└── RG-2017.8{}/\n".format(xtensa_tools_version_postfix) +
				"				└── XtensaTools/\n" +
			"$XTENSA_TOOLS_ROOT=/path/to/myXtensa ...\n" +
			f"Supported platforms {platform_names}"))

	parser.add_argument("-a", "--all", required=False, action="store_true",
						help="Build all currently supported platforms")
	parser.add_argument("platforms", nargs="*", action=validate_platforms_arguments,
						help="List of platforms to build")
	parser.add_argument("-d", "--debug", required=False, action="store_true",
						help="Shortcut for: -o sof/app/debug_overlay.conf")
    # NO SOF release will ever user the option --fw-naming.
    # This option is only for disguising SOF IPC4 as CAVS IPC4 and only in cases where
    # the kernel 'ipc_type' expects CAVS IPC4. In this way, developers and CI can test
    # IPC4 on older platforms.
	parser.add_argument("--fw-naming", required=False, choices=["AVS", "SOF"],
						default="SOF", help="""
Determine firmware naming conversion and folder structure
For SOF:
    /lib/firmware/intel/sof
    └───────community
        │   └── sof-tgl.ri
        ├── dbgkey
        │   └── sof-tgl.ri
        └── sof-tgl.ri
For AVS(filename dsp_basefw.bin):
Noted that with fw_naming set as 'AVS', there will be output subdirectories for each platform
    /lib/firmware/intel/sof-ipc4
    └── tgl
        ├── community
        │   └── dsp_basefw.bin
        ├── dbgkey
        │   └── dsp_basefw.bin
        └── dsp_basefw.bin"""
	)
	parser.add_argument("-j", "--jobs", required=False, type=int,
						help="Number of concurrent jobs. Passed to west build and"
						" to cmake (for rimage)")
	parser.add_argument("-k", "--key", type=pathlib.Path, required=False,
						help="Path to a non-default rimage signing key.")
	parser.add_argument("-o", "--overlay", type=pathlib.Path, required=False, action='append',
						default=[], help="Paths to overlays")
	parser.add_argument("-p", "--pristine", required=False, action="store_true",
						help="Perform pristine build removing build directory.")
	parser.add_argument("-u", "--update", required=False, action="store_true",
		help="""Runs west update command - clones SOF dependencies. Downloads next to this sof clone a new Zephyr
project with its required dependencies. Creates a modules/audio/sof symbolic link pointing
back at this sof clone.  All projects are checkout out to
revision defined in manifests of SOF and Zephyr.""")
	parser.add_argument('-v', '--verbose', default=0, action='count',
			    help="""Verbosity level. Repetition of the flag increases verbosity.
The same number of '-v' is passed to "west".
""",
	)
	# Cannot use a standard -- delimiter because argparse deletes it.
	parser.add_argument("-C", "--cmake-args", action='append', default=[],
			    help="""Cmake arguments passed as is to cmake configure step.
Can be passed multiple times; whitespace is preserved Example:

     -C=--warn-uninitialized  -C '-DEXTRA_FLAGS=-Werror -g0'

Note '-C --warn-uninitialized' is not supported by argparse, an equal
sign must be used (https://bugs.python.org/issue9334)""",
	)

	parser.add_argument("--key-type-subdir", default="community",
			    choices=["community", "none", "dbgkey"],
			    help="""Output subdirectory for rimage signing key type.
Default key type subdirectory is \"community\".""")


	parser.add_argument("--use-platform-subdir", default = False,
			    action="store_true",
			    help="""Use an output subdirectory for each platform.
Otherwise, all firmware files are installed in the same staging directory by default.""")

	parser.add_argument("--no-interactive", default=False, action="store_true",
			    help="""Run script in non-interactive mode when user input can not be provided.
This should be used with programmatic script invocations (eg. Continuous Integration).
				""")
	parser.add_argument("--version", required=False, action="store_true",
			    help="Prints version of this script.")

	args = parser.parse_args()

	if args.all:
		args.platforms = platform_names

	# print help message if no arguments provided
	if len(sys.argv) == 1:
			parser.print_help()
			sys.exit(0)

	if args.fw_naming == 'AVS':
		if not args.use_platform_subdir:
			args.use_platform_subdir=True
			warnings.warn("The option '--fw-naming AVS' has to be used with '--use-platform-subdir'. Enable '--use-platform-subdir' automatically.")

def execute_command(*run_args, **run_kwargs):
	"""[summary] Provides wrapper for subprocess.run that prints
	command executed when 'more verbose' verbosity level is set."""
	command_args = run_args[0]

	# If you really need the shell in some non-portable section then
	# invoke subprocess.run() directly.
	if run_kwargs.get('shell') or not isinstance(command_args, list):
		raise RuntimeError("Do not rely on non-portable shell parsing")

	if args.verbose >= 0:
		cwd = run_kwargs.get('cwd')
		print_cwd = f"In dir: {cwd}" if cwd else f"in current dir: {os.getcwd()}"
		print_args = shlex.join(command_args)
		output = f"{print_cwd}; running command:\n    {print_args}"
		env_arg = run_kwargs.get('env')
		env_change = set(env_arg.items()) - set(os.environ.items()) if env_arg else None
		if env_change and (run_kwargs.get('sof_log_env') or args.verbose >= 1):
			output += "\n... with extra/modified environment:\n"
			for k_v in env_change:
				output += f"{k_v[0]}={k_v[1]}\n"
		print(output, flush=True)

	run_kwargs = {k: run_kwargs[k] for k in run_kwargs if not k.startswith("sof_")}

	if not 'check' in run_kwargs:
		run_kwargs['check'] = True
	#pylint:disable=subprocess-run-check

	return subprocess.run(*run_args, **run_kwargs)

def symlink_or_copy(directory, origbase, newbase):
	"""Create a symbolic link or copy in the same directory. Don't
	bother Windows users with symbolic links because they require
	special privileges. Windows don't care about /lib/firmware/sof/
	anyway. Make a copy instead to preserve cross-platform consistency.

	"""
	new = pathlib.Path(directory) / newbase
	if py_platform.system() == "Windows":
		shutil.copy2(pathlib.Path(directory) / origbase, new)
	else:
		new.symlink_to(origbase)

def show_installed_files():
	"""[summary] Scans output directory building binary tree from files and folders
	then presents them in similar way to linux tree command."""
	graph_root = AnyNode(name=STAGING_DIR.name, long_name=".", parent=None)
	relative_entries = [
		entry.relative_to(STAGING_DIR) for entry in sorted(STAGING_DIR.glob("**/*"))
	]
	nodes = [ graph_root ]
	for entry in relative_entries:
		# Node's documentation does allow random attributes
		# pylint: disable=no-member
		# sorted() makes sure our parent is already there.
		# This is slightly awkward, a recursive function would be more readable
		matches = [node for node in nodes if node.long_name == str(entry.parent)]
		assert len(matches) == 1, f'"{entry}" does not have exactly one parent'
		nodes.append(AnyNode(name=entry.name, long_name=str(entry), parent=matches[0]))

	for pre, _, node in RenderTree(graph_root, render.AsciiStyle):
		fpath = STAGING_DIR / node.long_name
		stem = node.name[:-3] if node.name.endswith(".gz") else node.name

		shasum_trailer = ""
		if checksum_wanted(stem) and fpath.is_file() and not fpath.is_symlink():
			shasum_trailer =  "\tsha256=" + checksum(fpath)

		print(f"{pre}{node.name} {shasum_trailer}")


# TODO: among other things in this file it should be less SOF-specific;
# try to move as much as possible to generic Zephyr code. See
# discussions in https://github.com/zephyrproject-rtos/zephyr/pull/51954
def checksum_wanted(stem):
	for pattern in CHECKSUM_WANTED:
		if fnmatch.fnmatch(stem, pattern):
			return True
	return False


def checksum(fpath):
	if fpath.suffix == ".gz":
		inputf = gzip.GzipFile(fpath, "rb")
	else:
		inputf = open(fpath, "rb")
	chksum = hashlib.sha256(inputf.read()).hexdigest()
	inputf.close()
	return chksum


def check_west_installation():
	west_path = shutil.which("west")
	if not west_path:
		raise FileNotFoundError("Install west and a west toolchain,"
			"https://docs.zephyrproject.org/latest/getting_started/index.html")
	print(f"Found west: {west_path}")

def west_reinitialize(west_root_dir: pathlib.Path, west_manifest_path: pathlib.Path):
	"""[summary] Performs west reinitialization to SOF manifest file asking user for permission.
	Prints error message if script is running in non-interactive mode.

	:param west_root_dir: directory where is initialized.
	:type west_root_dir: pathlib.Path
	:param west_manifest_path: manifest file to which west is initialized.
	:type west_manifest_path: pathlib.Path
	:raises RuntimeError: Raised when west is initialized to wrong manifest file
	(not SOFs manifest) and script is running in non-interactive mode.
	"""
	global west_top
	message = "West is initialized to manifest other than SOFs!\n"
	message +=  f"Initialized to manifest: {west_manifest_path}." + "\n"
	dot_west_directory  = pathlib.Path(west_root_dir.resolve(), ".west")
	if args.no_interactive:
		message += f"Try deleting {dot_west_directory } directory and rerun this script."
		raise RuntimeError(message)
	question = message + "Reinitialize west to SOF manifest? [Y/n] "
	print(f"{question}")
	while True:
		reinitialize_answer = input().lower()
		if reinitialize_answer in ["y", "n"]:
			break
		sys.stdout.write('Please respond with \'Y\' or \'n\'.\n')

	if reinitialize_answer != 'y':
		sys.exit("Can not proceed. Reinitialize your west manifest to SOF and rerun this script.")
	shutil.rmtree(dot_west_directory)
	execute_command(["west", "init", "-l", f"{SOF_TOP}"], cwd=west_top)


# TODO: use west APIs directly instead of all these indirect subprocess.run("west", ...) processes
def west_init_if_needed():
	"""[summary] Validates whether west workspace had been initialized and points to SOF manifest.
	Peforms west initialization if needed.
	"""
	global west_top, SOF_TOP
	west_manifest_path = pathlib.Path(SOF_TOP, "west.yml")
	result_rootdir = execute_command(["west", "topdir"], capture_output=True, cwd=west_top,
		timeout=10, check=False)
	if result_rootdir.returncode != 0:
		execute_command(["west", "init", "-l", f"{SOF_TOP}"], cwd=west_top)
		return
	west_root_dir = pathlib.Path(result_rootdir.stdout.decode().rstrip()).resolve()
	result_manifest_dir = execute_command(["west", "config", "manifest.path"], capture_output=True,
		cwd=west_top, timeout=10, check=True)
	west_manifest_dir = pathlib.Path(west_root_dir, result_manifest_dir.stdout.decode().rstrip()).resolve()
	manifest_file_result = execute_command(["west", "config", "manifest.file"], capture_output=True,
		cwd=west_top, timeout=10, check=True)
	returned_manifest_path = pathlib.Path(west_manifest_dir, manifest_file_result.stdout.decode().rstrip())
	if not returned_manifest_path.samefile(west_manifest_path):
		west_reinitialize(west_root_dir, returned_manifest_path)
	else:
		print(f"West workspace: {west_root_dir}")
		print(f"West manifest path: {west_manifest_path}")

def create_zephyr_directory():
	global west_top
	# Do not fail when there's only an empty directory left over
	# (because of some early interruption of this script or proxy
	# misconfiguration, etc.)
	try:
		# rmdir() is safe: it deletes empty directories ONLY.
		west_top.rmdir()
	except OSError as oserr:
		if oserr.errno not in [errno.ENOTEMPTY, errno.ENOENT]:
			raise oserr
		# else when not empty then let the next line fail with a
		# _better_ error message:
		#         "zephyrproject already exists"

	west_top.mkdir(parents=False, exist_ok=False)
	west_top = west_top.resolve(strict=True)

def create_zephyr_sof_symlink():
	global west_top, SOF_TOP
	if not west_top.exists():
		raise FileNotFoundError("No west top: {}".format(west_top))
	audio_modules_dir = pathlib.Path(west_top, "modules", "audio")
	audio_modules_dir.mkdir(parents=True, exist_ok=True)
	sof_symlink = pathlib.Path(audio_modules_dir, "sof")
	# Symlinks creation requires administrative privileges in Windows or special user rights
	try:
		if not sof_symlink.exists():
			sof_symlink.symlink_to(SOF_TOP, target_is_directory=True)
	except:
		print(f"Failed to create symbolic link: {sof_symlink} to {SOF_TOP}."
			"\nIf you run script on Windows run it with administrative privileges or\n"
			"grant user symlink creation rights -"
			"see: https://docs.microsoft.com/en-us/windows/security/threat-protection/"
			"security-policy-settings/create-symbolic-links")
		raise

def west_update():
	"""[summary] Clones all west manifest projects to specified revisions"""
	global west_top
	execute_command(["west", "update"], check=True, timeout=3000, cwd=west_top)


def get_sof_version():
	"""[summary] Get version string major.minor.micro of SOF
	firmware file. When building multiple platforms from the same SOF
	commit, all platforms share the same version. So for the 1st platform,
	extract the version information from sof/versions.json and later platforms will
	reuse it.
	"""
	global sof_fw_version
	if sof_fw_version:
		return sof_fw_version

	versions = {}
	with open(SOF_TOP / "versions.json") as versions_file:
		versions = json.load(versions_file)
		# Keep this default value the same as the default SOF_MICRO in version.cmake
		sof_micro = versions['SOF'].get('MICRO', "0")
		sof_fw_version = (
			f"{versions['SOF']['MAJOR']}.{versions['SOF']['MINOR']}.{sof_micro}"
		)
	return sof_fw_version

def rmtree_if_exists(directory):
	"This is different from ignore_errors=False because it deletes everything or nothing"
	if os.path.exists(directory):
		shutil.rmtree(directory)

def clean_staging(platform):
	print(f"Cleaning {platform} from {STAGING_DIR}")

	rmtree_if_exists(STAGING_DIR / "sof-info" / platform)

	sof_output_dir = STAGING_DIR / "sof"

	# --use-platform-subdir
	rmtree_if_exists(sof_output_dir / platform)

	# Remaining .ri and .ldc files
	for p in [ platform ] + platform_configs[platform].aliases:
		for f in sof_output_dir.glob(f"**/sof-{p}.*"):
			os.remove(f)


RIMAGE_BUILD_DIR  = west_top / "build-rimage"

# Paths in `west.yml` must be "static", we cannot have something like a
# variable "$my_sof_path/rimage/" checkout.  In the future "rimage/" will
# be moved one level up and it won't be nested inside "sof/" anymore. But
# for now we must stick to `sof/rimage/[tomlc99]` for
# backwards-compatibility with XTOS platforms and git submodules, see more
# detailed comments in west.yml
RIMAGE_SOURCE_DIR = west_top / "sof" / "tools" / "rimage"


def rimage_west_configuration(platform_dict, dest_dir):
	"""Configure rimage in a new file `dest_dir/westconfig.ini`, starting
	from the workspace .west/config.
	Returns the pathlib.Path to the new file.
	"""

	saved_local_var = os.environ.get('WEST_CONFIG_LOCAL')
	workspace_west_config_path = os.environ.get('WEST_CONFIG_LOCAL',
						   str(west_top / ".west" / "config"))
	platform_west_config_path = dest_dir / "westconfig.ini"
	dest_dir.mkdir(parents=True, exist_ok=True)
	shutil.copyfile(workspace_west_config_path, platform_west_config_path)

	# Create `platform_wconfig` object pointing at our copy
	os.environ['WEST_CONFIG_LOCAL'] = str(platform_west_config_path)
	platform_wconfig = west_config.Configuration()
	if saved_local_var is None:
		del os.environ['WEST_CONFIG_LOCAL']
	else:
		os.environ['WEST_CONFIG_LOCAL'] = saved_local_var

	# By default, run rimage directly from the rimage build directory
	if platform_wconfig.get("rimage.path") is None:
		rimage_executable = shutil.which("rimage", path=RIMAGE_BUILD_DIR)
		assert pathlib.Path(str(rimage_executable)).exists()
		platform_wconfig.set("rimage.path", shlex.quote(rimage_executable),
				     west_config.ConfigFile.LOCAL)

	_ws_args = platform_wconfig.get("rimage.extra-args")
	workspace_extra_args = [] if _ws_args is None else shlex.split(_ws_args)

	# Flatten default rimage options while giving precedence to the workspace =
	# the user input. We could just append and leave duplicates but that would be
	# at best confusing and at worst relying on undocumented rimage precedence.
	extra_args = []
	for default_opt in rimage_options(platform_dict):
		if not default_opt[0] in workspace_extra_args:
			extra_args += default_opt

	extra_args += workspace_extra_args

	platform_wconfig.set("rimage.extra-args", shlex.join(extra_args))

	return platform_west_config_path


def build_rimage():

	old_rimage_loc = SOF_TOP / "rimage"
	# Don't warn on empty directories
	if ( old_rimage_loc  / "CMakeLists.txt" ).exists():
		warnings.warn(f"""{old_rimage_loc} is now ignored,
		new location is {RIMAGE_SOURCE_DIR}"""
		)
	rimage_dir_name = RIMAGE_BUILD_DIR.name
	# CMake build rimage module
	if not (RIMAGE_BUILD_DIR / "CMakeCache.txt").is_file():
		execute_command(["cmake", "-B", rimage_dir_name, "-G", "Ninja",
				 "-S", str(RIMAGE_SOURCE_DIR)],
				cwd=west_top)
	rimage_build_cmd = ["cmake", "--build", rimage_dir_name]
	if args.jobs is not None:
		rimage_build_cmd.append(f"-j{args.jobs}")
	if args.verbose > 1:
			rimage_build_cmd.append("-v")
	execute_command(rimage_build_cmd, cwd=west_top)


def rimage_options(platform_dict):
	"""Return a list of default rimage options as a list of tuples,
	example: [ (-f, 2.5.0), (-b, 1), (-k, key.pem),... ]

	"""
	opts = []

	if args.verbose > 0:
		opts.append(("-v",) * args.verbose)

	signing_key = None
	if args.key:
		key_path = pathlib.Path(args.key)
		assert key_path.exists(), f"{key_path} not found"
		signing_key = key_path.resolve()
	elif "RIMAGE_KEY" in platform_dict:
		signing_key = platform_dict["RIMAGE_KEY"]

	if signing_key is not None:
		opts.append(("-k", str(signing_key)))

	sof_fw_vers = get_sof_version()

	opts.append(("-f", sof_fw_vers))

	# Default value is 0 in rimage but for Zephyr the "build counter" has always
	# been hardcoded to 1 in CMake and there is even a (broken) test that fails
	# when it's not hardcoded to 1.
	# FIXME: drop this line once the following test is fixed
	# tests/avs/fw_00_basic/test_01_load_fw_extended.py::TestLoadFwExtended::()::
	#                         test_00_01_load_fw_and_check_version
	opts.append(("-b", "1"))

	if platform_dict.get("IPC4_RIMAGE_DESC", None) is not None:
		rimage_desc = platform_dict["IPC4_RIMAGE_DESC"]
	else:
		rimage_desc = platform_dict["name"] + ".toml"

	opts.append(("-c", str(RIMAGE_SOURCE_DIR / "config" / rimage_desc)))

	return opts


STAGING_DIR = None
def build_platforms():
	global west_top, SOF_TOP
	print(f"SOF_TOP={SOF_TOP}")
	print(f"west_top={west_top}")

	global STAGING_DIR
	STAGING_DIR = pathlib.Path(west_top, "build-sof-staging")
	# Don't leave the install of an old build behind
	if args.pristine:
		rmtree_if_exists(STAGING_DIR)
	else:
		# This is important in (at least) two use cases:
		# - when switching `--use-platform-subdir` on/off or changing key subdir,
		# - when the build starts failing after a code change.
		# Do not delete platforms that were not requested so this script can be
		# invoked once per platform.
		for platform in args.platforms:
			clean_staging(platform)
		rmtree_if_exists(STAGING_DIR / "tools")


	# smex does not use 'install -D'
	sof_output_dir = pathlib.Path(STAGING_DIR, "sof")
	sof_output_dir.mkdir(parents=True, exist_ok=True)
	for platform in args.platforms:
		platf_build_environ = os.environ.copy()
		if args.use_platform_subdir:
			sof_platform_output_dir = pathlib.Path(sof_output_dir, platform)
			sof_platform_output_dir.mkdir(parents=True, exist_ok=True)
		else:
			sof_platform_output_dir = sof_output_dir

		# For now convert the new dataclass to what it used to be
		_dict = dataclasses.asdict(platform_configs[platform])
		platform_dict = { k:v for (k,v) in _dict.items() if _dict[k] is not None }

		xtensa_tools_root_dir = os.getenv("XTENSA_TOOLS_ROOT")
		# when XTENSA_TOOLS_ROOT environmental variable is set,
		# use user installed Xtensa tools not Zephyr SDK
		if "XTENSA_TOOLS_VERSION" in platform_dict and xtensa_tools_root_dir:
			xtensa_tools_root_dir = pathlib.Path(xtensa_tools_root_dir)
			if not xtensa_tools_root_dir.is_dir():
				raise RuntimeError(f"Platform {platform} uses Xtensa toolchain."
					"\nVariable XTENSA_TOOLS_VERSION points path that does not exist\n"
					"or is not a directory")

			# set variables expected by zephyr/cmake/toolchain/xcc/generic.cmake
			platf_build_environ["ZEPHYR_TOOLCHAIN_VARIANT"] = platf_build_environ.get("ZEPHYR_TOOLCHAIN_VARIANT",
				platform_dict["DEFAULT_TOOLCHAIN_VARIANT"])
			XTENSA_TOOLCHAIN_PATH = str(pathlib.Path(xtensa_tools_root_dir, "install",
				"tools").absolute())
			platf_build_environ["XTENSA_TOOLCHAIN_PATH"] = XTENSA_TOOLCHAIN_PATH
			TOOLCHAIN_VER = platform_dict["XTENSA_TOOLS_VERSION"]
			XTENSA_CORE = platform_dict["XTENSA_CORE"]
			platf_build_environ["TOOLCHAIN_VER"] = TOOLCHAIN_VER

			# Set variables expected by xcc toolchain. CMake cannot set (evil) build-time
			# environment variables at configure time:
			# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#how-can-i-get-or-set-environment-variables
			XTENSA_BUILDS_DIR=str(pathlib.Path(xtensa_tools_root_dir, "install", "builds",
				TOOLCHAIN_VER).absolute())
			XTENSA_SYSTEM = str(pathlib.Path(XTENSA_BUILDS_DIR, XTENSA_CORE, "config").absolute())
			platf_build_environ["XTENSA_SYSTEM"] = XTENSA_SYSTEM

		platform_build_dir_name = f"build-{platform}"

		PLAT_CONFIG = platform_dict["PLAT_CONFIG"]
		build_cmd = ["west"]
		build_cmd += ["-v"] * args.verbose
		build_cmd += ["build", "--build-dir", platform_build_dir_name]
		source_dir = pathlib.Path(SOF_TOP, "app")
		build_cmd += ["--board", PLAT_CONFIG, str(source_dir)]
		if args.pristine:
			build_cmd += ["-p", "always"]

		if args.jobs is not None:
			build_cmd += [f"--build-opt=-j{args.jobs}"]

		build_cmd.append('--')
		if args.cmake_args:
			build_cmd += args.cmake_args

		overlays = [str(item.resolve(True)) for item in args.overlay]
		# The '-d' option is a shortcut for '-o path_to_debug_overlay', we are good
		# if both are provided, because it's no harm to merge the same overlay twice.
		if args.debug:
			overlays.append(str(pathlib.Path(SOF_TOP, "app", "debug_overlay.conf")))

		if overlays:
			overlays = ";".join(overlays)
			build_cmd.append(f"-DOVERLAY_CONFIG={overlays}")

		abs_build_dir = pathlib.Path(west_top, platform_build_dir_name)

		# Longer story in https://github.com/zephyrproject-rtos/zephyr/pull/56671
		if not args.pristine and (
			pathlib.Path(abs_build_dir, "build.ninja").is_file()
			or pathlib.Path(abs_build_dir, "Makefile").is_file()
		):
			if args.cmake_args or overlays:
				warnings.warn("""CMake args slow down incremental builds.
	Passing CMake parameters and overlays on the command line slows down incremental builds
	see https://docs.zephyrproject.org/latest/guides/west/build-flash-debug.html#one-time-cmake-arguments
	Try "west config build.cmake-args -- ..." instead.""")

		platf_build_environ['WEST_CONFIG_LOCAL'] = str(rimage_west_configuration(
			platform_dict,
			STAGING_DIR / "sof-info" / platform
		))

		# Make sure the build logs don't leave anything hidden
		execute_command(['west', 'config', '-l'], cwd=west_top,
				env=platf_build_environ, sof_log_env=True)
		print()

		# Build
		try:
			execute_command(build_cmd, cwd=west_top, env=platf_build_environ, sof_log_env=True)
		except subprocess.CalledProcessError as cpe:
			zephyr_path = pathlib.Path(west_top, "zephyr")
			if not os.path.exists(zephyr_path):
				sys.exit("Zephyr project not found. Please run this script with -u flag or `west update zephyr` manually.")
			else: # unknown failure
				raise cpe

		smex_executable = pathlib.Path(west_top, platform_build_dir_name, "zephyr", "smex_ep",
			"build", "smex")
		fw_ldc_file = pathlib.Path(sof_platform_output_dir, f"sof-{platform}.ldc")
		input_elf_file = pathlib.Path(west_top, platform_build_dir_name, "zephyr", "zephyr.elf")
		# Extract metadata
		execute_command([str(smex_executable), "-l", str(fw_ldc_file), str(input_elf_file)])

		for p_alias in platform_configs[platform].aliases:
			symlink_or_copy(sof_platform_output_dir, f"sof-{platform}.ldc", f"sof-{p_alias}.ldc")

		# reproducible-zephyr.ri is less useful now that show_installed_files() shows
		# checksums too. However: - it's still useful when only the .ri file is
		# available (no build logs for the other image), - it makes sure sof_ri_info.py
		# itself does not bitrot, - it can catch rimage issues, see for instance rimage
		# fix 4fb9fe00575b
		if platform not in RI_INFO_UNSUPPORTED:
			reproducible_checksum(platform, west_top / platform_build_dir_name / "zephyr" / "zephyr.ri")

		install_platform(platform, sof_platform_output_dir, platf_build_environ)

	src_dest_list = []

	# Install sof-logger
	sof_logger_dir = pathlib.Path(west_top, platform_build_dir_name, "zephyr",
		"sof-logger_ep", "build", "logger")
	sof_logger_executable_to_copy = pathlib.Path(shutil.which("sof-logger", path=sof_logger_dir))
	tools_output_dir = pathlib.Path(STAGING_DIR, "tools")
	sof_logger_installed_file = pathlib.Path(tools_output_dir, sof_logger_executable_to_copy.name).resolve()

	src_dest_list += [(sof_logger_executable_to_copy, sof_logger_installed_file)]

	src_dest_list += [(pathlib.Path(SOF_TOP) /
		"tools" / "mtrace"/ "mtrace-reader.py",
		tools_output_dir)]

	# Append future files to `src_dest_list` here (but prefer
	# copying entire directories; more flexible)

	for _src, _dst in src_dest_list:
		os.makedirs(os.path.dirname(_dst), exist_ok=True)
		# looses file owner and group - file is commonly accessible
		shutil.copy2(str(_src), str(_dst))

	# cavstool and friends
	shutil.copytree(pathlib.Path(west_top) /
			  "zephyr" / "soc" / "xtensa" / "intel_adsp" / "tools",
			tools_output_dir,
			symlinks=True, ignore_dangling_symlinks=True, dirs_exist_ok=True)


def install_platform(platform, sof_platform_output_dir, platf_build_environ):

	# Keep in sync with caller
	platform_build_dir_name = f"build-{platform}"

	# Install to STAGING_DIR
	abs_build_dir = pathlib.Path(west_top) / platform_build_dir_name / "zephyr"

	if args.fw_naming == "AVS":
		# Disguise ourselves for local testing purposes
		output_fwname = "dsp_basefw.bin"
	else:
		# Regular name
		output_fwname = "".join(["sof-", platform, ".ri"])

	install_key_dir = sof_platform_output_dir
	if args.key_type_subdir != "none":
		install_key_dir = install_key_dir / args.key_type_subdir

	os.makedirs(install_key_dir, exist_ok=True)
	# looses file owner and group - file is commonly accessible
	shutil.copy2(abs_build_dir / "zephyr.ri", install_key_dir / output_fwname)

	# The production key is usually different
	if args.key_type_subdir != "none" and args.fw_naming != "AVS":
		for p_alias in platform_configs[platform].aliases:
			symlink_or_copy(install_key_dir, output_fwname, f"sof-{p_alias}.ri")


	# sof-info/ directory

	@dataclasses.dataclass
	class InstFile:
		'How to install one file'
		name: pathlib.Path
		renameTo: pathlib.Path = None
		# TODO: upgrade this to 3 states: optional/warning/error
		optional: bool = False
		gzip: bool = True
		txt: bool = False

	installed_files = [
		# Fail if one of these is missing
		InstFile(".config", "config", txt=True),
		InstFile("misc/generated/configs.c", "generated_configs.c", txt=True),
		InstFile("include/generated/version.h", "zephyr_version.h",
			 gzip=False, txt=True),
		InstFile("include/generated/sof_versions.h", "sof_versions.h",
			 gzip=False, txt=True),
		InstFile(BIN_NAME + ".elf"),
		InstFile(BIN_NAME + ".lst", txt=True),
		InstFile(BIN_NAME + ".map", txt=True),

		# CONFIG_BUILD_OUTPUT_STRIPPED
		# Renaming ELF files highlights the workaround below that strips the .comment section
		InstFile(BIN_NAME + ".strip", renameTo=f"stripped-{BIN_NAME}.elf"),

		# Not every platform has intermediate rimage modules
		InstFile("main-stripped.mod", renameTo="stripped-main.elf", optional=True),
		InstFile("boot.mod", optional=True),
		InstFile("main.mod", optional=True),
	]

	# We cannot import at the start because zephyr may not be there yet
	sys.path.insert(1, os.path.join(sys.path[0],
					'..', '..', 'zephyr', 'scripts', 'west_commands'))
	import zcmake

	cmake_cache = zcmake.CMakeCache.from_build_dir(abs_build_dir.parent)
	objcopy = cmake_cache.get("CMAKE_OBJCOPY")

	sof_info = pathlib.Path(STAGING_DIR) / "sof-info" / platform
	sof_info.mkdir(parents=True, exist_ok=True)
	gzip_threads = concurrent.ThreadPoolExecutor()
	gzip_futures = []
	for f in installed_files:
		if not pathlib.Path(abs_build_dir / f.name).is_file() and f.optional:
			continue
		dstname = f.renameTo or f.name

		src = abs_build_dir / f.name
		dst = sof_info / dstname

		# Some Xtensa compilers (ab?)use the .ident / .comment
		# section and append the typically absolute and not
		# reproducible /path/to/the.c file after the usual
		# compiler ID.
		# https://sourceware.org/binutils/docs/as/Ident.html
		#
		# --strip-all does not remove the .comment section.
		# Remove it like some gcc test scripts do:
		# https://gcc.gnu.org/git/?p=gcc.git;a=commit;h=c7046906c3ae
		if "strip" in str(dstname):
			execute_command(
				[str(x) for x in [objcopy, "--remove-section", ".comment", src, dst]],
				# Some xtensa installs don't have a
				# XtensaTools/config/default-params symbolic link
				env=platf_build_environ,
			)
		elif f.txt:
			dos2unix(src, dst)
		else:
			shutil.copy2(src, dst)
		if f.gzip:
			gzip_futures.append(gzip_threads.submit(gzip_compress, dst))
	for gzip_res in concurrent.as_completed(gzip_futures):
		gzip_res.result() # throws exception if gzip unexpectedly failed
	gzip_threads.shutdown()


# Zephyr's CONFIG_KERNEL_BIN_NAME default value
BIN_NAME = 'zephyr'

CHECKSUM_WANTED = [
	# Some .ri files have a deterministic signature, others use
	# a cryptographic salt. Even for the latter a checksum is still
	# useful to match an artefact with a specific build log.
	'*.ri',
	'dsp_basefw.bin',

	'*version*.h',
	'*configs.c', # deterministic unlike .config
	'*.strip', '*stripped*', # stripped ELF files are reproducible
	'boot.mod', # no debug section -> no need to strip this ELF
	BIN_NAME + '.lst',       # objdump --disassemble
	'*.ldc',
]

# Prefer CRLF->LF because unlike LF->CRLF it's (normally) idempotent.
def dos2unix(in_name, out_name):
	with open(in_name, "rb") as inf:
		# must read all at once otherwise could fall between CR and LF
		content = inf.read()
		assert content
		with open(out_name, "wb") as outf:
			outf.write(content.replace(b"\r\n", b"\n"))

def gzip_compress(fname, gzdst=None):
	gzdst = gzdst or pathlib.Path(f"{fname}.gz")
	with open(fname, 'rb') as inputf:
		# mtime=0 for recursive diff convenience
		with gzip.GzipFile(gzdst, 'wb', mtime=0) as gzf:
			shutil.copyfileobj(inputf, gzf)
	os.remove(fname)


# As of October 2022, sof_ri_info.py expects .ri files to include a CSE manifest / signature.
# Don't run sof_ri_info and ignore silently .ri files that don't have one.
RI_INFO_UNSUPPORTED = []

RI_INFO_UNSUPPORTED += ['imx8', 'imx8x', 'imx8m']
RI_INFO_UNSUPPORTED += ['rn']
RI_INFO_UNSUPPORTED += ['mt8186', 'mt8195']

# For temporary workarounds. Unlike _UNSUPPORTED above, the platforms below will print a warning.
RI_INFO_FIXME = [ ]

def reproducible_checksum(platform, ri_file):

	if platform in RI_INFO_FIXME:
		print(f"FIXME: sof_ri_info does not support '{platform}'")
		return

	parsed_ri = sof_ri_info.parse_fw_bin(ri_file, False, False)
	repro_output = ri_file.parent / ("reproducible-" + ri_file.name)
	chk256 = sof_ri_info.EraseVariables(ri_file, parsed_ri, west_top / repro_output)
	print('sha256sum {0}\n{1} {0}'.format(repro_output, chk256))


def main():
	parse_args()
	if args.version:
		print(VERSION)
		sys.exit(0)
	check_west_installation()
	if len(args.platforms) == 0:
		print("No platform build requested")
	else:
		print("Building platforms: {}".format(" ".join(args.platforms)))

	west_init_if_needed()

	if args.update:
		# Initialize zephyr project with west
		west_update()
		create_zephyr_sof_symlink()

	if args.platforms:
		build_rimage()
		build_platforms()
		show_installed_files()

if __name__ == "__main__":
	main()
