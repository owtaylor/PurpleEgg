#include <glib-unix.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "host-command.h"

typedef struct {
  PeggHostCommandCallback callback;
  guint connection_id;
  int pid;
  gpointer user_data;
} HostCommandData;

void
on_child_exited (GDBusConnection *connection,
                 const gchar     *sender_name,
                 const gchar     *object_path,
                 const gchar     *interface_name,
                 const gchar     *signal_name,
                 GVariant        *parameters,
                 gpointer         user_data)
{
  HostCommandData *data = user_data;
  int pid = -1;
  int exit_status = -1;

  g_variant_get (parameters, "(uu)", &pid, &exit_status);
  if (pid == data->pid)
    {
      data->callback (pid, exit_status, data->user_data);
      g_dbus_connection_signal_unsubscribe (connection, data->connection_id);
      g_free (data);
    }
}

gboolean
pegg_in_flatpak (void)
{
  int in_flatpak = -1;
  if (in_flatpak == -1)
    {
      g_autofree char *flatpak_info = g_build_filename (g_get_user_runtime_dir (),
                                                        "flatpak-info",
                                                        NULL);
      g_autoptr (GFile) f = g_file_new_for_path (flatpak_info);
      in_flatpak = g_file_query_exists (f, NULL);
    }

  return in_flatpak;
}

int
pegg_call_host_command (GDBusConnection          *connection,
                        char                    **args,
                        PeggHostCommandCallback   callback,
                        gpointer                  user_data,
                        GCancellable             *cancellable,
                        GError                  **error)
{
  g_autoptr(GUnixFDList) fd_list = g_unix_fd_list_new ();
  int stdin_handle = g_unix_fd_list_append (fd_list, 0, error);
  if (stdin_handle == -1)
    return -1;
  int stdout_handle = g_unix_fd_list_append (fd_list, 1, error);
  if (stdout_handle == -1)
    return -1;
  int stderr_handle = g_unix_fd_list_append (fd_list, 2, error);
  if (stderr_handle == -1)
    return -1;

  g_autoptr(GVariantBuilder) fd_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{uh}"));
  g_variant_builder_add (fd_builder, "{uh}", 0, stdin_handle);
  g_variant_builder_add (fd_builder, "{uh}", 1, stdout_handle);
  g_variant_builder_add (fd_builder, "{uh}", 2, stderr_handle);

  g_autoptr(GVariantBuilder) env_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
  const char *term = g_getenv("TERM");
  if (term)
    g_variant_builder_add (env_builder, "{ss}", "TERM", term);

  g_autoptr(GVariant) params = g_variant_new ("(^ay^aay@a{uh}@a{ss}u)",
                                              g_get_home_dir (),
                                              args,
                                              g_variant_builder_end (g_steal_pointer (&fd_builder)),
                                              g_variant_builder_end (g_steal_pointer (&env_builder)),
                                              0); /* FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV */
  g_variant_ref_sink (params);

  HostCommandData *data = g_new0 (HostCommandData, 1);
  data->callback = callback;
  data->user_data = user_data;
  data->connection_id = g_dbus_connection_signal_subscribe (connection,
                                                            "org.freedesktop.Flatpak",
                                                            "org.freedesktop.Flatpak.Development",
                                                            "HostCommandExited",
                                                            "/org/freedesktop/Flatpak/Development",
                                                            NULL,
                                                            0, /* flags */
                                                            on_child_exited,
                                                            data,
                                                            NULL);

  g_autoptr(GVariant) reply;
  reply = g_dbus_connection_call_with_unix_fd_list_sync (connection,
                                                         "org.freedesktop.Flatpak",
                                                         "/org/freedesktop/Flatpak/Development",
                                                         "org.freedesktop.Flatpak.Development",
                                                         "HostCommand",
                                                         params,
                                                         G_VARIANT_TYPE ("(u)"),
                                                         G_DBUS_CALL_FLAGS_NONE,
                                                         G_MAXINT,
                                                         fd_list,
                                                         NULL,
                                                         cancellable,
                                                         error);
  if (reply == NULL)
    {
      g_dbus_connection_signal_unsubscribe (connection, data->connection_id);
      g_free (data);
      return -1;
    }

  int pid;
  g_variant_get (reply, "(u)", &pid);

  data->pid = pid;

  return pid;
}

gboolean
pegg_send_host_command_signal (GDBusConnection *connection,
                               int              pid,
                               int              signum,
                               gboolean         to_process_group,
                               GCancellable    *cancellable,
                               GError         **error)
{
  g_autoptr(GVariant) reply;
  reply = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.Flatpak",
                                       "/org/freedesktop/Flatpak/Development",
                                       "org.freedesktop.Flatpak.Development",
                                       "HostCommandSignal",
                                       g_variant_new ("(uub)",
                                                      pid, signum, to_process_group),
                                       G_VARIANT_TYPE ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       G_MAXINT,
                                       cancellable,
                                       error);
  return reply != NULL;
}
