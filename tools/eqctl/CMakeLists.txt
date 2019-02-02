add_executable(sof-eqctl
	eqctl.c
)

target_link_libraries(sof-eqctl PRIVATE
	"-lasound"
)
