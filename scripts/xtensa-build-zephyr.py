#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

# W0311, W0312, C0103, C0116
#pylint:disable=bad-indentation
#pylint:disable=mixed-indentation
#pylint:disable=invalid-name
#pylint:disable=missing-function-docstring
import argparse
import shlex
import subprocess
import pathlib
import errno
import platform as py_platform
import sys
import shutil
import multiprocessing
import os
import warnings
# anytree module is defined in Zephyr build requirements
from anytree import AnyNode, RenderTree
from packaging import version

MIN_PYTHON_VERSION = 3, 8
assert sys.version_info >= MIN_PYTHON_VERSION, \
	f"Python {MIN_PYTHON_VERSION} or above is required."

# Version of this script matching Major.Minor.Patch style.
VERSION = version.Version("2.0.0")

# Constant value resolves SOF_TOP directory as: "this script directory/.."
SOF_TOP = pathlib.Path(__file__).parents[1].resolve()
west_top = pathlib.Path(SOF_TOP, "..").resolve()
default_rimage_key = pathlib.Path(SOF_TOP, "keys", "otc_private_key.pem")

sof_fw_version = None
sof_build_version = None

if py_platform.system() == "Windows":
	xtensa_tools_version_postfix = "-win32"
elif py_platform.system() == "Linux":
	xtensa_tools_version_postfix = "-linux"
else:
	xtensa_tools_version_postfix = "-unsupportedOS"
	warnings.warn(f"Your operating system: {py_platform.system()} is not supported")

platform_list = [
	# Intel platforms
	{
		"name": "apl",
		"PLAT_CONFIG": "intel_adsp_cavs15",
		"XTENSA_CORE": "X4H3I16w2D48w3a_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}"
	},
	{
		"name": "cnl",
		"PLAT_CONFIG": "intel_adsp_cavs18",
		"XTENSA_CORE": "X6H3CNL_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}"
	},
	{
		"name": "icl",
		"PLAT_CONFIG": "intel_adsp_cavs20",
		"XTENSA_CORE": "X6H3CNL_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}"
	},
	{
		"name": "jsl",
		"PLAT_CONFIG": "intel_adsp_cavs20_jsl",
		"XTENSA_CORE": "X6H3CNL_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}"
	},
	{
		"name": "tgl",
		"PLAT_CONFIG": "intel_adsp_cavs25",
		"IPC4_CONFIG_OVERLAY": "ipc4_overlay.conf",
		"IPC4_RIMAGE_DESC": "tgl-cavs.toml",
		"XTENSA_CORE": "cavs2x_LX6HiFi3_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}",
		"RIMAGE_KEY": pathlib.Path(SOF_TOP, "keys", "otc_private_key_3k.pem")
	},
	{
		"name": "tgl-h",
		"PLAT_CONFIG": "intel_adsp_cavs25_tgph",
		"IPC4_CONFIG_OVERLAY": "ipc4_overlay.conf",
		"IPC4_RIMAGE_DESC": "tgl-h-cavs.toml",
		"XTENSA_CORE": "cavs2x_LX6HiFi3_2017_8",
		"XTENSA_TOOLS_VERSION": f"RG-2017.8{xtensa_tools_version_postfix}",
		"RIMAGE_KEY": pathlib.Path(SOF_TOP, "keys", "otc_private_key_3k.pem")
	},
	{
		"name": "mtl",
		"PLAT_CONFIG": "intel_adsp_ace15_mtpm",
		"XTENSA_CORE": "ace10_LX7HiFi4_RI_2020_5",
		"XTENSA_TOOLS_VERSION": f"RI-2020.5{xtensa_tools_version_postfix}",
		"RIMAGE_KEY": pathlib.Path(SOF_TOP, "keys", "otc_private_key_3k.pem")
	},
	# NXP platforms
	{
		"name": "imx8",
		"PLAT_CONFIG": "nxp_adsp_imx8",
		"RIMAGE_KEY": "" # no key needed for imx8
	},
	{
		"name": "imx8x",
		"PLAT_CONFIG": "nxp_adsp_imx8x",
		"RIMAGE_KEY": "ignored for imx8x"
	},
	{
		"name": "imx8m",
		"PLAT_CONFIG": "nxp_adsp_imx8m",
		"RIMAGE_KEY": "ignored for imx8m"
	}
]

platform_names = [platform["name"] for platform in platform_list]

class validate_platforms_arguments(argparse.Action):
	"""Validates positional platform arguments whether provided platform name is supported."""
	def __call__(self, parser, namespace, values, option_string=None):
		if values:
			for value in values:
				if value not in platform_names:
					raise argparse.ArgumentError(self, f"Unsupported platform: {value}")
		setattr(namespace, "platforms", values)

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
						help="Enable debug build")
	parser.add_argument("-i", "--ipc", required=False, choices=["IPC4"],
			    help="""Generic shortcut for: --overlay <platform>/ipc4_overlay.conf. Valid only
for IPC3 platforms supporting IPC4 too.""")
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
        │   ├── sof-tgl.ri
        │   └── sof-cnl.ri
        ├── dbgkey
        │   ├── sof-tgl.ri
        │   └── sof-cnl.ri
        ├── sof-tgl.ri
        └── sof-cnl.ri
For AVS(filename dsp_basefw.bin):
Noted that with fw_naming set as 'AVS', there will be output subdirectories for each platform
    /lib/firmware/intel/sof-ipc4
    └── tgl
    │   ├── community
    │   │   └── dsp_basefw.bin
    │   ├── dbgkey
    │   │   └── dsp_basefw.bin
    │   └── dsp_basefw.bin
    └── cnl"""
	)
	parser.add_argument("-j", "--jobs", required=False, type=int,
						default=multiprocessing.cpu_count(),
						help="Set number of make build jobs for rimage."
						" Jobs=number of cores by default. Ignored by west build.")
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
		if args.ipc != "IPC4":
			args.ipc="IPC4"
			warnings.warn("The option '--fw-naming AVS' has to be used with '-i IPC4'. Enable '-i IPC4' automatically.")


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
		print(f"{print_cwd}; running command: {print_args}", flush=True)

	if run_kwargs.get('check') is None:
		run_kwargs['check'] = True
	#pylint:disable=subprocess-run-check

	return subprocess.run(*run_args, **run_kwargs)

def show_installed_files():
	"""[summary] Scans output directory building binary tree from files and folders
	then presents them in similar way to linux tree command."""
	graph_root = AnyNode(name=STAGING_DIR.name, long_name=STAGING_DIR.name, parent=None)
	relative_entries = [entry.relative_to(STAGING_DIR) for entry in STAGING_DIR.glob("**/*")]
	nodes = [ graph_root ]
	for entry in relative_entries:
		if str(entry.parent) == ".":
			nodes.append(AnyNode(name=entry.name, long_name=str(entry), parent=graph_root))
		else:
			node_parent = [node for node in nodes if node.long_name == str(entry.parent)][0]
			if not node_parent:
				warnings.warn("Failed to construct installed files tree")
				return
			nodes.append(AnyNode(name=entry.name, long_name=str(entry), parent=node_parent))
	for pre, fill, node in RenderTree(graph_root):
		print(f"{pre}{node.name}")

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
		if reinitialize_answer == "y" or reinitialize_answer == "n":
			break
		sys.stdout.write('Please respond with \'Y\' or \'n\'.\n')

	if reinitialize_answer != 'y':
		sys.exit("Can not proceed. Reinitialize your west manifest to SOF and rerun this script.")
	shutil.rmtree(dot_west_directory)
	execute_command(["west", "init", "-l", f"{SOF_TOP}"], cwd=west_top)

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

	west_top.mkdir(mode=511, parents=False, exist_ok=False)
	west_top = west_top.resolve(strict=True)

def create_zephyr_sof_symlink():
	global west_top, SOF_TOP
	if not west_top.exists():
		raise FileNotFoundError("No west top: {}".format(west_top))
	audio_modules_dir = pathlib.Path(west_top, "modules", "audio")
	audio_modules_dir.mkdir(mode=511, parents=True, exist_ok=True)
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


def get_build_and_sof_version(abs_build_dir):
	"""[summary] Get version string major.minor.micro and build of SOF
	firmware file. When building multiple platforms from the same SOF
	commit, all platforms share the same version. So for the 1st platform,
	generate the version string from sof_version.h and later platforms will
	reuse it.
	"""
	global sof_fw_version
	global sof_build_version
	if sof_fw_version and sof_build_version:
		return sof_fw_version, sof_build_version

	versions = {}
	with open(pathlib.Path(abs_build_dir,
		  "zephyr/include/generated/sof_versions.h"), encoding="utf8") as hfile:
		for hline in hfile:
			words = hline.split()
			if words[0] == '#define':
				versions[words[1]] = words[2]
	sof_fw_version = versions['SOF_MAJOR'] + '.' + versions['SOF_MINOR'] + '.' + \
		      versions['SOF_MICRO']
	sof_build_version = versions['SOF_BUILD']

	return sof_fw_version, sof_build_version

def build_platforms():
	global west_top, SOF_TOP
	print(f"SOF_TOP={SOF_TOP}")
	print(f"west_top={west_top}")

	global STAGING_DIR
	STAGING_DIR = pathlib.Path(west_top, "build-sof-staging")
	if args.pristine:
		if os.path.exists(STAGING_DIR):
			shutil.rmtree(STAGING_DIR)

	# smex does not use 'install -D'
	sof_output_dir = pathlib.Path(STAGING_DIR, "sof")
	sof_output_dir.mkdir(mode=511, parents=True, exist_ok=True)
	for platform in args.platforms:
		if args.use_platform_subdir:
			sof_platform_output_dir = pathlib.Path(sof_output_dir, platform)
			sof_platform_output_dir.mkdir(parents=True, exist_ok=True)
		else:
			sof_platform_output_dir = sof_output_dir

		platform_dict = [x for x in platform_list if x["name"] == platform][0]
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
			os.environ["ZEPHYR_TOOLCHAIN_VARIANT"] = "xcc"
			XTENSA_TOOLCHAIN_PATH = str(pathlib.Path(xtensa_tools_root_dir, "install",
				"tools").absolute())
			os.environ["XTENSA_TOOLCHAIN_PATH"] = XTENSA_TOOLCHAIN_PATH
			TOOLCHAIN_VER = platform_dict["XTENSA_TOOLS_VERSION"]
			XTENSA_CORE = platform_dict["XTENSA_CORE"]
			os.environ["TOOLCHAIN_VER"] = TOOLCHAIN_VER
			print(f"XTENSA_TOOLCHAIN_PATH={XTENSA_TOOLCHAIN_PATH}")
			print(f"TOOLCHAIN_VER={TOOLCHAIN_VER}")

			# set variables expected by xcc toolchain
			XTENSA_BUILDS_DIR=str(pathlib.Path(xtensa_tools_root_dir, "install", "builds",
				TOOLCHAIN_VER).absolute())
			XTENSA_SYSTEM = str(pathlib.Path(XTENSA_BUILDS_DIR, XTENSA_CORE, "config").absolute())
			os.environ["XTENSA_SYSTEM"] = XTENSA_SYSTEM
			print(f"XTENSA_SYSTEM={XTENSA_SYSTEM}")

		platform_build_dir_name = f"build-{platform}"

		# https://docs.zephyrproject.org/latest/guides/west/build-flash-debug.html#one-time-cmake-arguments
		# https://github.com/zephyrproject-rtos/zephyr/pull/40431#issuecomment-975992951
		abs_build_dir = pathlib.Path(west_top, platform_build_dir_name)
		if (pathlib.Path(abs_build_dir, "build.ninja").is_file()
		    or pathlib.Path(abs_build_dir, "Makefile").is_file()):
			if args.cmake_args and not args.pristine:
				print(args.cmake_args)
				raise RuntimeError("Some CMake arguments are ignored in incremental builds, "
						   + f"you must delete {abs_build_dir} first")

		PLAT_CONFIG = platform_dict["PLAT_CONFIG"]
		build_cmd = ["west"]
		build_cmd += ["-v"] * args.verbose
		build_cmd += ["build", "--build-dir", platform_build_dir_name]
		source_dir = pathlib.Path(SOF_TOP, "app")
		build_cmd += ["--board", PLAT_CONFIG, str(source_dir)]
		if args.pristine:
			build_cmd += ["-p", "always"]

		build_cmd.append('--')
		if args.cmake_args:
			build_cmd += args.cmake_args

		overlays = [str(item.resolve(True)) for item in args.overlay]
		# The '-d' option is a shortcut for '-o path_to_debug_overlay', we are good
		# if both are provided, because it's no harm to merge the same overlay twice.
		if args.debug:
			overlays.append(str(pathlib.Path(SOF_TOP, "app", "debug_overlay.conf")))

		# The '-i IPC4' is a shortcut for '-o path_to_ipc4_overlay', we are good if both
		# are provided, because it's no harm to merge the same overlay twice.
		if args.ipc == "IPC4":
			overlays.append(str(pathlib.Path(SOF_TOP, "app", "overlays", platform,
                            platform_dict["IPC4_CONFIG_OVERLAY"])))

		if overlays:
			overlays = ";".join(overlays)
			build_cmd.append(f"-DOVERLAY_CONFIG={overlays}")

		# Build
		try:
			execute_command(build_cmd, cwd=west_top)
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
		# CMake - configure rimage module
		rimage_dir_name="build-rimage"
		rimage_source_dir = pathlib.Path(SOF_TOP, "rimage")
		execute_command(["cmake", "-B", rimage_dir_name, "-S", str(rimage_source_dir)],
			cwd=west_top)
		# CMake build rimage module
		execute_command(["cmake", "--build", rimage_dir_name, "-j", str(args.jobs)],
			cwd=west_top)
		# Sign firmware
		rimage_executable = shutil.which("rimage", path=pathlib.Path(west_top, rimage_dir_name))
		rimage_config = pathlib.Path(SOF_TOP, "rimage", "config")
		sign_cmd = ["west"]
		sign_cmd += ["-v"] * args.verbose
		sign_cmd += ["sign", "--build-dir", platform_build_dir_name, "--tool", "rimage"]
		sign_cmd += ["--tool-path", rimage_executable]
		signing_key = ""
		if args.key:
			signing_key = args.key
		elif "RIMAGE_KEY" in platform_dict:
			signing_key = platform_dict["RIMAGE_KEY"]
		else:
			signing_key = default_rimage_key

		sign_cmd += ["--tool-data", str(rimage_config), "--", "-k", str(signing_key)]

		sof_fw_version, sof_build_version = get_build_and_sof_version(abs_build_dir)

		sign_cmd += ["-f", sof_fw_version]

		sign_cmd += ["-b", sof_build_version]

		if args.fw_naming == "AVS":
			output_fwname="dsp_basefw.bin"
		else:
			output_fwname="".join(["sof-", platform, ".ri"])
		if args.ipc == "IPC4":
			rimage_desc = pathlib.Path(SOF_TOP, "rimage", "config", platform_dict["IPC4_RIMAGE_DESC"])
			sign_cmd += ["-c", str(rimage_desc)]

		execute_command(sign_cmd, cwd=west_top)
		# Install by copy
		fw_file_to_copy = pathlib.Path(west_top, platform_build_dir_name, "zephyr", "zephyr.ri")
		if args.key_type_subdir == "none":
			fw_file_installed = pathlib.Path(sof_platform_output_dir,
							 f"{output_fwname}")
		else:
			fw_file_installed = pathlib.Path(sof_platform_output_dir, args.key_type_subdir,
							 f"{output_fwname}")
		os.makedirs(os.path.dirname(fw_file_installed), exist_ok=True)
		# looses file owner and group - file is commonly accessible
		shutil.copy2(str(fw_file_to_copy), str(fw_file_installed))

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
		build_platforms()
		show_installed_files()

if __name__ == "__main__":
	main()
