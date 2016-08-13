#ifndef __INTROSPECTION_H__
#define __INTROSPECTION_H__

#include <gio/gio.h>

#define SEARCH_PATH "/org/gnome/PurpleEgg/SearchProvider"
#define SEARCH_INTERFACE "org.gnome.Shell.SearchProvider2"

GDBusNodeInfo      *get_introspection_info     (void);
GDBusInterfaceInfo *get_introspection_interface(const char *interface_name);

#endif /* __INTROSPECTION_H__ */

