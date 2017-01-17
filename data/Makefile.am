-include $(top_srcdir)/git.mk

EXTRA_DIST=
CLEANFILES=

icondir=$(datadir)/icons/hicolor/scalable/apps
icon_DATA = org.gnome.PurpleEgg.svg
EXTRA_DIST += icon_DATA

symbolicdir=$(datadir)/icons/hicolor/symbolic/apps
symbolic_DATA = org.gnome.PurpleEgg-symbolic.svg
EXTRA_DIST += symbolic_DATA

desktopdir=$(datadir)/applications
desktop_DATA = org.gnome.PurpleEgg.desktop
EXTRA_DIST += desktop_DATA

searchdir=$(datadir)/gnome-shell/search-providers

search_DATA = org.gnome.PurpleEgg.search-provider.ini
EXTRA_DIST += searchdir_DATA

org.gnome.PurpleEgg.service: org.gnome.PurpleEgg.service.in
	sed "s,@BINDIR@,$(bindir),g" < $< > $@

CLEANFILES += org.gnome.PurpleEgg.service

servicedir=$(datadir)/dbus-1/services
service_DATA = org.gnome.PurpleEgg.service
