#include "dbus_portal.h"

#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>



void subscribe_to_signal(dbus_data_struct *dbus_data, GDBusSignalCallback callback) {
    g_dbus_connection_signal_subscribe(dbus_data->connection, NULL, "org.freedesktop.portal.Request", "Response", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, callback, dbus_data, NULL);
}
int runmaincontext = 1;


// Quick explanation : we run the glib mainloop in main(), while functions are calling each other asynchronously.
// It goes like this : 
// This is required, because else, it isn't possible to get a callback, ever.
// Once we get a callback for our last dbus method call (OpenPipeWireRemote()), we can release the mainloop

////////////////////////////////////////////////////////////////////////////

void do_pipewire_stuff(int fd) {

}

void open_pipewire_remote_cb(GObject *source, GAsyncResult *res, void *raw_data) {
    printf("Got reply for OpenPipeWireRemote()\n");

    runmaincontext = 0;

    GVariant *result = NULL;
    g_autoptr(GUnixFDList) fd_list = NULL;
    int fd_index;
    
    result = g_dbus_proxy_call_with_unix_fd_list_finish(G_DBUS_PROXY(source), &fd_list, res, NULL);

    g_variant_get(result, "(h)", &fd_index, NULL);
    
    do_pipewire_stuff(g_unix_fd_list_get(fd_list, fd_index, NULL));
    return;
}

void open_pipewire_remote(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *raw_data) {
    printf("Got a reply for StartSession()\n");
    dbus_data_struct *dbus_data = raw_data;
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_dbus_proxy_call_with_unix_fd_list(dbus_data->proxy, "OpenPipeWireRemote", g_variant_new("(oa{sv})", dbus_data->session_path, &builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, open_pipewire_remote_cb, dbus_data);

    return;
}

////////////////////////////////////////////////////////////////////////////

void start_session(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *raw_data) {
    printf("Got a reply for SelectSources()\n");
    dbus_data_struct *dbus_data = raw_data;
    GVariantBuilder builder;
    
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string("RWUG1"));

    subscribe_to_signal(dbus_data, &open_pipewire_remote);

    g_dbus_proxy_call(dbus_data->proxy, "Start", g_variant_new("(osa{sv})", dbus_data->session_path, "RWUG_SERVER", &builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

    return;
}

////////////////////////////////////////////////////////////////////////////

void select_sources(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *raw_data) {
    printf("Got a reply for CreateSession()\n");
    dbus_data_struct *dbus_data = raw_data;
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string("RWUG1"));
    g_variant_builder_add(&builder, "{sv}", "multiple", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&builder, "{sv}", "cursor_mode", g_variant_new_uint32(1));
    g_variant_builder_add(&builder, "{sv}", "types", g_variant_new_uint32(2));

    subscribe_to_signal(dbus_data, &start_session);

    g_dbus_proxy_call(dbus_data->proxy, "SelectSources", g_variant_new("(oa{sv})", dbus_data->session_path, &builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

    return;
}

////////////////////////////////////////////////////////////////////////////

void create_session_cb(GObject *source, GAsyncResult *result, void *raw_data) {
    printf("Got callback for CreateSession()\n");
    dbus_data_struct *dbus_data = raw_data;
    

    char *request_path;

    GVariant *variant = g_dbus_proxy_call_finish(dbus_data->proxy, result, NULL);
    g_variant_get(variant, "(o)", &request_path);

    strcpy(dbus_data->session_path, "/org/freedesktop/portal/desktop/session/");
    strcat(dbus_data->session_path, &request_path[40]); // Gets us the session path, by re-using the "SENDER/TOKEN" field from the request path (according to the flatpak portal API)

    subscribe_to_signal(dbus_data, &select_sources);

    printf("Session path : %s\n", dbus_data->session_path);
    return;
}

void create_session(dbus_data_struct *dbus_data) {
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string("RWUG1"));
    g_variant_builder_add(&builder, "{sv}", "session_handle_token", g_variant_new_string("RWUG1"));

    g_dbus_proxy_call(dbus_data->proxy, "CreateSession", g_variant_new("(a{sv})", &builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, create_session_cb, dbus_data);

    return;
}
