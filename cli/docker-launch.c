#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include "host-command.h"

static GMainLoop *loop;
static char *tmpdir;
static char buffer[1];

static void
on_subprocess_exited (GObject       *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error = NULL;
  g_subprocess_wait_finish (G_SUBPROCESS (source_object),
                            res, &error);
  g_main_loop_quit (loop);
}

static void
on_host_command_exited (int      pid,
                        int      status,
                        gpointer user_data)
{
  g_main_loop_quit (loop);
}

static void
on_input (GObject      *source_object,
          GAsyncResult *res,
          gpointer      user_data)
{
  GError *error = NULL;
  gssize result = g_input_stream_read_finish (G_INPUT_STREAM (source_object),
                                              res, &error);
  if (result == -1)
    {
      g_printerr("Error reading from fd: %s\n", error->message);
      exit(1);
    }

  g_main_loop_quit (loop);
}

static void
cleanup (void)
{
  pegg_restore_stdin ();

  if (tmpdir != NULL)
    {
      unlink (g_build_filename (tmpdir, "cid", NULL));
      rmdir (tmpdir);
    }
}

int
main(int argc, char **argv)
{
  GError *error = NULL;
  gboolean use_pty = FALSE;

  signal (SIGHUP, SIG_IGN);
  signal (SIGINT, SIG_IGN);
  signal (SIGTERM, SIG_IGN);

  int first_arg = 1;

  if (argc > 1 && strcmp (argv[1], "--pty") == 0)
    {
      first_arg++;
      use_pty = TRUE;
    }

  if (argc < first_arg + 1)
    {
      g_printerr ("First argument must be the file descriptor to wait on\n");
      goto fail;
    }

  int wait_fd = atoi(argv[first_arg]);
  GInputStream *input = g_unix_input_stream_new (wait_fd, TRUE);
  g_input_stream_read_async (input, buffer, 1, G_PRIORITY_DEFAULT, NULL, on_input, NULL);

  if (pegg_in_flatpak ())
    {
      /* Hacky: *if* the host system uses XDG_RUNTIME_DIR of the
       * form /run/user/<pid>, then g_get_user_runtime_dir()/app/<app_id>
       * will be a shared between host and flatpak runtime. If not, this
       * simply won't work. Pass the CID via an extra FD? Use a home directory
       * path?
       */
      tmpdir = g_build_filename (g_get_user_runtime_dir (),
                                 "app",
                                 "org.gnome.PurpleEgg",
                                 "pegg.XXXXXX",
                                 NULL);
      if (!mkdtemp (tmpdir))
        {
          g_printerr ("Can't create temporary directory %s for container ID: %s\n", tmpdir, strerror (errno));
          goto fail;
        }
    }
  else
    {
      tmpdir = g_dir_make_tmp ("pegg.XXXXXX", &error);
      if (!tmpdir)
        {
          g_printerr ("Can't create temporary directory for container ID: %s\n", error->message);
          goto fail;
        }
    }

  char *cidfile = g_build_filename (tmpdir, "cid", NULL);

  GPtrArray *arg_array = g_ptr_array_new();

  g_ptr_array_add(arg_array, "docker");
  g_ptr_array_add(arg_array, "run");
  g_ptr_array_add(arg_array, g_strconcat("--cidfile=", cidfile, NULL));

  for (int i = first_arg + 1; i < argc; i++)
    g_ptr_array_add(arg_array, argv[i]);

  g_ptr_array_add (arg_array, NULL);
  char **subprocess_args = (char **)g_ptr_array_free (arg_array, FALSE);

  GDBusConnection *connection = NULL;
  if (pegg_in_flatpak ())
    {
      if (use_pty)
        pegg_make_stdin_raw ();

      connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

      int pid = pegg_call_host_command (connection,
                                        subprocess_args,
                                        use_pty ? PEGG_HOST_COMMAND_USE_PTY : PEGG_HOST_COMMAND_NONE,
                                        on_host_command_exited,
                                        NULL, NULL, &error);
      if (pid == -1)
        {
          g_printerr ("Can't execute docker-run on host: %s\n", error->message);
          goto fail;
        }
    }
  else
    {
      GSubprocess *docker_run = g_subprocess_newv ((const char *const *)subprocess_args,
                                                   G_SUBPROCESS_FLAGS_STDIN_INHERIT, &error);
      if (!docker_run)
        {
          g_printerr ("Can't execute docker-run: %s\n", error->message);
          goto fail;
        }
      g_subprocess_wait_async (docker_run, NULL, on_subprocess_exited, NULL);
    }

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  GFile *f = g_file_new_for_path (cidfile);
  char *contents = NULL;
  if (!g_file_load_contents  (f, NULL, &contents, NULL, NULL, &error))
    {
      g_printerr ("Can't load the container ID: %s\n", error->message);
      goto fail;
    }

  const char * const rm_args[] = {
    "docker", "rm", "-f", contents, NULL
  };

  if (pegg_in_flatpak ())
    {
      int pid = pegg_call_host_command (connection,
                                        (char **)rm_args,
                                        PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL,
                                        on_host_command_exited,
                                        NULL, NULL, &error);
      if (pid == -1)
        {
          g_printerr ("Can't execute docker-rm on host: %s\n", error->message);
          goto fail;
        }

      loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (loop);
    }
  else
    {
      GSubprocess *docker_rm = g_subprocess_newv (rm_args,
                                                  G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                                  &error);
      if (!docker_rm)
        {
          g_printerr ("Can't execute docker-rm: %s\n", error->message);
          goto fail;
        }

      char *stderr;
      char *stdout;
      if (!g_subprocess_communicate_utf8 (docker_rm, NULL, NULL, &stderr, &stdout, &error))
        {
          g_printerr ("Error waiting for docker-rm: %s\n", error->message);
          goto fail;
        }
    }

  cleanup();
  return 0;

 fail:
  cleanup();
  return 1;
}
