#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <json-c/json.h>

#include <gio/gio.h>

#include "udp_socket.h"
#include "gamepad_button.h"
#include "dbus_portal.h"

#define PORT 4242
#define MAXLINE 1024

#define STICK_RADIUS 32767

#define GYROSCOPE_RADIUS 8388607  // The gamepad supposedly sends 3 bytes per gyroscope axis. This is 2^23-1.
#define GYROSCOPE_RESOLUTION 1000 // Random value.

#define ACCELEROMETER_RADIUS 32767    // 2 bytes per accelerometer axis.
#define ACCELEROMETER_RESOLUTION 8192 // The data we get seems to be in G. I am only going with such high resolution for precision.

void register_axes(struct libevdev *device) {
    struct input_absinfo stick_info;
    stick_info.maximum =  STICK_RADIUS;
    stick_info.minimum = -STICK_RADIUS;
    stick_info.flat = 0;
    stick_info.fuzz = 0;
    stick_info.resolution = 0;

    libevdev_enable_event_type(device, EV_ABS);
    libevdev_enable_event_code(device, EV_ABS, ABS_X,  &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_Y,  &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RX, &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RY, &stick_info);
}

void register_buttons(struct libevdev *device) {
    libevdev_enable_event_type(device, EV_KEY);
    for (int i = 0; i < GAMEPAD_BUTTON_DATA_LENGTH; ++i) {
        libevdev_enable_event_code(device, EV_KEY, GAMEPAD_BUTTON_DATA[i].evdev_button_code, NULL);
    }
}

void register_gyro(struct libevdev *device) {
    struct input_absinfo acc_info, gyro_info;
    gyro_info.maximum =  GYROSCOPE_RADIUS;
    gyro_info.minimum = -GYROSCOPE_RADIUS;
    gyro_info.flat = 0;
    gyro_info.fuzz = 0;
    gyro_info.resolution = GYROSCOPE_RESOLUTION;

    acc_info.maximum =  ACCELEROMETER_RADIUS;
    acc_info.minimum = -ACCELEROMETER_RADIUS;
    acc_info.flat = 0;
    acc_info.fuzz = 0;
    acc_info.resolution = ACCELEROMETER_RESOLUTION;

    libevdev_enable_property(device, INPUT_PROP_ACCELEROMETER);
    libevdev_enable_event_type(device, EV_MSC);
    libevdev_enable_event_code(device, EV_MSC, MSC_TIMESTAMP, NULL);

    libevdev_enable_event_type(device, EV_ABS);
    libevdev_enable_event_code(device, EV_ABS, ABS_X,  &acc_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_Y,  &acc_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_Z,  &acc_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RX, &gyro_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RY, &gyro_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RZ, &gyro_info);
}



int main() {
    struct libevdev *device, *device_imu;
    struct libevdev_uinput *uinput_device, *uinput_device_imu;

    struct timeval current_time;
    int elapsed_time = 0;

    char buffer[MAXLINE];

    struct json_object *gamepad_data, *trigger_data, *release_data;
    struct json_object *l_stick_x, *l_stick_y, *r_stick_x, *r_stick_y;
    struct json_object *acc_x, *acc_y, *acc_z, *gyro_x, *gyro_y, *gyro_z;

    GError *error = NULL;;
    dbus_data_struct dbus_data = {NULL, NULL, malloc(64 * sizeof(char))};

    dbus_data.connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_no_error(error);

    dbus_data.proxy = g_dbus_proxy_new_sync(dbus_data.connection, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.ScreenCast", NULL, &error);
    g_assert_no_error(error);
    
    create_session(&dbus_data);

    while(runmaincontext == 1) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }
    printf("done\n");


    // setup input device
    device = libevdev_new();
    if (!device) {
        perror("Failed to register new evdev input device.");
        return EXIT_FAILURE;
    }

    libevdev_set_name(device, "Wii U GamePad");
    register_buttons(device);
    register_axes(device);

    printf("Registered and configured evdev input device.\n");


    if (libevdev_uinput_create_from_device(device, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput_device) < 0) {
        perror("Failed to create uinput input device.");
        return EXIT_FAILURE;
    }

    printf("Registered uinput input device at %s.\n", libevdev_uinput_get_devnode(uinput_device));

    // setup motion device
    device_imu = libevdev_new();
    if (!device_imu) {
        perror("Failed to register new evdev motion device.");
        return EXIT_FAILURE;
    }

    libevdev_set_name(device_imu, "Wii U GamePad IMU");
    register_gyro(device_imu);

    printf("Registered and configured evdev motion device.\n");


    if (libevdev_uinput_create_from_device(device_imu, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput_device_imu) < 0) {
        perror("Failed to create uinput motion device.");
        return EXIT_FAILURE;
    }

    printf("Registered uinput motion device at %s.\n", libevdev_uinput_get_devnode(uinput_device_imu));


    int udp_socket = init_udp_socket(PORT);
    if (udp_socket < 0) return EXIT_FAILURE;


    while (1) {
        buffer[recv(udp_socket, (char*) buffer, MAXLINE, MSG_WAITALL)] = '\0';
        gamepad_data = json_tokener_parse(buffer);

        // buttons
        json_object_object_get_ex(gamepad_data, "trigger", &trigger_data);
        int32_t trigger = json_object_get_int(trigger_data);

        json_object_object_get_ex(gamepad_data, "release", &release_data);
        int32_t release = json_object_get_int(release_data);

        for (int i = 0; i < GAMEPAD_BUTTON_DATA_LENGTH; ++i) {
            struct gamepad_button button = GAMEPAD_BUTTON_DATA[i];

            if (trigger & button.button_code) {
                libevdev_uinput_write_event(uinput_device, EV_KEY, button.evdev_button_code, 1);
            } else if (release & button.button_code) {
                libevdev_uinput_write_event(uinput_device, EV_KEY, button.evdev_button_code, 0);
            }
        }


        json_object_object_get_ex(gamepad_data, "l_stick_x", &l_stick_x);
        json_object_object_get_ex(gamepad_data, "l_stick_y", &l_stick_y);
        json_object_object_get_ex(gamepad_data, "r_stick_x", &r_stick_x);
        json_object_object_get_ex(gamepad_data, "r_stick_y", &r_stick_y);

        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_X, json_object_get_double(l_stick_x) * STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_Y, json_object_get_double(l_stick_y) * -STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_RX, json_object_get_double(r_stick_x) * STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_RY, json_object_get_double(r_stick_y) * -STICK_RADIUS);


        libevdev_uinput_write_event(uinput_device, EV_SYN, SYN_REPORT, 0);

        // gyro
        json_object_object_get_ex(gamepad_data, "acc_x", &acc_x);
        json_object_object_get_ex(gamepad_data, "acc_y", &acc_y);
        json_object_object_get_ex(gamepad_data, "acc_z", &acc_z);

        json_object_object_get_ex(gamepad_data, "gyro_x", &gyro_x);
        json_object_object_get_ex(gamepad_data, "gyro_y", &gyro_y);
        json_object_object_get_ex(gamepad_data, "gyro_z", &gyro_z);

        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_X, json_object_get_double(acc_x) * -8192);
        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_Y, json_object_get_double(acc_y) *  8192);
        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_Z, json_object_get_double(acc_z) * -8192);
        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_RX, json_object_get_double(gyro_x) * -342500); // pitch
        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_RY, json_object_get_double(gyro_y) * -342500); // yaw     342.5 * GYRO_RES
        libevdev_uinput_write_event(uinput_device_imu, EV_ABS, ABS_RZ, json_object_get_double(gyro_z) *  342500); // roll    This number is random

        // get time as micro seconds
        gettimeofday(&current_time, NULL);
        elapsed_time = (current_time.tv_sec * 1000000 + current_time.tv_usec);
        libevdev_uinput_write_event(uinput_device_imu, EV_MSC, MSC_TIMESTAMP, elapsed_time);

        // free memory
        while (json_object_put(gamepad_data) != 1);
    }

    destroy_udp_socket(&udp_socket);
    libevdev_uinput_destroy(uinput_device);
    libevdev_free(device);
    return EXIT_SUCCESS;
}
