
target_sources(dummy PRIVATE dummy.c)

set_target_properties(dummy PROPERTIES
  HPSRAM_ADDR "0xa06a1000"
)
