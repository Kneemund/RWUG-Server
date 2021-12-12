#include <stdio.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

extern int runmaincontext;

struct dbus_data {
    GDBusConnection *connection;
    GDBusProxy *proxy;
    char *session_path;
};
typedef struct dbus_data dbus_data_struct;


void create_session(dbus_data_struct *dbus_data);
