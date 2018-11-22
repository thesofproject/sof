#!/bin/bash
# Simple script for printing preprocessor output in human-readable fashion
# Mostly useful for debugging macro metaprogramming
# For more detailed useage, use --h option.
enstyle(){
	awk \
		-v style="${1}" \
		-v regex="${2}" \
'{\
	gsub(\
		regex,\
		"\033["style"m&\033[0m"\
	);\
	print;\
}'
}
styleUserRegexp()
{
	if [[ "${USER_REGEXP}" != "" ]] ; then
		enstyle "7;33" "${USER_REGEXP}"
	else
		tee # pipe that does nothing, just pass input to output
	fi
}
styleAll(){
	if [[ "${COLORISE}" == "1" ]] ; then
		enstyle   "33" '[{}()\\[\\],;]'\
		| enstyle "1;31" '->|\\.|&'\
		| enstyle "1;35" '\\s(if|while|do|for|return|exit|panic)'\
		| enstyle "1;36" '(u?int(32|64)_t|void|struct|const|char|volatile|static|unsigned|long)\\s*\\**'\
		| enstyle   "32" '0x[A-Za-z0-9]+'\
		| styleUserRegexp
	else
		tee # pipe that does nothing, just pass input to output
	fi
}

pretty_macro(){
	params="${1}"
	infile="${2}"

	gcc_params="${params} -E -dD ${infile}"
	if [[ "${SHOW_ERR}" == "0" ]] ; then
		gcc_params+=" 2>/dev/null"
	fi
	cmd="gcc ${gcc_params}"
	if [[ "${VERBOSE}"  == "1" ]] ; then
		echo "params are: [${params}]"
		echo "infile  is: [${infile}]"
		echo "cmd     is: [${cmd}]"
	fi

	bash -c "$cmd" \
		| sed\
			-e 's/;\ /;\n\t/g'\
			-e 's/{\ /\n{\n\t/g'\
			-e 's/}[\ ]*/\n}\n/g'\
		| styleAll
}

VERBOSE=0
SHOW_ERR=0
COLORISE=0
gcc_params=""
USER_REGEXP=""
while getopts ":f:I:u:hevc" opt; do
	case ${opt} in
		h) #usage
			echo \
"usage: $0 [-hevc] (-f IN_FILE) [-I INCLUDE_DIR] [-u USER_REGEXP]

Expands c macroes in files given with -f flag and prints them prettily.

mandatory arguments:
    -f IN_FILE      input file to gcc
optional  arguments:
    -h              show this help message and exit
    -e              don't redirect gcc errors to /dev/null
    -v              show verbose output
    -c              color the output
    -u USER_REGEXP  highlights user defined regular expression 
    -I INCLUDE_DIR  pass INCLUDE_DIR to gcc with -I option
"
			exit 0
		;;
		u)
			USER_REGEXP="${OPTARG}"
		;;
		e)
			SHOW_ERR=1
		;;
		v)
			VERBOSE=1
		;;
		c)
			COLORISE=1
		;;
		I)
			gcc_params+=" -I ${OPTARG}"
		;;
		f)
			infiles+=("${OPTARG}") 
		;;
		\?)
			echo "Invalid option: -${OPTARG}" >&2
			exit 1
		;;
		:)
			echo "Option -${OPTARG} requires an argument." >&2
			exit 1
		;;
	esac
done
if [[ "${VERBOSE}" == "1" ]] ; then
	echo "gcc_params are: [${gcc_params}]"
	echo "infiles    are: [${infiles[@]}]"
	echo "user regexp is: [${USER_REGEXP}]"
fi
for file in ${infiles[@]} ; do
	pretty_macro "${gcc_params}" "${file}"
done
exit 0

