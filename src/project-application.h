#ifndef PROJECT_APPLICATION_H
#define PROJECT_APPLICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PROJECT_TYPE_APPLICATION (project_application_get_type())

G_DECLARE_FINAL_TYPE (ProjectApplication, project_application, PROJECT, APPLICATION, GtkApplication)

ProjectApplication *project_application_new (void);

G_END_DECLS

#endif /* PROJECT_APPLICATION_H */

