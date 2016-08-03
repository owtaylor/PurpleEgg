#include "project-tab.h"
#include <vte/vte.h>

struct _ProjectTab
{
  GtkBox parent_instance;
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
project_tab_constructed (GObject *object)
{
  ProjectTab *self = PROJECT_TAB (object);
  const char *terminal_argv[] = { "/bin/bash", "-l", "-c", "exec fedenv shell", NULL };
  const char *terminal_envv[] = { "NEWTERM=1", NULL };
  GError *error = NULL;

  vte_terminal_spawn_sync (VTE_TERMINAL (self->vte),
                           VTE_PTY_DEFAULT,
                           self->directory,
                           (char **)terminal_argv,
			   (char **)terminal_envv,
                           G_SPAWN_DEFAULT,
                           NULL, /* child_setup */
                           NULL, /* child_setup_data */
                           NULL, /* child_pid_out */
                           NULL, /* cancellable */
                           &error);
}

static void
project_tab_class_init (ProjectTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_tab_finalize;
  object_class->get_property = project_tab_get_property;
  object_class->set_property = project_tab_set_property;
  object_class->constructed = project_tab_constructed;

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
  g_print("%s\n", g_file_get_path (file));
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
  GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  self->vte = vte_terminal_new ();

  vte_terminal_set_colors(VTE_TERMINAL (self->vte),
                          &foreground_color,
			  &background_color,
			  palette,
			  G_N_ELEMENTS(palette));

  self->title = g_strdup ("");

  gtk_container_add (GTK_CONTAINER(self), scrolled_window);
  gtk_widget_set_hexpand (scrolled_window, TRUE);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_widget_show (scrolled_window);
  gtk_container_add (GTK_CONTAINER(scrolled_window), self->vte);
  gtk_widget_show (self->vte);
  g_signal_connect (self->vte, "child-exited",
                    G_CALLBACK(on_child_exited), self);
  g_signal_connect (self->vte, "current-directory-uri-changed",
                    G_CALLBACK(on_current_directory_uri_changed), self);
  g_signal_connect (self->vte, "window-title-changed",
                    G_CALLBACK(on_window_title_changed), self);
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
