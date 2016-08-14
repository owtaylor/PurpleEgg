#ifndef PROJECT_WINDOW_H
#define PROJECT_WINDOW_H

#include "project-application.h"

G_BEGIN_DECLS

#define PROJECT_TYPE_WINDOW (project_window_get_type())

G_DECLARE_FINAL_TYPE (ProjectWindow, project_window, PROJECT, WINDOW, GtkApplicationWindow)

ProjectWindow *project_window_new (ProjectApplication *application,
                                   const char         *directory);

const char *project_window_get_directory (ProjectWindow *window);

G_END_DECLS

#endif /* PROJECT_WINDOW_H */

