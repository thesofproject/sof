#ifndef GDB_H
#define GDB_H

#define GDB_BUFMAX 256
#define GDB_NUMBER_OF_REGISTERS 64

void gdb_init(void);
void gdb_handle_exception(void);
extern void gdb_debug_info(unsigned char *str);
extern void gdb_init_debug_exception(void);

#endif /* GDB_H */
