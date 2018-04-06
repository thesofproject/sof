modules: template.sof

# all module file individially built and then linked below
template.o: template.c
	$(CC) $(CFLAGS) $(COMMON_INCDIR) -c template.c module.c

# used by rimage for module loading/signing
module.o: module.c
	$(CC) $(CFLAGS) $(COMMON_INCDIR) -c module.c

# link all object files togetether into SOF module
template.sof: module.o template.o
	$(LD) -r module.o template.o -o template.sof

clean-local:
	rm -fr *.o

bin-local:
	ls
#rimage