#ifndef PROJECT_TAB_H
#define PROJECT_TAB_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PROJECT_TYPE_TAB (project_tab_get_type())

G_DECLARE_FINAL_TYPE (ProjectTab, project_tab, PROJECT, TAB, GtkBox)

ProjectTab *project_tab_new (const char *directory);

void        project_tab_set_font_scale (ProjectTab *tab,
					double      font_scale);
void        project_tab_get_size       (ProjectTab *tab,
					int        *columns,
					int        *rows);
void        project_tab_set_size       (ProjectTab *tab,
					int         columns,
					int         rows);

const char *project_tab_get_title (ProjectTab *tab);
void        project_tab_copy      (ProjectTab *tab);
void        project_tab_paste     (ProjectTab *tab);

G_END_DECLS

#endif /* PROJECT_TAB_H */

