plugin_LTLIBRARIES = libgstducati.la

# headers we need but don't want installed
noinst_HEADERS = \
	gstducativc1dec.h \
	gstducatimpeg4dec.h \
	gstducatih264dec.h \
	gstducatividdec.h \
	gstducati.h

# sources used to compile this plug-in
libgstducati_la_SOURCES = \
	gstducativc1dec.c \
	gstducatimpeg4dec.c \
	gstducatih264dec.c \
	gstducatividdec.c \
	gstducati.c \
	$(noinst_HEADERS)

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstducati_la_CFLAGS = $(GST_CFLAGS) $(MEMMGR_CFLAGS) $(LIBDCE_CFLAGS)
libgstducati_la_LIBADD = $(GST_LIBS) $(MEMMGR_LIBS) $(LIBDCE_LIBS)
libgstducati_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(GST_ALL_LDFLAGS)
libgstducati_la_LIBTOOLFLAGS = --tag=disable-static
