#ifndef PROJECT_CREATOR_H
#define PROJECT_CREATOR_H

#include "project-window.h"

G_BEGIN_DECLS

#define PROJECT_TYPE_CREATOR (project_creator_get_type())

G_DECLARE_FINAL_TYPE (ProjectCreator, project_creator, PROJECT, CREATOR, GtkBin)

ProjectCreator *project_creator_new (ProjectWindow *window);

GtkWidget * project_creator_get_titlebar  (ProjectCreator *self);

G_END_DECLS

#endif /* PROJECT_CREATOR_H */

