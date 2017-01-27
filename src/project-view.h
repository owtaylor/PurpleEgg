#ifndef PROJECT_VIEW_H
#define PROJECT_VIEW_H

#include "project-window.h"

G_BEGIN_DECLS

#define PROJECT_TYPE_VIEW (project_view_get_type())

G_DECLARE_FINAL_TYPE (ProjectView, project_view, PROJECT, VIEW, GtkBin)

ProjectView *project_view_new (ProjectWindow *window,
                               const char    *directory);

const char *project_view_get_directory (ProjectView *view);
GtkWidget * project_view_get_titlebar  (ProjectView *self);

G_END_DECLS

#endif /* PROJECT_VIEW_H */

