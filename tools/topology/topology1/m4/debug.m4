divert(-1)

define(`DEBUG_START',
`ifdef(`GRAPH', `errprint(`digraph topology {
node [color=Red,fontname=Courier]
edge [color=Blue, style=dashed]'
)')')

define(`DEBUG_END',
`ifdef(`GRAPH', `errprint(`}'
)')')

define(`DEBUG_GRAPH',
`ifdef(`GRAPH', `errprint("$2"->"$1"
)')')

define(`DEBUG_DAI',
`ifdef(`INFO', `errprint(/* note: $1 DAI ADD dai_index $2 should match the member (index) in struct dai array (firmware platform specific, usually in dai.c) */
)')')

define(`DEBUG_DAI_CONFIG',
`ifdef(`INFO', `errprint(/* note: $1 DAI CONFIG id $2 should match the member (id) in struct snd_soc_dai_link in kernel machine driver (usually under linux/sound/soc/intel/boards/) */
)')')

define(`DEBUG_PCM_ADD',
`ifdef(`INFO', `errprint(/* note: PCM ADD for $1 id $2 should be unique within playback or capture pcm ids*/
)')')

divert(0)dnl
