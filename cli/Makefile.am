-include $(top_srcdir)/git.mk

EXTRA_DIST =

libexec_PROGRAMS = pegg-docker-launch pegg-run-host

pegg_docker_launch_SOURCES = docker-launch.c
pegg_docker_launch_CFLAGS = $(PEGG_CFLAGS) -I$(top_srcdir)/common
pegg_docker_launch_LDFLAGS = $(PEGG_LIBS)
pegg_docker_launch_LDADD = $(top_builddir)/common/libPurpleEgg-common.la

pegg_run_host_SOURCES = run-host.c
pegg_run_host_CFLAGS = $(PEGG_CFLAGS) -I$(top_srcdir)/common
pegg_run_host_LDFLAGS = $(PEGG_LIBS)
pegg_run_host_LDADD = $(top_builddir)/common/libPurpleEgg-common.la

bin_SCRIPTS = pegg

pegg: pegg.in Makefile
	sed "s,@LIBEXEC@,$(libexecdir),g" < $< > $@
EXTRA_DIST += pegg.in

CLEANFILES = pegg
