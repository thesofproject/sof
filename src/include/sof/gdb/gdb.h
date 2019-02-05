#ifndef GDB_H
#define GDB_H

#define GDB_BUFMAX 256
#define GDB_NUMBER_OF_REGISTERS 64
#define DISABLE_LOWER_INTERRUPTS_MASK ~0x1F
void gdb_handle_exception(void);
void gdb_debug_info(unsigned char *str);
void gdb_init_debug_exception(void);
void gdb_init(void);

#endif /* GDB_H */
