#include "project-window.h"

int
main(int argc, char **argv)
{
  gtk_init (&argc, &argv);

  ProjectApplication *app = project_application_new();
  g_application_run (G_APPLICATION(app), argc, argv);

  return 0;
}
