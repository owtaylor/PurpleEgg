#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <termios.h>

#include <glib-unix.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include "host-command.h"

typedef struct {
  PeggHostCommandCallback callback;
  guint connection_id;
  int pid;
  gpointer user_data;
} HostCommandData;

static gboolean restore_stdin = FALSE;
static struct termios old_options;

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

void
pegg_make_stdin_raw (void)
{
  struct termios options;

  restore_stdin = TRUE;

  if (tcgetattr(0, &old_options) == -1)
    g_warning("Error getting TTY options: %s", g_strerror (errno));

  options = old_options;
  cfmakeraw(&options);

  if (tcsetattr(0, TCSANOW, &options) == -1)
    g_warning("Setting TTY to raw mode: %s", g_strerror (errno));
}

void
pegg_restore_stdin (void)
{
  if (restore_stdin)
    tcsetattr (0, TCSANOW, &old_options);
}

static void
on_eof (GObject      *source_object,
        GAsyncResult *result,
        gpointer      data)
{
  GError *error = NULL;
  g_output_stream_splice_finish(G_OUTPUT_STREAM(source_object), result, &error);
  if (error != NULL)
    {
      g_warning ("Error finishing splice: %s\n", error->message);
      g_clear_error (&error);
    }
}

static gboolean
add_pty_fd (GUnixFDList *fd_list,
            const char  *pty_path,
            mode_t       mode,
            GError     **error)
{
  int fd = open(pty_path, mode);
  if (fd == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   "Error opening slave fd (mode=%d): %s",
                   mode, g_strerror (errsv));
      return FALSE;
    }

  int handle = g_unix_fd_list_append (fd_list, fd, error);
  if (handle == -1)
    {
      (void)close (fd);
      return FALSE;
    }

  return TRUE;
}

static void
close_pipes(int *pipes)
{
  for (int i = 0; i < 6; i++)
    if (pipes[i] != -1)
      (void) close(pipes[i]);
}

static GUnixFDList *
prepare_fd_list (PeggHostCommandFlags flags,
                 int                 *stdin_handle,
                 int                 *stdout_handle,
                 int                 *stderr_handle,
                 GCancellable        *cancellable,
                 GError             **error)
{
  g_autoptr(GUnixFDList) fd_list = g_unix_fd_list_new ();
  int stdout_fd = -1;

  if (flags & PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL)
    {
      stdout_fd = open ("/dev/null", O_WRONLY);
      if (stdout_fd == -1)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       "Error opening /dev/null: %s",
                       g_strerror (errsv));
          goto cleanup;
        }
    }

  if (flags & PEGG_HOST_COMMAND_USE_PTY)
    {
      /* USE_PTY is for the case when we want to run a client
       * that connects to a terminal, but the current terminal
       * is already in use. We allocate a new PTY, set
       * the mode of the current terminal to raw, and forward
       * everything to the new terminal.
       */

      /* We'll forward stdin and stdout to the master */
      int pty_master_fd = posix_openpt (O_RDWR | O_NOCTTY);
      if (pty_master_fd == -1)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       "Error opening PTY master device: %s",
                       g_strerror (errsv));
          goto cleanup;
        }

      /* Now open the slave side of the PTY and use that for the command
       * we spawn */

      unlockpt (pty_master_fd);

      const char *pty_path = ptsname (pty_master_fd);
      *stdin_handle = add_pty_fd (fd_list, pty_path, O_RDONLY | O_NOCTTY, error);
      if (*stdin_handle == -1)
        goto cleanup;
      *stdout_handle = add_pty_fd (fd_list, pty_path, O_WRONLY | O_NOCTTY, error);
      if (*stdout_handle == -1)
        goto cleanup;
      *stderr_handle = add_pty_fd (fd_list, pty_path, O_WRONLY | O_NOCTTY, error);
      if (*stderr_handle == -1)
        goto cleanup;

      g_autoptr(GInputStream) stdin = g_unix_input_stream_new(0, FALSE);
      g_autoptr(GOutputStream) stdout = g_unix_output_stream_new(1, FALSE);
      g_autoptr(GInputStream) instream = g_unix_input_stream_new(pty_master_fd, FALSE);
      g_autoptr(GOutputStream) outstream = g_unix_output_stream_new(pty_master_fd, FALSE);

      g_output_stream_splice_async(stdout, instream,
                                   G_OUTPUT_STREAM_SPLICE_NONE,
                                   G_PRIORITY_DEFAULT, cancellable,
                                   on_eof, NULL);

      g_output_stream_splice_async(outstream, stdin,
                                   G_OUTPUT_STREAM_SPLICE_NONE,
                                   G_PRIORITY_DEFAULT, cancellable,
                                   on_eof, NULL);
    }
  else
    {
      fd_list = g_unix_fd_list_new ();

      /* In the case where we are using a PTY, we need to forward input
       * to the PTY. In the other case, we could normally just forward
       * the stdin/stdout/stderr FD's directly to the host command; but
       * this will produce a warning message with older versions flatpak,
       * which try to set ownership of the terminal in all cases; to work
       * around this, we use an intermediate pipe.
       *
       * (See https://github.com/flatpak/flatpak/pull/512)
       */
#if 0
      *stdin_handle = g_unix_fd_list_append (fd_list, 0, error);
      if (*stdin_handle == -1)
        goto cleanup;
      *stdout_handle = g_unix_fd_list_append (fd_list,
                                              (flags & PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL) ? stdout_fd : 1,
                                              error);
      if (*stdout_handle == -1)
        goto cleanup;
      stdout_fd = -1;
      *stderr_handle = g_unix_fd_list_append (fd_list, 2, error);
      if (*stderr_handle == -1)
        goto cleanup;
#else
      int pipes[6] = { -1, -1,  -1, -1,  -1, -1 };

      if (pipe(pipes) == -1 ||
          (!(flags & PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL) && pipe(pipes + 2) == -1) ||
          pipe(pipes + 4) == -1)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       "Error opening pipes for channels: %s",
                       g_strerror (errsv));
          close_pipes (pipes);
          goto cleanup;
        }

      *stdin_handle = g_unix_fd_list_append (fd_list, pipes[0], error);
      if (*stdin_handle == -1)
        {
          close_pipes (pipes);
          goto cleanup;
        }
      *stdout_handle = g_unix_fd_list_append (fd_list,
                                              (flags & PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL) ? stdout_fd : 1,
                                              error);
      if (*stdout_handle == -1)
        {
          close_pipes (pipes);
          goto cleanup;
        }
      stdout_fd = -1;
      *stderr_handle = g_unix_fd_list_append (fd_list, pipes[5], error);
      if (*stderr_handle == -1)
        {
          close_pipes (pipes);
          goto cleanup;
        }

      g_autoptr(GInputStream) stdin = g_unix_input_stream_new(0, FALSE);
      g_autoptr(GOutputStream) instream = g_unix_output_stream_new(pipes[1], FALSE);
      g_output_stream_splice_async(instream, stdin,
                                   G_OUTPUT_STREAM_SPLICE_NONE,
                                   G_PRIORITY_DEFAULT, cancellable,
                                   on_eof, NULL);

      if (!(flags & PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL))
        {
          g_autoptr(GOutputStream) stdout = g_unix_output_stream_new(1, FALSE);
          g_autoptr(GInputStream) outstream = g_unix_input_stream_new(pipes[2], FALSE);
          g_output_stream_splice_async(stdout, outstream,
                                       G_OUTPUT_STREAM_SPLICE_NONE,
                                       G_PRIORITY_DEFAULT, cancellable,
                                       on_eof, NULL);
        }

      g_autoptr(GOutputStream) stderr = g_unix_output_stream_new(2, FALSE);
      g_autoptr(GInputStream) errstream = g_unix_input_stream_new(pipes[4], FALSE);
      g_output_stream_splice_async(stderr, errstream,
                                   G_OUTPUT_STREAM_SPLICE_NONE,
                                   G_PRIORITY_DEFAULT, cancellable,
                                   on_eof, NULL);


#endif
    }

  return g_object_ref (fd_list);

 cleanup:
  if (stdout_fd != -1)
    close (stdout_fd);

  return NULL;
}

int
pegg_call_host_command (GDBusConnection          *connection,
                        char                    **args,
                        PeggHostCommandFlags      flags,
                        PeggHostCommandCallback   callback,
                        gpointer                  user_data,
                        GCancellable             *cancellable,
                        GError                  **error)
{
  int stdin_handle, stdout_handle, stderr_handle;
  g_autoptr(GUnixFDList) fd_list = prepare_fd_list (flags,
                                                    &stdin_handle, &stdin_handle, &stderr_handle,
                                                    cancellable, error);
  if (!fd_list)
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
                                              g_get_current_dir (),
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
