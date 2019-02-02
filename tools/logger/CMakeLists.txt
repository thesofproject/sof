add_executable(sof-logger
	logger.c
	convert.c
)

target_compile_options(sof-logger PRIVATE
	-Wall -Werror
)

target_include_directories(sof-logger PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}"
)
