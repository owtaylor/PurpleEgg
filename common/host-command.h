
#include <gio/gio.h>

#ifndef HOST_COMMAND_H
#define HOST_COMMAND_H
typedef void (*PeggHostCommandCallback) (int      pid,
                                         int      status,
                                         gpointer user_data);

typedef enum {
  PEGG_HOST_COMMAND_NONE,
  PEGG_HOST_COMMAND_USE_PTY,
  PEGG_HOST_COMMAND_STDOUT_TO_DEV_NULL
} PeggHostCommandFlags;

gboolean pegg_in_flatpak (void);

void pegg_make_stdin_raw (void);
void pegg_restore_stdin  (void);

int pegg_call_host_command (GDBusConnection          *connection,
                            char                    **args,
                            PeggHostCommandFlags      flags,
                            PeggHostCommandCallback   callback,
                            gpointer                  user_data,
                            GCancellable             *cancellable,
                            GError                  **error);
gboolean pegg_send_host_command_signal (GDBusConnection *connection,
                                        int              pid,
                                        int              signum,
                                        gboolean         to_process_group,
                                        GCancellable    *cancellable,
                                        GError         **error);

#endif /* HOST_COMMAND_H */

