"-lpthread" in the makefile was not working in one of our linux machines, throwing an "undefined reference to pthread_create" error.
Replace it with "-pthread" in both the lines under "all" and that resolves it.


make all - build development code.

make tests - build test code.

make runtests - build dev code, build test code, run memo and run tests.
