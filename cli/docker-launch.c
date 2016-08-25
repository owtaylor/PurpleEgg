#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

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
  if (tmpdir != NULL)
    {
      unlink (g_build_filename (tmpdir, "cid", NULL));
      rmdir (tmpdir);
    }
}

int
main(int argc, char **argv)
{
  signal (SIGHUP, SIG_IGN);
  signal (SIGINT, SIG_IGN);
  signal (SIGTERM, SIG_IGN);

  if (argc < 2)
    {
      g_printerr ("First argument must be the file descriptor to wait on\n");
      goto fail;
    }

  int wait_fd = atoi(argv[1]);
  GInputStream *input = g_unix_input_stream_new (wait_fd, TRUE);
  g_input_stream_read_async (input, buffer, 1, G_PRIORITY_DEFAULT, NULL, on_input, NULL);

  GError *error = NULL;
  tmpdir = g_dir_make_tmp ("pegg.XXXXXX", &error);
  if (!tmpdir)
    {
      g_printerr ("Can't create temporary directory for container ID: %s\n", error->message);
      goto fail;
    }
  char *cidfile = g_build_filename (tmpdir, "cid", NULL);

  GPtrArray *arg_array = g_ptr_array_new();

  g_ptr_array_add(arg_array, "docker");
  g_ptr_array_add(arg_array, "run");
  g_ptr_array_add(arg_array, g_strconcat("--cidfile=", cidfile, NULL));

  for (int i = 2; i < argc; i++)
    g_ptr_array_add(arg_array, argv[i]);

  char **subprocess_args = (char **)g_ptr_array_free (arg_array, FALSE);

  GSubprocess *docker_run = g_subprocess_newv ((const char *const *)subprocess_args,
                                               G_SUBPROCESS_FLAGS_STDIN_INHERIT, &error);
  if (!docker_run)
    {
      g_printerr ("Can't execute docker-run: %s\n", error->message);
      goto fail;
    }
  g_subprocess_wait_async (docker_run, NULL, on_subprocess_exited, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  GFile *f = g_file_new_for_path (cidfile);
  char *contents = NULL;
  if (!g_file_load_contents  (f, NULL, &contents, NULL, NULL, &error))
    {
      g_printerr ("Can't load the container ID: %s\n", error->message);
      goto fail;
    }

  GSubprocess *docker_rm = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                             G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                             &error,
                                             "docker", "rm", "-f", contents, NULL);
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

  cleanup();
  return 0;

 fail:
  cleanup();
  return 1;
}
