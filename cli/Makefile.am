-include $(top_srcdir)/git.mk

EXTRA_DIST =

libexec_PROGRAMS = pegg-docker-launch pegg-run-host

pegg_docker_launch_SOURCES = docker-launch.c host-command.c
pegg_docker_launch_CFLAGS = $(PEGG_CFLAGS)
pegg_docker_launch_LDFLAGS = $(PEGG_LIBS)

pegg_run_host_SOURCES = run-host.c host-command.c
pegg_run_host_CFLAGS = $(PEGG_CFLAGS)
pegg_run_host_LDFLAGS = $(PEGG_LIBS)

bin_SCRIPTS = pegg

pegg: pegg.in
	sed "s,@LIBEXEC@,$(libexecdir),g" < $< > $@
EXTRA_DIST += pegg.in

