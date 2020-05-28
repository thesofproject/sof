divert(-1)

include(`debug.m4')

define(`concat',`$1$2')

define(`STR', `"'$1`"')

dnl Argument iterator.
define(`argn', `ifelse(`$1', 1, ``$2'',
       `argn(decr(`$1'), shift(shift($@)))')')

define(`fatal_error', `errprint(`m4: '__file__: __line__`: fatal error: $*
')m4exit(1)')

dnl Defines a list of items from a variable number of params.
dnl Use as last argument in a macro.
dnl The first argument specifies the number of tabs to be added for formatting
define(`LIST_LOOP', `argn(j,$@)
$1ifelse(i,`2', `', `define(`i', decr(i))define(`j', incr(j))$0($@)')')

define(`LIST', `pushdef(`i', $#)pushdef(`j', `2')LIST_LOOP($@)popdef(i)popdef(j)')

dnl Sums a list of variable arguments. Use as last argument in macro.
define(`SUM_LOOP', `eval(argn(j,$@)
		ifelse(i,`1', `', `define(`i', decr(i)) define(`j', incr(j)) + $0($@)'))')

dnl Memory capabilities
define(`MEMCAPS', `pushdef(`i', $#) pushdef(`j', `1') SUM_LOOP($@)')

dnl create direct DAPM/pipeline link between 2 widgets)
define(`dapm', `"$1, , $2"'`DEBUG_GRAPH($1, $2)')

dnl COMP_SAMPLE_SIZE(FMT)
define(`COMP_SAMPLE_SIZE',
`ifelse(
	$1, `s16le', `2',
	$1, `s24_4le', `4',
	$1, `s32le', `4',
	$1, `float', `4',
	`4')')

dnl COMP_FORMAT_NAME(FMT)
define(`COMP_FORMAT_NAME',
`ifelse(
	$1, `s16le', `S16_LE',
	$1, `s24le', `S24_LE',
	$1, `s32le', `S32_LE',
	$1, `float', `FLOAT_LE',
	)')

dnl COMP_FORMAT_VALUE(FMT)
define(`COMP_FORMAT_VALUE',
`ifelse(
	$1, `s16le', `0x00',
	$1, `s24le', `0x01',
	$1, `s32le', `0x02',
	)')

# note: only support number < 256 at the moment
dnl DEC2HEX(dec_num)
define(`DEC2HEX',
`ifelse(
	eval(`$1 < 16'), `1', concat(`0x0',eval($1, 16)),
	eval(`$1 < 256'), `1', concat(`0x',eval($1, 16)),
	)')

dnl P_GRAPH(NAME, PIPELINE_ID, CONNECTIONS)
define(`P_GRAPH',
`SectionGraph.STR($1 $2) {'
`	index STR($2)'
`'
`	lines ['
`		$3'
`	]'
`}')

dnl W_VENDORTUPLES(name, tokens, RATE_OUT)
define(`W_VENDORTUPLES',
`SectionVendorTuples.STR($1) {'
`	tokens STR($2)'
`'
`	tuples."word" {'
`		$3'
`	}'
`}')

dnl W_DATA(name, tuples)
define(`W_DATA',
`SectionData.STR($1) {'
`	tuples STR($2)'
`}')

dnl VIRTUAL_DAPM_ROUTE_OUT(name, dai type, dai index, direction, index)
define(`VIRTUAL_DAPM_ROUTE_OUT',
`SectionWidget.STR($1) {'
`       index STR($5)'
`       type "output"'
`       no_pm "true"'
`}'
`SectionGraph.STR($2) {'
`       index STR($5)'
`'
`       lines ['
`               dapm($1,$2$3.$4)'
`       ]'
`}')

dnl VIRTUAL_DAPM_ROUTE_IN(name, dai type, dai index, direction, index)
define(`VIRTUAL_DAPM_ROUTE_IN',
`SectionWidget.STR($1) {'
`       index STR($5)'
`       type "input"'
`       no_pm "true"'
`}'
`SectionGraph.STR($2) {'
`       index STR($5)'
`'
`       lines ['
`               dapm($2$3.$4, $1)'
`       ]'
`}')

dnl VIRTUAL_WIDGET(name, type, index)
define(`VIRTUAL_WIDGET',
`ifelse(`$#', `3',
`SectionWidget.STR($1) {'
`       index STR($3)'
`       type STR($2)'
`       no_pm "true"'
`}', `fatal_error(`Invalid parameters ($#) to VIRTUAL_WIDGET')')')

dnl create SOF_ABI_VERSION if not defined
dnl you can give the abi.h with -DSOF_ABI_FILE=(full path to abi.h)
dnl otherwise this will be empty macro
ifdef(`SOF_ABI_VERSION', `',
ifdef(`SOF_ABI_FILE',
`define(`SOF_MAJOR','
`esyscmd(`grep "#define SOF_ABI_MAJOR "' SOF_ABI_FILE `| grep -E ".[[:digit:]]$" -o'))dnl'
`define(`SOF_MINOR','
`esyscmd(`grep "#define SOF_ABI_MINOR "' SOF_ABI_FILE `| grep -E ".[[:digit:]]$" -o'))dnl'
`define(`SOF_PATCH','
`esyscmd(`grep "#define SOF_ABI_PATCH "' SOF_ABI_FILE `| grep -E ".[[:digit:]]$" -o'))dnl'
`define(`SOF_MAJOR_SHIFT','
`esyscmd(`grep "#define SOF_ABI_MAJOR_SHIFT"' SOF_ABI_FILE `| grep -E ".[[:digit:]]$" -o'))dnl'
`define(`SOF_MINOR_SHIFT','
`esyscmd(`grep "#define SOF_ABI_MINOR_SHIFT"' SOF_ABI_FILE `| grep -E ".[[:digit:]]$" -o'))dnl'
`define(`SOF_ABI_VERSION','
`eval(eval((SOF_MAJOR) << (SOF_MAJOR_SHIFT))dnl'
`| eval((SOF_MINOR) << (SOF_MINOR_SHIFT))))dnl'
 `'))

dnl print number's 4 bytes from right to left as hex values
define(`PRINT_BYTES_4',
`format(`0x%02x', eval(($1)&0xFF)),'dnl
`format(`0x%02x', eval(($1>>8)&0xFF)),'dnl
`format(`0x%02x', eval(($1>>16)&0xFF)),'dnl
`format(`0x%02x', eval(($1>>24)&0xFF))')dnl

dnl print number's 2 bytes from right to left as hex values
define(`PRINT_BYTES_2',
`format(`0x%02x', eval(($1)&0xFF)),'dnl
`format(`0x%02x', eval(($1>>8)&0xFF))')dnl

dnl print a number's right most byte as hex values
define(`PRINT_BYTE',
`format(`0x%02x', eval(($1)&0xFF))')dnl

dnl make a byte from 8 binary values, right to left in increasing argument order
define(`BITS_TO_BYTE',
`eval(eval($1 << 0) | eval($2 << 1) | eval($3 << 2) | eval($4 << 3)dnl
| eval($5 << 4) | eval($6 << 5) | eval($7 << 6) | eval($8 << 7))')dnl

dnl macro for component UUID declare, support copying from the FW source directly.
dnl DECLARE_SOF_RT_UUID(name, macro, a, b, c,
dnl		 d0, d1, d2, d3, d4, d5, d6, d7)
define(`DECLARE_SOF_RT_UUID',
`define(`$2',
	`format(`%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s:%2s',
		substr($3, 8, 2), substr($3, 6, 2), substr($3, 4, 2), substr($3, 2, 2),
		substr($4, 4, 2), substr($4, 2, 2), substr($5, 4, 2), substr($5, 2, 2),
		substr($6, 2, 2), substr($7, 2, 2), substr($8, 2, 2), substr($9, 2, 2),
		substr($10, 2, 2), substr($11, 2, 2), substr($12, 2, 2), substr($13, 2, 2)
)')')

divert(0) dnl

