#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <json-c/json.h>

#include "gamepad_button_code.h"

#define PORT 4242
#define MAXLINE 1024
#define STICK_RADIUS 32767

struct gamepad_button {
    int button_code;
    int evdev_button_code;
};

const struct gamepad_button gamepad_button_data[] = {
    { GAMEPAD_BUTTON_A, BTN_EAST },
    { GAMEPAD_BUTTON_B, BTN_SOUTH },
    { GAMEPAD_BUTTON_X, BTN_WEST },
    { GAMEPAD_BUTTON_Y, BTN_NORTH },
    { GAMEPAD_BUTTON_LEFT, BTN_DPAD_LEFT },
    { GAMEPAD_BUTTON_RIGHT, BTN_DPAD_RIGHT },
    { GAMEPAD_BUTTON_UP, BTN_DPAD_UP },
    { GAMEPAD_BUTTON_DOWN, BTN_DPAD_DOWN },
    { GAMEPAD_BUTTON_ZL, BTN_TL2 },
    { GAMEPAD_BUTTON_ZR, BTN_TR2 },
    { GAMEPAD_BUTTON_L, BTN_TL },
    { GAMEPAD_BUTTON_R, BTN_TR },
    { GAMEPAD_BUTTON_PLUS, BTN_START },
    { GAMEPAD_BUTTON_MINUS, BTN_SELECT },
    { GAMEPAD_BUTTON_HOME, BTN_MODE },
    { GAMEPAD_BUTTON_STICK_R, BTN_THUMBR },
    { GAMEPAD_BUTTON_STICK_L, BTN_THUMBL },
};

const int gamepad_button_data_length = sizeof(gamepad_button_data) / sizeof(gamepad_button_data[0]);

void register_axis(struct libevdev *device) {
    struct input_absinfo stick_info;
    stick_info.maximum = STICK_RADIUS;
    stick_info.minimum = -STICK_RADIUS;
    stick_info.flat = 0;
    stick_info.fuzz = 0;
    stick_info.resolution = 0;

    libevdev_enable_event_type(device, EV_ABS);
    libevdev_enable_event_code(device, EV_ABS, ABS_X, &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_Y, &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RX, &stick_info);
    libevdev_enable_event_code(device, EV_ABS, ABS_RY, &stick_info);
}

void register_buttons(struct libevdev *device) {
    libevdev_enable_event_type(device, EV_KEY);
    for (int i = 0; i < gamepad_button_data_length; ++i) {
        libevdev_enable_event_code(device, EV_KEY, gamepad_button_data[i].evdev_button_code, NULL);
    }
}

int setup_udp_socket(int* socket_fd) {
    if ((*socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket.");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(*socket_fd, (const struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("Failed to bind socket.");
        return EXIT_FAILURE;
    }

    printf("UDP server listening on port %d.\n", PORT);
    return EXIT_SUCCESS;
}

int main() {
    struct libevdev *device;
    struct libevdev_uinput *uinput_device;

    int socket_fd;
    char buffer[MAXLINE];

    struct json_object *parsed_json, *gamepad_data, *hold_data;
    struct json_object *l_stick_x, *l_stick_y, *r_stick_x, *r_stick_y;


    device = libevdev_new();
    if (!device) {
        perror("Failed to register new evdev device.");
        return EXIT_FAILURE;
    }

    libevdev_set_name(device, "Wii U Gamepad");
    register_buttons(device);
    register_axis(device);

    printf("Registered and configured evdev device.\n");


    if (libevdev_uinput_create_from_device(device, LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput_device) < 0) {
        perror("Failed to create uinput device.");
        return EXIT_FAILURE;
    }

    printf("Registered uinput device at %s.\n", libevdev_uinput_get_devnode(uinput_device));


    if (setup_udp_socket(&socket_fd) < 0) return EXIT_FAILURE;


    while (1) {
        buffer[recvfrom(socket_fd, (char*) buffer, MAXLINE, MSG_WAITALL, NULL, NULL)] = '\0';
        parsed_json = json_tokener_parse(buffer);
        json_object_object_get_ex(parsed_json, "wiiUGamePad", &gamepad_data);


        json_object_object_get_ex(gamepad_data, "hold", &hold_data);
        int32_t hold = json_object_get_int(hold_data);

        for (int i = 0; i < gamepad_button_data_length; ++i) {
            struct gamepad_button button = gamepad_button_data[i];
            libevdev_uinput_write_event(uinput_device, EV_KEY, button.evdev_button_code, (hold & button.button_code) != 0);
        }


        json_object_object_get_ex(gamepad_data, "lStickX", &l_stick_x);
        json_object_object_get_ex(gamepad_data, "lStickY", &l_stick_y);
        json_object_object_get_ex(gamepad_data, "rStickX", &r_stick_x);
        json_object_object_get_ex(gamepad_data, "rStickY", &r_stick_y);

        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_X, json_object_get_double(l_stick_x) * STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_Y, json_object_get_double(l_stick_y) * -STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_RX, json_object_get_double(r_stick_x) * STICK_RADIUS);
        libevdev_uinput_write_event(uinput_device, EV_ABS, ABS_RY, json_object_get_double(r_stick_y) * -STICK_RADIUS);


        libevdev_uinput_write_event(uinput_device, EV_SYN, SYN_REPORT, 0);


        // free memory
        while (json_object_put(parsed_json) != 1);
    }


    libevdev_uinput_destroy(uinput_device);
    libevdev_free(device);
    return EXIT_SUCCESS;
}
