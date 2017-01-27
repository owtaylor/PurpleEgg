#include "project-tab.h"
#include <vte/vte.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

struct _ProjectTab
{
  GtkBox parent_instance;
  GtkWidget *scrolled_window;
  GtkWidget *vte;
  char *directory;
  char *title;
};

G_DEFINE_TYPE (ProjectTab, project_tab, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DIRECTORY,
  N_PROPS
};

enum {
  TITLE_CHANGED,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL];
static GParamSpec *properties [N_PROPS];

ProjectTab *
project_tab_new (const char    *directory)
{
  return g_object_new (PROJECT_TYPE_TAB,
                       "directory", directory,
                       NULL);
}

static void
project_tab_finalize (GObject *object)
{
  ProjectTab *self = (ProjectTab *)object;

  g_free (self->title);

  G_OBJECT_CLASS (project_tab_parent_class)->finalize (object);
}

static void
project_tab_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ProjectTab *self = PROJECT_TAB (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_string (value, self->directory);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
project_tab_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ProjectTab *self = PROJECT_TAB (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_free (self->directory);
      self->directory = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
child_setup (gpointer user_data)
{
#if 0
  /* See note below, about why this isn't currently necessary - if we
   * disabled the use of 'bash -l' and PTY forwarding, then we would
   * need this.
   */

  /* VTE's main child setup takes control of the terminal, but we want
   * to leave that for the shell we run via HostCommand so that job
   * control works properly in that shell, so we reverse that and give
   * up control; propagating job control signals would be a less
   * finicky way to do this in some ways, but (among other issues),
   * having the terminal already owned by a different process causes
   * a warning from flatpak-session-helper.
   */

  /* Giving up the controlling terminal causes a SIGHUP */
  signal (SIGHUP, SIG_IGN);

  int fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  ioctl (fd, TIOCNOTTY, 0);
  close (fd);
#endif
}

static void
project_tab_constructed (GObject *object)
{
  ProjectTab *self = PROJECT_TAB (object);

  /* The use of bash here is to get the user's environment variables,
   * even if the session isn't run under a login shell; (mostly a
   * problem with older certain versions of GNOME+Wayland -see
   * https://bugzilla.gnome.org/show_bug.cgi?id=736660.) This doesn't
   * matter for the case where the interactive environment is a child
   * of docker, only for the un-sandboxed case. Note that because this
   * extra bash will take ownership of the TTY, we end up needing PTY
   * forwarding just like we do when pegg is run directly from a
   * terminal - without the invocation of bash here, we could let
   * flatpak-session-helper take ownership of the PTY opened by VTE.
   */
  const char *terminal_argv[] = { "/bin/bash", "-l", "-c", "exec " BINDIR "/pegg shell", NULL };
  const char *terminal_envv[] = { "PURPLEEGG=1", NULL };
  GError *error = NULL;

  vte_terminal_spawn_sync (VTE_TERMINAL (self->vte),
                           VTE_PTY_DEFAULT,
                           self->directory,
                           (char **)terminal_argv,
                           (char **)terminal_envv,
                           G_SPAWN_DEFAULT,
                           child_setup, /* child_setup */
                           NULL, /* child_setup_data */
                           NULL, /* child_pid_out */
                           NULL, /* cancellable */
                           &error);
}

static void
project_tab_get_preferred_width (GtkWidget *widget,
                                  int       *minimum_width,
                                  int       *natural_width)
{
  ProjectTab *self = PROJECT_TAB (widget);
  gtk_widget_get_preferred_width (self->vte, minimum_width, natural_width);
}

static void
project_tab_get_preferred_height (GtkWidget *widget,
                                  int       *minimum_height,
                                  int       *natural_height)
{
  ProjectTab *self = PROJECT_TAB (widget);
  gtk_widget_get_preferred_height (self->vte, minimum_height, natural_height);
}

static void
project_tab_class_init (ProjectTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_tab_finalize;
  object_class->get_property = project_tab_get_property;
  object_class->set_property = project_tab_set_property;
  object_class->constructed = project_tab_constructed;

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->get_preferred_width = project_tab_get_preferred_width;
  widget_class->get_preferred_height = project_tab_get_preferred_height;

  signals[TITLE_CHANGED] =
        g_signal_new ("title-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL,
                      NULL,
                      NULL,
                      G_TYPE_NONE,
                      0);

  properties [PROP_DIRECTORY] =
        g_param_spec_string ("directory",
                             "Directory",
                             "Directory",
                             NULL,
                             (G_PARAM_CONSTRUCT_ONLY |
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRECTORY,
                                   properties [PROP_DIRECTORY]);
}

static void
on_child_exited (VteTerminal   *vte,
                 int            status,
                 ProjectTab    *self)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
on_current_directory_uri_changed (VteTerminal *vte)
{
  const char *uri = vte_terminal_get_current_directory_uri (vte);
  g_autoptr(GFile) file = g_file_new_for_uri (uri);
}

static void
on_window_title_changed (VteTerminal *vte,
                         ProjectTab  *self)
{
  const char *title = vte_terminal_get_window_title (vte);
  g_free (self->title);
  self->title = g_strdup (title);
  g_signal_emit (self, signals[TITLE_CHANGED], 0);
}

GdkRGBA foreground_color = { 0.513725, 0.580392, 0.588235, 1 };
GdkRGBA background_color = { 0,        0.168627, 0.211764, 1 };

#define COLOR(r, g, b) { .red = (r) / 255.0, .green = (g) / 255.0, .blue = (b) / 255.0, .alpha = 1.0 }

GdkRGBA palette[] = {
    COLOR (0x07, 0x36, 0x42),
    COLOR (0xdc, 0x32, 0x2f),
    COLOR (0x85, 0x99, 0x00),
    COLOR (0xb5, 0x89, 0x00),
    COLOR (0x26, 0x8b, 0xd2),
    COLOR (0xd3, 0x36, 0x82),
    COLOR (0x2a, 0xa1, 0x98),
    COLOR (0xee, 0xe8, 0xd5),
    COLOR (0x00, 0x2b, 0x36),
    COLOR (0xcb, 0x4b, 0x16),
    COLOR (0x58, 0x6e, 0x75),
    COLOR (0x65, 0x7b, 0x83),
    COLOR (0x83, 0x94, 0x96),
    COLOR (0x6c, 0x71, 0xc4),
    COLOR (0x93, 0xa1, 0xa1),
    COLOR (0xfd, 0xf6, 0xe3)
};

static void
project_tab_init (ProjectTab *self)
{
  self->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  self->vte = vte_terminal_new ();

  vte_terminal_set_colors(VTE_TERMINAL (self->vte),
                          &foreground_color,
                          &background_color,
                          palette,
                          G_N_ELEMENTS(palette));

  self->title = g_strdup ("");

  gtk_container_add (GTK_CONTAINER(self), self->scrolled_window);
  gtk_widget_set_hexpand (self->scrolled_window, TRUE);
  gtk_widget_set_vexpand (self->scrolled_window, TRUE);
  gtk_widget_show (self->scrolled_window);
  gtk_container_add (GTK_CONTAINER(self->scrolled_window), self->vte);
  gtk_widget_show (self->vte);
  g_signal_connect (self->vte, "child-exited",
                    G_CALLBACK(on_child_exited), self);
  g_signal_connect (self->vte, "current-directory-uri-changed",
                    G_CALLBACK(on_current_directory_uri_changed), self);
  g_signal_connect (self->vte, "window-title-changed",
                    G_CALLBACK(on_window_title_changed), self);
}

void
project_tab_set_font_scale (ProjectTab *self,
                            double      font_scale)
{
  vte_terminal_set_font_scale (VTE_TERMINAL (self->vte), font_scale);
}

void
project_tab_get_size (ProjectTab *self,
                      int        *columns,
                      int        *rows)
{
  if (columns)
    *columns = vte_terminal_get_column_count (VTE_TERMINAL (self->vte));
  if (rows)
    *rows = vte_terminal_get_row_count (VTE_TERMINAL (self->vte));
}

void
project_tab_set_size (ProjectTab *self,
                      int         columns,
                      int         rows)
{
  vte_terminal_set_size (VTE_TERMINAL (self->vte), columns, rows);
}

const char *
project_tab_get_title (ProjectTab *self)
{
  return self->title;
}

void
project_tab_copy (ProjectTab *self)
{
  vte_terminal_copy_clipboard (VTE_TERMINAL (self->vte));
}

void
project_tab_paste (ProjectTab *self)
{
  vte_terminal_paste_clipboard (VTE_TERMINAL (self->vte));
}
