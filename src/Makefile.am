-include $(top_srcdir)/git.mk

bin_PROGRAMS = PurpleEgg
CLEANFILES =
BUILT_SOURCES =

project.gresource.c: project.gresource.xml project-creator.ui project-greeter.ui project-view.ui
	$(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --target=$@ --generate=.c $<

CLEANFILES += project.gresource.c
BUILT_SOURCES += project.gresource.c

search-generated.c search-generated.h: org.gnome.ShellSearchProvider2.xml
	gdbus-codegen --interface-prefix=org.gnome.Shell  --generate-c-code=search-generated $<

CLEANFILES += search-generated.c search-generated.h
BUILT_SOURCES += search-generated.c search-generated.h

PurpleEgg_SOURCES = \
	egg-radio-box.c \
	egg-radio-box.h \
	egg-three-grid.c \
	egg-three-grid.h \
	introspection.c \
	introspection.h \
	main.c \
	project-application.c \
	project-application.h \
	project-creator.c \
	project-creator.h \
	project-greeter.c \
	project-greeter.h \
	project-tab.c \
	project-tab.h \
	project-view.c \
	project-view.h \
	project-window.c \
	project-window.h \
	project.gresource.c \
	search-generated.c \
	search-generated.h
PurpleEgg_CPPFLAGS = $(PURPLEEGG_CFLAGS) $(WARN_CFLAGS) -I$(top_srcdir)/common -DBINDIR=\"$(bindir)\" -DLIBEXECDIR=\"$(libexecdir)\"
PurpleEgg_LDFLAGS = $(PURPLEEGG_LIBS) -lm
PurpleEgg_LDADD = $(top_builddir)/common/libPurpleEgg-common.la
