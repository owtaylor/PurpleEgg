#ifndef PROJECT_TAB_H
#define PROJECT_TAB_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PROJECT_TYPE_TAB (project_tab_get_type())

G_DECLARE_FINAL_TYPE (ProjectTab, project_tab, PROJECT, TAB, GtkBox)

ProjectTab *project_tab_new (const char *directory);
const char *project_tab_get_title (ProjectTab *tab);

G_END_DECLS

#endif /* PROJECT_TAB_H */

