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

dnl P_GRAPH(name, CONNECTIONS)
define(`P_GRAPH',
`SectionGraph.STR($1) {'
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
`SectionWidget.STR($1) {'
`       index STR($3)'
`       type STR($2)'
`       no_pm "true"'
`}')

divert(0) dnl

