#ifndef PROJECT_GREETER_H
#define PROJECT_GREETER_H

#include "project-window.h"

G_BEGIN_DECLS

#define PROJECT_TYPE_GREETER (project_greeter_get_type())

G_DECLARE_FINAL_TYPE (ProjectGreeter, project_greeter, PROJECT, GREETER, GtkBin)

ProjectGreeter *project_greeter_new (ProjectWindow *window);

GtkWidget * project_greeter_get_titlebar  (ProjectGreeter *self);

G_END_DECLS

#endif /* PROJECT_GREETER_H */

