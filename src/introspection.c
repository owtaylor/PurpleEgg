/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "introspection.h"

static const char introspection_xml[] =
    "<node>"
    "  <interface name='org.gnome.Shell.SearchProvider2'>"
    "    <method name='GetInitialResultSet'>"
    "      <arg type='as' name='terms' direction='in' />"
    "      <arg type='as' name='results' direction='out' />"
    "    </method>"
    "    <method name='GetSubsearchResultSet'>"
    "      <arg type='as' name='previous_results' direction='in' />"
    "      <arg type='as' name='terms' direction='in' />"
    "      <arg type='as' name='results' direction='out' />"
    "    </method>"
    "    <method name='GetResultMetas'>"
    "      <arg type='as' name='identifiers' direction='in' />"
    "      <arg type='aa{sv}' name='metas' direction='out' />"
    "    </method>"
    "    <method name='ActivateResult'>"
    "      <arg type='s' name='identifier' direction='in' />"
    "      <arg type='as' name='terms' direction='in' />"
    "      <arg type='u' name='timestamp' direction='in' />"
    "    </method>"
    "    <method name='LaunchSearch'>"
    "     <arg type='as' name='terms' direction='in' />"
    "     <arg type='u' name='timestamp' direction='in' />"
    "    </method>"
    "  </interface>"
    "</node>";

GDBusNodeInfo *
get_introspection_info(void)
{
    static GDBusNodeInfo *info;

    if (!info) {
        GError *error = NULL;
        info = g_dbus_node_info_new_for_xml(introspection_xml, &error);
        if (error)
            g_error ("Cannot parse introspection XML: %s\n", error->message);
    }

    return info;
}

GDBusInterfaceInfo *
get_introspection_interface(const char *interface_name)
{
    return g_dbus_node_info_lookup_interface(get_introspection_info(),
                                             interface_name);
}
