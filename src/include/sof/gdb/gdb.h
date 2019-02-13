#ifndef GDB_H
#define GDB_H

#define GDB_BUFMAX 256
#define GDB_NUMBER_OF_REGISTERS 64
#define GDB_DISABLE_LOWER_INTERRUPTS_MASK ~0x1F
#define GDB_REGISTER_MASK 0xFF
#define GDB_FIRST_BYTE_MASK 0xF0000000
#define GDB_VALID_MEM_START_BYTE 0xB
#define GDB_VALID_MEM_ADDRESS_LEN 0x8
#define GDB_AR_REG_RANGE 0x10 /**< identifies registers in current window */
#define GDB_PC_REG_ID 0x20 /**< identifies PC register */
#define GDB_AREG_RANGE 0x100 /**< identifies address registers range */
#define GDB_SPEC_REG_RANGE_START 0x200 /**< identifies spec registers range */
#define GDB_SPEC_REG_RANGE_END 0x300 /**< identifies spec registers range */
#define GDB_REG_RANGE_END 0x400 /**< identifies spec registers range */

void gdb_handle_exception(void);
void gdb_debug_info(unsigned char *str);
void gdb_init_debug_exception(void);
void gdb_init(void);

#endif /* GDB_H */
