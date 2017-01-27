-include $(top_srcdir)/git.mk

bin_PROGRAMS = PurpleEgg
CLEANFILES =
BUILT_SOURCES =

project.gresource.c: project.gresource.xml project-window.ui
	$(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --target=$@ --generate=.c $<

CLEANFILES += project.gresource.c
BUILT_SOURCES += project.gresource.c

search-generated.c search-generated.h: org.gnome.ShellSearchProvider2.xml
	gdbus-codegen --interface-prefix=org.gnome.Shell  --generate-c-code=search-generated $<

CLEANFILES += search-generated.c search-generated.h
BUILT_SOURCES += search-generated.c search-generated.h

PurpleEgg_SOURCES = \
	introspection.c \
	introspection.h \
	main.c \
	project-application.c \
	project-application.h \
	project-tab.c \
	project-tab.h \
	project-window.c \
	project-window.h \
	project.gresource.c \
	search-generated.c \
	search-generated.h
PurpleEgg_CPPFLAGS = $(PURPLEEGG_CFLAGS) $(WARN_CFLAGS) -DBINDIR=\"$(bindir)\" -DLIBEXECDIR=\"$(libexecdir)\"
PurpleEgg_LDFLAGS = $(PURPLEEGG_LIBS)
