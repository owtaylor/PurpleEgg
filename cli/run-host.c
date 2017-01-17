#include <glib-unix.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "host-command.h"

static void
on_host_command_exited (int      pid,
                        int      exit_status,
                        gpointer data)
{
  if (WIFEXITED (exit_status))
    exit (WEXITSTATUS (exit_status));
  else if (WIFSIGNALED (exit_status))
    exit (128 + WTERMSIG (exit_status));
  else
    exit (255);
}

typedef struct
{
  GDBusConnection *connection;
  int pid;
  int signum;
  gboolean to_process_group;
} WatchSignalData;


static gboolean
on_watched_signal (gpointer user_data)
{
  WatchSignalData *data = user_data;
  GError *error = NULL;

  if (!pegg_send_host_command_signal (data->connection,
                                      data->pid, data->signum,
                                      data->to_process_group,
                                      NULL, &error))
    {
      g_printerr ("Failed to send signal to child process: %s\n", error->message);
      g_clear_error (&error);
    }

  return G_SOURCE_CONTINUE;
}

static void
watch_signal (GDBusConnection *connection,
              int              pid,
              int              signum,
              gboolean         to_process_group)
{
  WatchSignalData *data = g_new0 (WatchSignalData, 1);
  data->connection = connection;
  data->pid = pid;
  data->signum = signum;
  data->to_process_group = to_process_group;

  g_unix_signal_add (signum, on_watched_signal, data);
}

int
main(int argc, char **argv)
{
  GError *error = NULL;

  if (!pegg_in_flatpak ())
    {
      g_printerr ("Not inside a flatpak\n");
      return 1;
    }

  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (!connection)
    {
      g_printerr ("Could not get connection to bus\n");
      return 1;
    }

  GPtrArray *arg_array = g_ptr_array_new();
  for (int i = 1; i < argc; i++)
    g_ptr_array_add (arg_array, g_strdup (argv[i]));
  g_ptr_array_add (arg_array, NULL);
  char **args = (char **)g_ptr_array_free (arg_array, FALSE);

  int pid = pegg_call_host_command (connection, args, on_host_command_exited, NULL, NULL, &error);
  if (pid == -1)
    {
      g_printerr ("Could not call command: %s\n", error->message);
      return 1;
  }

  watch_signal (connection, pid, SIGHUP, TRUE);
  watch_signal (connection, pid, SIGINT, TRUE);
  watch_signal (connection, pid, SIGTERM, FALSE);

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
