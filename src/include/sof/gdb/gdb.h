#ifndef GDB_H
#define GDB_H

#define GDB_BUFMAX 256
#define GDB_NUMBER_OF_REGISTERS 64
#define DISABLE_LOWER_INTERRUPTS_MASK ~0x1F
#define REGISTER_MASK 0xFF
#define FIRST_BYTE_MASK 0xF0000000
#define VALID_MEM_START_BYTE 0xB
#define VALID_MEM_ADDRESS_LEN 0x8
void gdb_handle_exception(void);
void gdb_debug_info(unsigned char *str);
void gdb_init_debug_exception(void);
void gdb_init(void);

#endif /* GDB_H */
