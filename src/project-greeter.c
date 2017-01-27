#include <string.h>
#include <stdlib.h>

#include "host-command.h"
#include "project-tab.h"
#include "project-greeter.h"

struct _ProjectGreeter
{
  GtkBin parent_instance;
  ProjectWindow *window;
  GtkWidget *titlebar;
  GtkListBox *listbox;
};

G_DEFINE_TYPE (ProjectGreeter, project_greeter, GTK_TYPE_BIN)

static void
add_window_actions (ProjectGreeter *self);

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ProjectGreeter *
project_greeter_new (ProjectWindow *window)
{
  return g_object_new (PROJECT_TYPE_GREETER,
                       "window", window,
                       NULL);
}

static void
project_greeter_finalize (GObject *object)
{
  G_OBJECT_CLASS (project_greeter_parent_class)->finalize (object);
}

static void
project_greeter_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ProjectGreeter *self = PROJECT_GREETER (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
project_greeter_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ProjectGreeter *self = PROJECT_GREETER (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      self->window = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static int
compare_project_names (const void *a, const void *b)
{
  const char *name_a = *(char **)a;
  const char *name_b = *(char **)b;

  return g_utf8_collate (name_a, name_b);
}

static void
on_row_activated (GtkListBox     *listbox,
                  GtkListBoxRow  *row,
                  ProjectGreeter *self)
{
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (row));
  g_autofree char *directory = g_strdup (g_object_get_data (G_OBJECT (label), "project-directory"));
  project_window_set_directory (self->window, directory);
}

static void
project_greeter_constructed (GObject *object)
{
  ProjectGreeter *self = PROJECT_GREETER (object);
  ProjectApplication *application = PROJECT_APPLICATION (gtk_window_get_application (GTK_WINDOW (self->window)));

  add_window_actions (self);

  GError *error = NULL;
  g_auto(GStrv) projects = project_application_get_all_projects (application, &error);
  if (projects == NULL)
    {
      g_warning ("Failed to load projects: %s", error->message);
      g_clear_error (&error);
      goto out;
    }

  qsort(projects, g_strv_length (projects), sizeof(char *), compare_project_names);

  for (char **dirp = projects; *dirp; dirp++)
    {
      const char *dir = *dirp;
      g_autofree char *name = g_path_get_basename (dir);
      GtkWidget *label = gtk_label_new (name);
      g_object_set_data_full (G_OBJECT (label), "project-directory", g_strdup (dir), g_free);
      gtk_widget_show (label);
      gtk_container_add (GTK_CONTAINER (self->listbox), label);
    }

  g_signal_connect (self->listbox, "row-activated",
                    G_CALLBACK (on_row_activated), self);

 out:
  G_OBJECT_CLASS (project_greeter_parent_class)->constructed (object);
}

static void
project_greeter_destroy (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (project_greeter_parent_class)->destroy (widget);
}

static void
project_greeter_class_init (ProjectGreeterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_greeter_finalize;
  object_class->get_property = project_greeter_get_property;
  object_class->set_property = project_greeter_set_property;
  object_class->constructed = project_greeter_constructed;

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = project_greeter_destroy;

  properties[PROP_WINDOW] =
        g_param_spec_object ("window",
                             "Window",
                             "Window",
                             PROJECT_TYPE_WINDOW,
                             (G_PARAM_CONSTRUCT_ONLY |
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WINDOW,
                                   properties [PROP_WINDOW]);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/org/gnome/PurpleEgg/project-greeter.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectGreeter, titlebar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectGreeter, listbox);
}

static void
open_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  ProjectGreeter *self = user_data;

  GtkListBoxRow* row = gtk_list_box_get_selected_row (self->listbox);
  if (!row)
    return;

  GtkWidget *label = gtk_bin_get_child (GTK_BIN (row));
  g_autofree char *directory = g_strdup (g_object_get_data (G_OBJECT (label), "project-directory"));

  project_window_set_directory (self->window, directory);
}

static void
new_cb (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  ProjectGreeter *self = user_data;

  project_window_show_creator (self->window);
}

static const GActionEntry window_actions[] = {
    { "open", open_cb, NULL, NULL, NULL },
    { "new", new_cb, NULL, NULL, NULL },
  };

static void
add_window_actions (ProjectGreeter *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self->window),
                                   window_actions, G_N_ELEMENTS (window_actions),
                                   self);
}


static void
project_greeter_init (ProjectGreeter *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
project_greeter_get_titlebar (ProjectGreeter *self)
{
  return self->titlebar;
}
