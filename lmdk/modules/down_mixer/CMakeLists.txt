
target_sources(down_mixer PRIVATE downmixer.c)

set_target_properties(down_mixer PROPERTIES
	HPSRAM_ADDR "0xa0691000"
)
