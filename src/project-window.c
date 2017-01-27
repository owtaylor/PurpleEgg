#include <string.h>

#include "host-command.h"
#include "project-tab.h"
#include "project-view.h"
#include "project-greeter.h"
#include "project-window.h"

struct _ProjectWindow
{
  GtkApplicationWindow parent_instance;
  char *directory;
  gboolean constructed;
};

G_DEFINE_TYPE (ProjectWindow, project_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_DIRECTORY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ProjectWindow *
project_window_new (ProjectApplication *application,
                    const char         *directory)
{
  return g_object_new (PROJECT_TYPE_WINDOW,
                       "application", application,
                       "directory", directory,
                       "default_width", 600,
                       "default_height", 400,
                       NULL);
}

static void
project_window_finalize (GObject *object)
{
  ProjectWindow *self = (ProjectWindow *)object;

  g_free (self->directory);

  G_OBJECT_CLASS (project_window_parent_class)->finalize (object);
}

static void
project_window_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ProjectWindow *self = PROJECT_WINDOW (object);

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
project_window_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ProjectWindow *self = PROJECT_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      project_window_set_directory (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
project_window_constructed (GObject *object)
{
  ProjectWindow *self = PROJECT_WINDOW (object);

  if (self->directory == NULL)
    project_window_show_greeter (self);

  G_OBJECT_CLASS (project_window_parent_class)->constructed (object);
}

static void
project_window_class_init (ProjectWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_window_finalize;
  object_class->get_property = project_window_get_property;
  object_class->set_property = project_window_set_property;
  object_class->constructed = project_window_constructed;

  properties[PROP_DIRECTORY] =
        g_param_spec_string ("directory",
                             "Directory",
                             "Directory",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRECTORY,
                                   properties [PROP_DIRECTORY]);
}

static void
project_window_init (ProjectWindow *self)
{
}

const char *
project_window_get_directory (ProjectWindow *self)
{
  return self->directory;
}

static void
clear_children (ProjectWindow *self)
{
  GtkWidget *titlebar = gtk_window_get_titlebar (GTK_WINDOW (self));
  if (titlebar)
    gtk_widget_destroy (titlebar);

  GtkWidget *child = gtk_bin_get_child (GTK_BIN (self));
  if (child)
    gtk_widget_destroy (child);
}

void
project_window_set_directory (ProjectWindow *self,
                              const char    *directory)
{
  if (g_strcmp0 (self->directory, directory) == 0)
    return;

  clear_children (self);

  self->directory = g_strdup (directory);

  if (self->directory)
    {
      g_autofree char *basename = g_path_get_basename (self->directory);
      gtk_window_set_title (GTK_WINDOW (self), basename);

      ProjectView *view = project_view_new (self, directory);
      gtk_widget_show (GTK_WIDGET (view));
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (view));
      gtk_window_set_titlebar (GTK_WINDOW (self),
                               project_view_get_titlebar (view));
    }
  else
    {
      gtk_window_set_title (GTK_WINDOW (self), "PurpleEgg");
    }
}

void
project_window_show_greeter (ProjectWindow *self)
{
  project_window_set_directory (self, NULL);
  clear_children (self);

  ProjectGreeter *greeter = project_greeter_new (self);

  gtk_widget_show (GTK_WIDGET (greeter));
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (greeter));
  gtk_window_set_titlebar (GTK_WINDOW (self),
                           project_greeter_get_titlebar (greeter));
}

void
project_window_show_creator (ProjectWindow *self)
{
  project_window_set_directory (self, NULL);
  clear_children (self);
}
