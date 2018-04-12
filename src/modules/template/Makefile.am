all: template.ro

# all module file individially built and then linked below
template.o: template.c
	$(CC) $(CFLAGS) $(COMMON_INCDIR) -c template.c module.c

# used by rimage for module loading/signing
module.o: module.c
	$(CC) $(CFLAGS) $(COMMON_INCDIR) -c module.c

# link all object files togetether into SOF module
template.ro: module.o template.o
	$(LD) -r module.o template.o -o template.ro.debug
	$(OBJCOPY) --strip-debug --strip-unneeded template.ro.debug template.ro

clean-local:
	rm -fr *.o
	rm -fr *.ro
	rm -fr *.debug

bin-local:
	rimage -o template-$(FW_NAME).rm -m $(FW_NAME) -r template.ro