"-lpthread" in the makefile was not working in one of our linux machines, throwing an "undefined reference to pthread_create" error.
Replace it with "-pthread" in both the lines under "all" and that resolves it.
