To compile sdmon in FreeBSD:

# make -f Makefile.freebsd

To clean-up sdmon in FreeBSD:

# make clean -f Makefile.freebsd

Set FreeBSD GEOM disk access (default value = 0):

# sysctl kern.geom.debugflags=16
