#include <string.h>

#include "project-application.h"
#include "project-window.h"
#include "introspection.h"
#include "search-generated.h"

struct _ProjectApplication
{
        GtkApplication parent_instance;
  guint registration_id;
  SearchProvider2 *search_skeleton;
};

G_DEFINE_TYPE (ProjectApplication, project_application, GTK_TYPE_APPLICATION)

enum {
        PROP_0,
        N_PROPS
};

static void create_or_activate_window (ProjectApplication *self,
                                       const char         *directory,
                                       guint32             timestamp);

ProjectApplication *
project_application_new (void)
{
        return g_object_new (PROJECT_TYPE_APPLICATION,
                       "application-id", "org.gnome.PurpleEgg",
                       NULL);
}

static void
project_application_finalize (GObject *object)
{
        // ProjectApplication *self = (ProjectApplication *)object;

        G_OBJECT_CLASS (project_application_parent_class)->finalize (object);
}

static void
project_application_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        // ProjectApplication *self = PROJECT_APPLICATION (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
project_application_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        // ProjectApplication *self = PROJECT_APPLICATION (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static char **
get_matching_projects (ProjectApplication *application,
                       const char *const  *terms,
                       GError            **error)
{
  GPtrArray *results = g_ptr_array_new();

  g_autoptr(GFile) projects_file = g_file_new_for_path ("/home/otaylor/Projects");

  g_autoptr(GFileEnumerator) enumerator = g_file_enumerate_children (projects_file,
                                                                     "standard::name",
                                                                     G_FILE_QUERY_INFO_NONE,
                                                                     NULL, error);
  g_auto(GStrv) folded_terms = g_new (char *, g_strv_length ((char **)terms));
  int i = 0;
  for (i = 0; terms[i]; i++)
    {
      folded_terms[i] = g_utf8_casefold(terms[i], -1);
    }
  folded_terms[i] = NULL;

  while (TRUE)
    {
      g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, NULL, error);
      if (info == NULL)
        {
          if (*error)
            goto fail;
          break;
        }

      const char *name = g_file_info_get_name (info);
      g_autofree char *folded_name = g_utf8_casefold (name, -1);
      gboolean matches = TRUE;
      for (char **term = folded_terms; matches && *term; term++)
        {
          if (strstr(folded_name, *term) == NULL)
            matches = FALSE;
        }

      if (matches)
        {
          g_autoptr(GFile) child = g_file_get_child (projects_file, name);
          g_ptr_array_add (results, g_file_get_path(child));
        }
    }

  if (!g_file_enumerator_close (enumerator, NULL, error))
    goto fail;

  g_ptr_array_add (results, NULL);
  return (char **)g_ptr_array_free (results, FALSE);

fail:
  g_ptr_array_free (results, TRUE);
  return NULL;
}

static gboolean
handle_get_initial_result_set (SearchProvider2       *object,
                               GDBusMethodInvocation *invocation,
                               const gchar *const    *arg_terms,
                               ProjectApplication    *self)
{
  GError *error = NULL;
  g_auto(GStrv) results = get_matching_projects (self, arg_terms, &error);
  if (!results)
    {
      g_dbus_method_invocation_take_error (invocation, error);
      return TRUE;
    }

  search_provider2_complete_get_initial_result_set (object,
                                                    invocation,
                                                    (const char *const *)results);

  return TRUE;
}

static gboolean
handle_get_subsearch_result_set (SearchProvider2       *object,
                                 GDBusMethodInvocation *invocation,
                                 const gchar *const    *arg_previous_results,
                                 const gchar *const    *arg_terms,
                                 ProjectApplication    *self)
{
  GError *error = NULL;
  g_auto(GStrv) results = get_matching_projects (self, arg_terms, &error);
  if (!results)
    {
      g_dbus_method_invocation_take_error (invocation, error);
      return TRUE;
    }
  search_provider2_complete_get_subsearch_result_set (object,
                                                      invocation,
                                                      (const char *const *)results);

  return TRUE;
}

static gboolean
handle_get_result_metas (SearchProvider2       *object,
                         GDBusMethodInvocation *invocation,
                         const gchar *const    *arg_identifiers,
                         ProjectApplication    *self)
{
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (const char *const *identifier = arg_identifiers; *identifier; identifier++)
    {
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}", "id", g_variant_new ("s", *identifier));
      g_autofree char *basename = g_path_get_basename (*identifier);
      g_variant_builder_add (&builder, "{sv}", "name", g_variant_new ("s", basename));
      g_variant_builder_add (&builder, "{sv}", "description", g_variant_new ("s", *identifier));
      g_variant_builder_add (&builder, "{sv}", "gicon", g_variant_new ("s", "folder"));
      g_variant_builder_close (&builder);
    }

  search_provider2_complete_get_result_metas (object,
                                              invocation,
                                              g_variant_builder_end (&builder));

  return TRUE;
}

static gboolean
handle_activate_result (SearchProvider2       *object,
                        GDBusMethodInvocation *invocation,
                        const gchar           *arg_identifier,
                        const gchar *const    *arg_terms,
                        guint                  arg_timestamp,
                        ProjectApplication    *self)
{
  create_or_activate_window (self, arg_identifier, arg_timestamp);
  search_provider2_complete_activate_result (object,
                                             invocation);
  return TRUE;
}

static gboolean
handle_launch_search (SearchProvider2       *object,
                      GDBusMethodInvocation *invocation,
                      const gchar *const    *arg_terms,
                      guint                  arg_timestamp,
                      ProjectApplication    *application)
{
  return FALSE;
}

static gboolean
project_application_dbus_register (GApplication    *application,
                                   GDBusConnection *connection,
                                   const gchar     *object_path,
                                   GError         **error)
{
  ProjectApplication *self = PROJECT_APPLICATION (application);

  if (!G_APPLICATION_CLASS (project_application_parent_class)->dbus_register (application,
                                                                              connection,
                                                                              object_path,
                                                                              error))
    return FALSE;

  self->search_skeleton = search_provider2_skeleton_new();

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->search_skeleton),                                         connection,
                                         SEARCH_PATH,
                                         error))
    {
      g_clear_object (&self->search_skeleton);
      return FALSE;
    }

  g_signal_connect (self->search_skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->search_skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->search_skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->search_skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);
  g_signal_connect (self->search_skeleton, "handle-launch-search",
                    G_CALLBACK (handle_launch_search), self);

  return TRUE;
}

static void
project_application_dbus_unregister (GApplication    *application,
                                     GDBusConnection *connection,
                                     const gchar     *object_path)
{
  ProjectApplication *self = PROJECT_APPLICATION (application);

  if (self->search_skeleton)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->search_skeleton));
      g_clear_object (&self->search_skeleton);
    }

  G_APPLICATION_CLASS (project_application_parent_class)->dbus_unregister (application,
                                                                           connection,
                                                                           object_path);
}

static void
create_or_activate_window (ProjectApplication *self,
                           const char         *directory,
                           guint32             timestamp)
{
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (self));
  for (GList *l = windows; l; l = l->next)
    {
      if (g_strcmp0 (project_window_get_directory (l->data), directory) == 0)
        {
          gtk_window_present_with_time (l->data, timestamp);
          return;
        }
    }

  ProjectWindow *window = project_window_new (self, directory);
  gtk_window_present_with_time (GTK_WINDOW (window), timestamp);
}

static void
project_application_activate (GApplication *application)
{
  ProjectApplication *self = PROJECT_APPLICATION (application);
  create_or_activate_window (self, "/home/otaylor/Projects/angular-phonecat",
                             GDK_CURRENT_TIME);
}

static void
project_application_class_init (ProjectApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = project_application_finalize;
  object_class->get_property = project_application_get_property;
  object_class->set_property = project_application_set_property;

  application_class->activate = project_application_activate;
  application_class->dbus_register = project_application_dbus_register;
  application_class->dbus_unregister = project_application_dbus_unregister;
}

static const char *tab1_accels[] = { "<Alt>1", NULL };
static const char *tab2_accels[] = { "<Alt>2", NULL };
static const char *tab3_accels[] = { "<Alt>3", NULL };
static const char *tab4_accels[] = { "<Alt>4", NULL };
static const char *tab5_accels[] = { "<Alt>5", NULL };
static const char *tab6_accels[] = { "<Alt>6", NULL };
static const char *tab7_accels[] = { "<Alt>7", NULL };
static const char *tab8_accels[] = { "<Alt>8", NULL };
static const char *tab9_accels[] = { "<Alt>9", NULL };
static const char *tab10_accels[] = { "<Alt>0", NULL };
static const char *new_tab_accels[] = { "<Control><Shift>t", NULL };
static const char *copy_accels[] = { "<Control><Shift>c", NULL };
static const char *paste_accels[] = { "<Control><Shift>v", NULL };
static const char *increase_font_size_accels[] = { "<Control>plus", NULL };
static const char *decrease_font_size_accels[] = { "<Control>minus", NULL };

static void
project_application_init (ProjectApplication *self)
{
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(1)",
                                         tab1_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(2)",
                                         tab2_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(3)",
                                         tab3_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(4)",
                                         tab4_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(5)",
                                         tab5_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(6)",
                                         tab6_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(7)",
                                         tab7_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(8)",
                                         tab8_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(9)",
                                         tab9_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.set_active_tab(10)",
                                         tab10_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.new_tab",
                                         new_tab_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.copy",
                                         copy_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.paste",
                                         paste_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.increase_font_size",
                                         increase_font_size_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.decrease_font_size",
                                         decrease_font_size_accels);
}
