#include <string.h>

#include "host-command.h"
#include "project-tab.h"
#include "project-view.h"

struct _ProjectView
{
  GtkBin parent_instance;
  ProjectWindow *window;
  GtkWidget *titlebar;
  char *directory;
  GtkStack *stack;
  GtkLabel *title;
  GtkComboBoxText *git_combo;
  char *current_git_branch;
  GFileMonitor *git_head_monitor;
  GFileMonitor *git_refs_monitor;
  double font_scale;
  int rows;
  int columns;
};

G_DEFINE_TYPE (ProjectView, project_view, GTK_TYPE_BIN)

static void new_tab (ProjectView *self);
static void add_window_actions (ProjectView *self);

enum {
  PROP_0,
  PROP_WINDOW,
  PROP_DIRECTORY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ProjectView *
project_view_new (ProjectWindow *window,
                  const char    *directory)
{
  return g_object_new (PROJECT_TYPE_VIEW,
                       "window", window,
                       "directory", directory,
                       NULL);
}

static void
project_view_finalize (GObject *object)
{
  ProjectView *self = (ProjectView *)object;

  g_free (self->directory);

  G_OBJECT_CLASS (project_view_parent_class)->finalize (object);
}

static void
project_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ProjectView *self = PROJECT_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_string (value, self->directory);
      break;
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
project_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ProjectView *self = PROJECT_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_free (self->directory);
      self->directory = g_value_dup_string (value);
      break;
    case PROP_WINDOW:
      self->window = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static char **
list_git_branches (ProjectView *self,
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
get_git_branch (ProjectView   *self,
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
update_git (ProjectView *self)
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

  g_free (self->current_git_branch);
  self->current_git_branch = g_strdup (current_branch);

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
                ProjectView       *self)
{
  update_git (self);
}

static void
on_git_branch_done (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source_object);

  GError *error = NULL;
  g_autofree char *stdout_buffer = NULL;
  g_autofree char *stderr_buffer = NULL;
  if (!g_subprocess_communicate_utf8_finish (subprocess, result,
                                             &stdout_buffer, &stderr_buffer,
                                             &error))
    {
      g_printerr ("Error running git branch: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  if (g_subprocess_get_status (subprocess) != 0)
    {
      g_printerr ("git branch failed:\n%s\n", stderr_buffer);
    }
}

static void
on_git_combo_changed (GtkComboBox *combo,
                      ProjectView *self)
{
  const char *branch = gtk_combo_box_text_get_active_text (self->git_combo);
  if (branch && branch[0] &&
      self->current_git_branch &&
      strcmp (branch, self->current_git_branch) != 0)
    {
      g_autofree char *git_dir = g_build_filename (self->directory, ".git", NULL);
      g_autofree char *git_dir_arg = g_strconcat ("--git-dir=", git_dir, NULL);
      GError *error = NULL;
      GPtrArray *arg_array = g_ptr_array_new ();
      /* It would be cleaner to use pegg_run_host() directly rather than
       * starting an intermediate binary; if we do this repeatedly, we should
       * have a helper that uses GSubprocess or pegg_run_host() behind the
       * scenes depending on whether we are within flatpak.
       */
      if (pegg_in_flatpak ())
        g_ptr_array_add (arg_array, g_strdup (LIBEXECDIR "/pegg-run-host"));
      g_ptr_array_add (arg_array, g_strdup ("git"));
      g_ptr_array_add (arg_array, g_strdup (git_dir_arg));
      g_ptr_array_add (arg_array, g_strdup ("checkout"));
      g_ptr_array_add (arg_array, g_strdup (branch));
      g_ptr_array_add (arg_array, NULL);
      g_auto(GStrv) args = (char **)g_ptr_array_free (arg_array, FALSE);
      g_autoptr(GSubprocess) subprocess = g_subprocess_newv ((const char * const *)args,
                                                             G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                             G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                                             &error);
      if (!subprocess)
        {
          g_printerr ("Failed to exec git checkout%s\n", error->message);
          g_clear_error (&error);
          return;
        }

      g_subprocess_communicate_utf8_async (subprocess, NULL, NULL,
                                           on_git_branch_done, self);
    }
}

static void
project_view_constructed (GObject *object)
{
  ProjectView *self = PROJECT_VIEW (object);

  add_window_actions (self);

  g_autofree char *basename = g_path_get_basename (self->directory);
  g_autofree char *title = g_markup_printf_escaped ("<b>%s</b>", basename);
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

  G_OBJECT_CLASS (project_view_parent_class)->constructed (object);
}

static void
project_view_destroy (GtkWidget *widget)
{
  ProjectView *self = PROJECT_VIEW (widget);

  if (self->git_head_monitor)
    g_object_run_dispose (G_OBJECT (self->git_head_monitor));
  g_clear_object (&self->git_head_monitor);

  if (self->git_refs_monitor)
    g_object_run_dispose (G_OBJECT (self->git_refs_monitor));
  g_clear_object (&self->git_refs_monitor);

  GTK_WIDGET_CLASS (project_view_parent_class)->destroy (widget);
}

static void
project_view_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  ProjectView *self = PROJECT_VIEW (widget);

  GTK_WIDGET_CLASS (project_view_parent_class)->size_allocate (widget, allocation);

  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  if (children)
    {
      ProjectTab *first_tab = children->data;
      project_tab_get_size (first_tab, &self->columns, &self->rows);
      g_list_free (children);
    }

//  g_print ("cols=%d, rows=%d\n", self->columns, self->rows);
}

static void
project_view_class_init (ProjectViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = project_view_finalize;
  object_class->get_property = project_view_get_property;
  object_class->set_property = project_view_set_property;
  object_class->constructed = project_view_constructed;

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = project_view_destroy;
  widget_class->size_allocate = project_view_size_allocate;

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
                                               "/org/gnome/PurpleEgg/project-view.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectView, stack);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectView, title);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectView, titlebar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), ProjectView, git_combo);

}

static void
update_titles (ProjectView *self)
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
  ProjectView *self = user_data;

  gint32 tab;
  g_variant_get (value, "i", &tab);

  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  GtkWidget *child = g_list_nth_data (children, tab - 1);
  if (child != NULL)
    gtk_stack_set_visible_child (self->stack, child);

  g_list_free (children);
}

static void
on_tab_destroy (GObject     *object,
                ProjectView *self)
{
  update_titles (self);
}

static void
on_title_changed (ProjectTab  *tab,
                  ProjectView *self)
{
  update_titles (self);
}

static void
new_tab (ProjectView *self)
{
  ProjectTab *tab = project_tab_new (self->directory);
  project_tab_set_font_scale (tab, self->font_scale);
  gtk_widget_show (GTK_WIDGET (tab));
  gtk_container_add (GTK_CONTAINER (self->stack), GTK_WIDGET (tab));
  g_signal_connect (tab, "destroy",
                    G_CALLBACK (on_tab_destroy), self);
  g_signal_connect (tab, "title-changed",
                    G_CALLBACK (on_title_changed), self);
  update_titles (self);
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (tab));
}

static void
new_tab_cb (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  new_tab (user_data);
}

static void
copy_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  ProjectView *self = user_data;

  GtkWidget *current_tab = gtk_stack_get_visible_child (self->stack);
  if (current_tab)
    project_tab_copy (PROJECT_TAB (current_tab));
}

static void
paste_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  ProjectView *self = user_data;

  GtkWidget *current_tab = gtk_stack_get_visible_child (self->stack);
  if (current_tab)
    project_tab_paste (PROJECT_TAB (current_tab));
}

void
project_view_update_window_size (ProjectView *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  if (children)
    {
      for (GList *l = children; l; l = l->next)
        project_tab_set_size (l->data, self->columns, self->rows);
      g_list_free (children);

      GtkRequisition natural_size;
      GtkRequisition min_size;
      gtk_widget_get_preferred_size (GTK_WIDGET (self->stack), &min_size, &natural_size);
      gtk_window_resize (GTK_WINDOW (self->window), natural_size.width, natural_size.height);
    }
}

static void
update_font_sizes (ProjectView *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self->stack));
  for (GList *l = children; l; l = l->next)
      project_tab_set_font_scale (l->data, self->font_scale);

  project_view_update_window_size (self);

  g_list_free (children);

}

static void
increase_font_size_cb (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ProjectView *self = user_data;

  self->font_scale *= 1.2;
  update_font_sizes (self);
}

static void
decrease_font_size_cb (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ProjectView *self = user_data;

  self->font_scale /= 1.2;
  update_font_sizes (self);
}

static const GActionEntry window_actions[] = {
    { "set_active_tab", NULL, "i", "1", set_active_tab_cb },
    { "new_tab", new_tab_cb, NULL, NULL, NULL },
    { "copy", copy_cb, NULL, NULL, NULL },
    { "paste", paste_cb, NULL, NULL, NULL },
    { "increase_font_size", increase_font_size_cb, NULL, NULL, NULL },
    { "decrease_font_size", decrease_font_size_cb, NULL, NULL, NULL }
  };

static void
project_view_init (ProjectView *self)
{
  self->font_scale = 1.0;
  self->rows = 24;
  self->columns = 80;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->git_combo, "changed",
                    G_CALLBACK (on_git_combo_changed), self);
}

static void
add_window_actions (ProjectView *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self->window),
                                   window_actions, G_N_ELEMENTS (window_actions),
                                   self);
}


const char *
project_view_get_directory (ProjectView *self)
{
  return self->directory;
}

GtkWidget *
project_view_get_titlebar (ProjectView *self)
{
  return self->titlebar;
}
