#include <string.h>

#include "project-tab.h"
#include "project-window.h"

struct _ProjectWindow
{
  GtkApplicationWindow parent_instance;
  char *directory;
  GtkStack *stack;
  GtkLabel *title;
  GtkComboBoxText *git_combo;
  GFileMonitor *git_head_monitor;
  GFileMonitor *git_refs_monitor;
};

G_DEFINE_TYPE (ProjectWindow, project_window, GTK_TYPE_APPLICATION_WINDOW)

static void new_tab (ProjectWindow *self);

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
                       NULL);
}

static void
project_window_finalize (GObject *object)
{
  ProjectWindow *self = (ProjectWindow *)object;

  /* FIXME: should run-dispose these on destroy */
  g_clear_object (&self->git_head_monitor);
  g_clear_object (&self->git_refs_monitor);
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
      g_free (self->directory);
      self->directory = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static char **
list_git_branches (ProjectWindow *self,
		   GError       **error)
{
  GPtrArray *results = g_ptr_array_new();

  g_autofree char *git_refs = g_build_filename (self->directory, ".git", "refs", "heads", NULL);
  g_autoptr(GFile) git_refs_file = g_file_new_for_path (git_refs);

  g_autoptr(GFileEnumerator) enumerator = g_file_enumerate_children (git_refs_file,
                                                                     "standard::name",
                                                                     G_FILE_QUERY_INFO_NONE,
                                                                     NULL, error);
  while (TRUE)
    {
      g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, NULL, error);
      if (info == NULL)
        {
          if (*error)
            goto fail;
          break;
        }

      g_ptr_array_add (results, g_strdup (g_file_info_get_name (info)));
    }

  if (!g_file_enumerator_close (enumerator, NULL, error))
    goto fail;

  g_ptr_array_add (results, NULL);
  return (char **)g_ptr_array_free (results, FALSE);

fail:
  g_ptr_array_free (results, TRUE);
  return NULL;
}

static char *
get_git_branch (ProjectWindow *self,
	        GError       **error)
{
  g_autofree char *git_head = g_build_filename (self->directory, ".git", "HEAD", NULL);
  g_autoptr(GFile) git_head_file = g_file_new_for_path (git_head);

  g_autofree char *contents = NULL;
  if (!g_file_load_contents (git_head_file, NULL,
			     &contents, NULL, NULL,
			     error))
    return NULL;

  g_strstrip (contents);

  if (g_str_has_prefix (contents, "ref: refs/heads/"))
    return g_strdup (contents + strlen("ref: refs/heads/"));
  else
    return NULL;
}

static void
update_git (ProjectWindow *self)
{
  GError *error = NULL;

  g_auto(GStrv) branches = list_git_branches (self, &error);
  if (error)
    {
      g_clear_error (&error);
      gtk_widget_hide (GTK_WIDGET (self->git_combo));
      return;
    }
  g_autofree char *current_branch  = get_git_branch (self, &error);
  if (error)
    g_clear_error (&error);

  gtk_widget_show (GTK_WIDGET (self->git_combo));
  gtk_combo_box_text_remove_all (self->git_combo);

  for (int i = 0; branches[i]; i++)
    {
      gtk_combo_box_text_append_text (self->git_combo, branches[i]);
      if (g_strcmp0 (branches[i], current_branch) == 0)
	gtk_combo_box_set_active (GTK_COMBO_BOX (self->git_combo), i);
    }
}

static void
on_git_changed (GFileMonitor      *monitor,
       	        GFile             *file,
                GFile             *other_file,
                GFileMonitorEvent  event_type,
                ProjectWindow     *self)
{
  update_git (self);
}

static void
project_window_constructed (GObject *object)
{
  ProjectWindow *self = PROJECT_WINDOW (object);

  g_autofree char *basename = g_path_get_basename (self->directory);
  g_autofree char *title = g_markup_printf_escaped ("<b>%s</b>", basename);
  gtk_window_set_title (GTK_WINDOW (self), basename);
  gtk_label_set_markup (self->title, title);

  new_tab (self);

  GError *error = NULL;

  g_autofree char *git_head = g_build_filename (self->directory, ".git", "HEAD", NULL);
  g_autoptr(GFile) git_head_file = g_file_new_for_path (git_head);
  self->git_head_monitor = g_file_monitor_file (git_head_file,
                                                G_FILE_MONITOR_NONE,
					        NULL, &error);
  if (self->git_head_monitor)
    {
      g_signal_connect (self->git_head_monitor, "changed",
			G_CALLBACK (on_git_changed), self);
    }
  else
    {
      g_warning ("Can't monitor .git/HEAD: %s\n", error->message);
      g_clear_error (&error);
    }

  g_autofree char *git_refs = g_build_filename (self->directory, ".git", "refs", "heads", NULL);
  g_autoptr(GFile) git_refs_file = g_file_new_for_path (git_refs);
  self->git_refs_monitor = g_file_monitor_directory (git_refs_file,
                                                     G_FILE_MONITOR_NONE,
					             NULL, &error);
  if (self->git_refs_monitor)
    {
      g_signal_connect (self->git_refs_monitor, "changed",
			G_CALLBACK (on_git_changed), self);
    }
  else
    {
      g_warning ("Can't monitor .git/refs/heads: %s\n", error->message);
      g_clear_error (&error);
    }

  update_git (self);

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
  	                     (G_PARAM_CONSTRUCT_ONLY |
			      G_PARAM_READWRITE |
  	                      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRECTORY,
                                   properties [PROP_DIRECTORY]);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/org/gnome/NewTerm/project-window.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectWindow, stack);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectWindow, title);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectWindow, git_combo);
}

static void
update_titles (ProjectWindow *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  GList *l;
  int i = 1;
  for (l = children; l; l = l->next)
    {
      const char *title = project_tab_get_title (l->data);
      g_autofree char *to_set = NULL;
      if (*title != '\0')
      	to_set = g_strdup_printf ("%d %s", i, title);
      else
	to_set = g_strdup_printf ("%d", i);
      gtk_container_child_set (GTK_CONTAINER (self->stack), l->data,
			       "title", to_set,
			       NULL);
      i++;
    }

  g_list_free (children);
}

static void
set_active_tab_cb (GSimpleAction *action,
                   GVariant      *value,
                   gpointer       user_data)
{
  ProjectWindow *self = user_data;

  gint32 tab;
  g_variant_get (value, "i", &tab);

  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  GtkWidget *child = g_list_nth_data (children, tab - 1);
  if (child != NULL)
    gtk_stack_set_visible_child (self->stack, child);

  g_list_free (children);
}

static void
on_tab_destroy (GObject       *object,
	        ProjectWindow *self)
{
  update_titles (self);
}

static void
on_title_changed (ProjectTab    *tab,
	          ProjectWindow *self)
{
  update_titles (self);
}

static void
new_tab (ProjectWindow *self)
{
  ProjectTab *tab = project_tab_new (self->directory);
  gtk_widget_show (GTK_WIDGET (tab));
  gtk_container_add (GTK_CONTAINER (self->stack), GTK_WIDGET (tab));
  g_signal_connect (tab, "destroy",
		    G_CALLBACK (on_tab_destroy), self);
  g_signal_connect (tab, "title-changed",
		    G_CALLBACK (on_title_changed), self);
  update_titles (self);
}

static void
new_tab_cb (GSimpleAction *action,
	    GVariant      *parameter,
	    gpointer       user_data)
{
  new_tab (user_data);
}

static const GActionEntry window_actions[] = {
    { "set_active_tab", NULL, "i", "1", set_active_tab_cb },
    { "new_tab", new_tab_cb, NULL, NULL, NULL }
  };

static void
project_window_init (ProjectWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   window_actions, G_N_ELEMENTS (window_actions),
                                   self);
}

const char *
project_window_get_directory (ProjectWindow *self)
{
  return self->directory;
}
