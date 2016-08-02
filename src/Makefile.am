-include $(top_srcdir)/git.mk

bin_PROGRAMS = newterm

project.gresource.c: project.gresource.xml project-window.ui
	$(GLIB_COMPILE_RESOURCES) --target=$@ --generate=.c $<

search-generated.c search-generated.h: org.gnome.ShellSearchProvider2.xml
	gdbus-codegen --interface-prefix=org.gnome.Shell  --generate-c-code=search-generated org.gnome.ShellSearchProvider2.xml

newterm_SOURCES = \
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
newterm_CPPFLAGS = $(NEWTERM_CFLAGS) $(WARN_CFLAGS)
newterm_LDFLAGS = $(NEWTERM_LIBS)
