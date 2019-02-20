# includes
set(GENERATED_DIRECTORY ${PROJECT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIRECTORY}/include)

target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/arch/host/include)
target_include_directories(sof_options INTERFACE ${PROJECT_SOURCE_DIR}/src/library/include)
target_include_directories(sof_options INTERFACE ${GENERATED_DIRECTORY}/include)

# TODO: add Kconfig support also for testbench when it'll be decoupled from fw
set(CONFIG_HOST 1)
set(CONFIG_LIB 1)
set(CONFIG_COMP_VOLUME 1)
set(CONFIG_COMP_SRC 1)
set(CONFIG_COMP_FIR 1)
set(CONFIG_COMP_IIR 1)
set(CONFIG_COMP_TONE 1)
set(CONFIG_COMP_MIXER 1)
set(CONFIG_COMP_MUX 1)
set(CONFIG_COMP_SWITCH 1)
set(CONFIG_COMP_DAI 1)
set(CONFIG_COMP_SEL 1)

configure_file (
	"${PROJECT_SOURCE_DIR}/src/arch/host/config.h.in"
	"${GENERATED_DIRECTORY}/include/config.h"
)

# linker flags
target_link_libraries(sof_options INTERFACE -lpthread)

# C & ASM flags
target_compile_options(sof_options INTERFACE -g -O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3)
