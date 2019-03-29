MAJOR=`grep '#define SOF_ABI_MAJOR ' $1/src/include/uapi/abi.h | grep -E ".[[:digit:]]$" -o`
MINOR=`grep '#define SOF_ABI_MINOR ' $1/src/include/uapi/abi.h | grep -E ".[[:digit:]]$" -o`
PATCH=`grep '#define SOF_ABI_PATCH ' $1/src/include/uapi/abi.h | grep -E ".[[:digit:]]$" -o`
printf "define(\`SOF_ABI_MAJOR', \`0x%02x')\n" $MAJOR > abi.h
printf "define(\`SOF_ABI_MINOR', \`0x%02x')\n" $MINOR >> abi.h
printf "define(\`SOF_ABI_PATCH', \`0x%02x')\n" $PATCH >> abi.h
