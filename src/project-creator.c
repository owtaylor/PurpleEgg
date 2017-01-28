#include <string.h>
#include <stdlib.h>

#include "project-creator.h"
#include "egg-radio-box.h"
#include "egg-three-grid.h"

struct _ProjectCreator
{
  GtkBin parent_instance;
  ProjectWindow *window;
  GtkWidget *titlebar;
};

G_DEFINE_TYPE (ProjectCreator, project_creator, GTK_TYPE_BIN)

static void
add_window_actions (ProjectCreator *self);

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ProjectCreator *
project_creator_new (ProjectWindow *window)
{
  return g_object_new (PROJECT_TYPE_CREATOR,
                       "window", window,
                       NULL);
}

static void
project_creator_finalize (GObject *object)
{
  G_OBJECT_CLASS (project_creator_parent_class)->finalize (object);
}

static void
project_creator_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ProjectCreator *self = PROJECT_CREATOR (object);

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
project_creator_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ProjectCreator *self = PROJECT_CREATOR (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      self->window = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
project_creator_constructed (GObject *object)
{
  ProjectCreator *self = PROJECT_CREATOR (object);

  add_window_actions (self);

  G_OBJECT_CLASS (project_creator_parent_class)->constructed (object);
}

static void
project_creator_destroy (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (project_creator_parent_class)->destroy (widget);
}

static void
project_creator_class_init (ProjectCreatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_creator_finalize;
  object_class->get_property = project_creator_get_property;
  object_class->set_property = project_creator_set_property;
  object_class->constructed = project_creator_constructed;

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = project_creator_destroy;

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

  egg_three_grid_get_type();
  egg_radio_box_get_type();

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/org/gnome/PurpleEgg/project-creator.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectCreator, titlebar);
}

static void
cancel_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  ProjectCreator *self = user_data;

  project_window_show_greeter (self->window);
}

static void
create_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
}

static const GActionEntry window_actions[] = {
    { "create", create_cb, NULL, NULL, NULL },
    { "cancel", cancel_cb, NULL, NULL, NULL },
  };

static void
add_window_actions (ProjectCreator *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self->window),
                                   window_actions, G_N_ELEMENTS (window_actions),
                                   self);
}


static void
project_creator_init (ProjectCreator *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
project_creator_get_titlebar (ProjectCreator *self)
{
  return self->titlebar;
}
