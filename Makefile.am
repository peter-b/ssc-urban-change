bin_PROGRAMS = ridge-changemap
dist_man_MANS = ridge-changemap.1

ridge_changemap_SOURCES = \
	ridge-changemap.h \
	ridge-changemap.c \
	ridge-changemap-map.c \
	ridge-changemap-export.c

AM_CFLAGS = -g -Wall -pedantic \
	$(GSL_CFLAGS) $(RIDGETOOL_CFLAGS) $(GLIB_CFLAGS) \
	$(CAIRO_CFLAGS) $(CAIRO_PNG_CFLAGS) $(CAIRO_PDF_CFLAGS) $(CAIRO_SVG_CFLAGS)
LDADD = $(RIDGETOOL_LIBS) $(GLIB_LIBS) \
	$(CAIRO_LIBS) $(CAIRO_PNG_LIBS) $(CAIRO_PDF_LIBS) $(CAIRO_SVG_LIBS)

ACLOCAL_AMFLAGS = -I m4
