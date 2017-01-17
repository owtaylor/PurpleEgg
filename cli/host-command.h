
#include <gio/gio.h>

#ifndef HOST_COMMAND_H
#define HOST_COMMAND_H
typedef void (*PeggHostCommandCallback) (int      pid,
                                         int      status,
                                         gpointer user_data);

gboolean pegg_in_flatpak (void);

int pegg_call_host_command (GDBusConnection          *connection,
                            char                    **args,
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

