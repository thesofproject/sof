#!/bin/bash
# Exemplatory usage of sof-coredump-reader.py ($reader_name)
# We read from dump file into sof-coredump-reader.py, then we pipe its output
# to xt-gdb, which operates on given elf-file.

THIS_SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
elf="${1}"
dump="${2}"

reader_name="sof-coredump-reader.py" # in case it is changed
reader_output="$(${THIS_SCRIPT_DIR}/${reader_name} -vc -i ${dump} -l 4 \
		-o reader-output.txt)"
reader_result="$?" # if $reader_name script fails, running xt-gdb is pointless
if [[ ${reader_result} -ne 0 ]] ; then
	echo "${reader_name} failed!"
	exit ${reader_result}
else
	(echo "${reader_output}")
	echo "quit" >> reader-output.txt
	xt-gdb -q "${elf}" --command=reader-output.txt |& tee gdb_log.txt \
		| grep -E '^[0-9]|#|\$'
	rm -rf reader-output.txt
fi
