#define _DEFAULT_SOURCE

#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include "udp_socket.h"
#include "gamepad_button.h"

#define PORT 4242

#define RWUG_IN_SIZE 58
#define RWUG_OUT_SIZE 4

#define RWUG_PLAY 0x01
#define RWUG_STOP 0x02

#define DATA_UPDATE_RATE 10000

#define STICK_RADIUS 32767

#define GYROSCOPE_RADIUS 8388607  // The gamepad supposedly sends 3 bytes per gyroscope axis. This is 2^23-1.
#define GYROSCOPE_RESOLUTION 1000 // Random value.

#define ACCELEROMETER_RADIUS 32767    // 2 bytes per accelerometer axis.
#define ACCELEROMETER_RESOLUTION 8192 // The data we get seems to be in G. I am only going with such high resolution for precision.

void set_abs_info(int fd, uint32_t code, const struct input_absinfo* abs_info) {
    struct uinput_abs_setup abs_setup;
	abs_setup.code = code;
	abs_setup.absinfo = *abs_info;

	ioctl(fd, UI_ABS_SETUP, &abs_setup);
}

void register_axes(int fd, struct uinput_user_dev* device) {
    device->absmax[ABS_X]  =  STICK_RADIUS;
    device->absmin[ABS_X]  = -STICK_RADIUS;
    device->absmax[ABS_Y]  =  STICK_RADIUS;
    device->absmin[ABS_Y]  = -STICK_RADIUS;
    device->absmax[ABS_RX] =  STICK_RADIUS;
    device->absmin[ABS_RX] = -STICK_RADIUS;
    device->absmax[ABS_RY] =  STICK_RADIUS;
    device->absmin[ABS_RY] = -STICK_RADIUS;

    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);

    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_TIMESTAMP);
}

void register_buttons(int fd) {
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int i = 0; i < GAMEPAD_BUTTON_DATA_LENGTH; ++i) {
        ioctl(fd, UI_SET_KEYBIT, GAMEPAD_BUTTON_DATA[i].evdev_button_code);
    }
}

void register_force_feedback(int fd) {
    ioctl(fd, UI_SET_EVBIT, EV_FF);
    ioctl(fd, UI_SET_FFBIT, FF_PERIODIC);
    ioctl(fd, UI_SET_FFBIT, FF_RUMBLE);
    ioctl(fd, UI_SET_FFBIT, FF_GAIN);
    ioctl(fd, UI_SET_FFBIT, FF_SQUARE);
    ioctl(fd, UI_SET_FFBIT, FF_TRIANGLE);
    ioctl(fd, UI_SET_FFBIT, FF_SINE);
}

void register_motion(int fd) {
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_ACCELEROMETER);

    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_TIMESTAMP);

    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_Z);
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);
    ioctl(fd, UI_SET_ABSBIT, ABS_RZ);

    struct input_absinfo acc_info;
    acc_info.maximum =  ACCELEROMETER_RADIUS;
    acc_info.minimum = -ACCELEROMETER_RADIUS;
    acc_info.flat = 0;
    acc_info.fuzz = 0;
    acc_info.resolution = ACCELEROMETER_RESOLUTION;

    set_abs_info(fd, ABS_X, &acc_info);
    set_abs_info(fd, ABS_Y, &acc_info);
    set_abs_info(fd, ABS_Z, &acc_info);

    struct input_absinfo gyro_info;
    gyro_info.maximum =  GYROSCOPE_RADIUS;
    gyro_info.minimum = -GYROSCOPE_RADIUS;
    gyro_info.flat = 0;
    gyro_info.fuzz = 0;
    gyro_info.resolution = GYROSCOPE_RESOLUTION;

    set_abs_info(fd, ABS_RX, &gyro_info);
    set_abs_info(fd, ABS_RY, &gyro_info);
    set_abs_info(fd, ABS_RZ, &gyro_info);
}

ssize_t write_event(int* fd, uint16_t type, uint16_t code, int32_t value) {
    struct input_event event;
    event.type = type;
    event.code = code;
    event.value = value;
    event.time.tv_sec = 0;
    event.time.tv_usec = 0;

    return write(*fd, &event, sizeof(struct input_event));
}

int main() {
    // VIRTUAL CONTROLLER
    int controller_fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);
    if (controller_fd < 0) {
        perror("Failed to open uinput file descriptor for the virtual controller.");
        return -1;
    }

    struct uinput_user_dev controller_device;
    memset(&controller_device, 0, sizeof(controller_device));
    snprintf(controller_device.name, UINPUT_MAX_NAME_SIZE, "Wii U GamePad");
    controller_device.id.bustype = BUS_VIRTUAL;
    controller_device.ff_effects_max = 16; // Technically only 1, but that might make it incompatible with some games.

    register_buttons(controller_fd);
    register_force_feedback(controller_fd);
    register_axes(controller_fd, &controller_device);

    if (write(controller_fd, &controller_device, sizeof(controller_device)) < 0) {
        perror("Failed to write device data of virtual controller.");
        return -1;
    }

    if (ioctl(controller_fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create virtual controller.");
        return -1;
    }



    // MOTION DEVICE
    int motion_fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);
    if (motion_fd < 0) {
        perror("Failed to open uinput file descriptor for the motion device.");
        return -1;
    }

    struct uinput_user_dev motion_device;
    memset(&motion_device, 0, sizeof(motion_device));
    snprintf(motion_device.name, UINPUT_MAX_NAME_SIZE, "Wii U GamePad IMU");
    motion_device.id.bustype = BUS_VIRTUAL;

    register_motion(motion_fd);

    if (write(motion_fd, &motion_device, sizeof(motion_device)) < 0) {
        perror("Failed to write device data of motion device.");
        return -1;
    }

    if (ioctl(motion_fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create motion device.");
        return -1;
    }



    int udp_socket = init_udp_socket(PORT);
    if (udp_socket < 0) return -1;

    struct ff_effect current_ff_effect;
    memset(&current_ff_effect, 0, sizeof(current_ff_effect));

    struct sockaddr_in gamepad_addr;
    memset(&gamepad_addr, 0, sizeof(gamepad_addr));
    socklen_t gamepad_addr_size = sizeof(gamepad_addr);

    while (1) {
        uint8_t incoming_packet[RWUG_IN_SIZE];
        if (recvfrom(udp_socket, incoming_packet, RWUG_IN_SIZE, MSG_DONTWAIT, (struct sockaddr*) &gamepad_addr, &gamepad_addr_size) > 0) {
            // VIRTUAL CONTROLLER
            uint32_t hold;
            memcpy(&hold, &incoming_packet[38], sizeof(uint32_t));

            for (int i = 0; i < GAMEPAD_BUTTON_DATA_LENGTH; ++i) {
                struct gamepad_button button = GAMEPAD_BUTTON_DATA[i];
                write_event(&controller_fd, EV_KEY, button.evdev_button_code, (hold & button.button_code) != 0);
            }

            float stickLX, stickLY, stickRX, stickRY;
            memcpy(&stickLX, &incoming_packet[42], sizeof(float));
            memcpy(&stickLY, &incoming_packet[46], sizeof(float));
            memcpy(&stickRX, &incoming_packet[50], sizeof(float));
            memcpy(&stickRY, &incoming_packet[54], sizeof(float));

            write_event(&controller_fd, EV_ABS, ABS_X,  stickLX *  STICK_RADIUS);
            write_event(&controller_fd, EV_ABS, ABS_Y,  stickLY * -STICK_RADIUS);
            write_event(&controller_fd, EV_ABS, ABS_RX, stickRX *  STICK_RADIUS);
            write_event(&controller_fd, EV_ABS, ABS_RY, stickRY * -STICK_RADIUS);

            uint64_t timestamp;
            memcpy(&timestamp, &incoming_packet[30], sizeof(uint64_t));

            write_event(&controller_fd, EV_MSC, MSC_TIMESTAMP, timestamp);
            write_event(&controller_fd, EV_SYN, SYN_REPORT, 0);



            // MOTION DEVICE
            float accX, accY, accZ;
            memcpy(&accX, &incoming_packet[0], sizeof(float));
            memcpy(&accY, &incoming_packet[4], sizeof(float));
            memcpy(&accZ, &incoming_packet[8], sizeof(float));

            write_event(&motion_fd, EV_ABS, ABS_X, accX * ACCELEROMETER_RESOLUTION);
            write_event(&motion_fd, EV_ABS, ABS_Y, accY * ACCELEROMETER_RESOLUTION);
            write_event(&motion_fd, EV_ABS, ABS_Z, accZ * ACCELEROMETER_RESOLUTION);

            float gyroX, gyroY, gyroZ;
            memcpy(&gyroX, &incoming_packet[12], sizeof(float));
            memcpy(&gyroY, &incoming_packet[16], sizeof(float));
            memcpy(&gyroZ, &incoming_packet[20], sizeof(float));

            write_event(&motion_fd, EV_ABS, ABS_RX, gyroX * GYROSCOPE_RESOLUTION);
            write_event(&motion_fd, EV_ABS, ABS_RY, gyroY * GYROSCOPE_RESOLUTION);
            write_event(&motion_fd, EV_ABS, ABS_RZ, gyroZ * GYROSCOPE_RESOLUTION);

            write_event(&motion_fd, EV_MSC, MSC_TIMESTAMP, timestamp);
            write_event(&motion_fd, EV_SYN, SYN_REPORT, 0);
        }

        struct input_event read_event;
        while (read(controller_fd, &read_event, sizeof(read_event)) == sizeof(read_event)) {
            switch (read_event.type) {
                case EV_FF: {
                    switch (read_event.code) {
                        case FF_GAIN: {
                            printf("unhandled: set gain\n");
                            break;
                        }

                        default: {
                            uint8_t packet[RWUG_OUT_SIZE];

                            if (read_event.value && current_ff_effect.replay.length > 0) {
                                // PLAY
                                packet[0] = RWUG_PLAY;
                                packet[1] = (uint8_t) (current_ff_effect.u.rumble.strong_magnitude * (255.0 / 65536.0)); // scale uint16_t to uint8_t
                                memcpy(&packet[2], &current_ff_effect.replay.length, sizeof(uint16_t));
                            } else {
                                // STOP
                                packet[0] = RWUG_STOP;
                                packet[1] = 0;
                                packet[2] = 0;
                                packet[3] = 0;
                            }

                            sendto(udp_socket, packet, RWUG_OUT_SIZE, 0, (struct sockaddr*) &gamepad_addr, gamepad_addr_size);
                            break;
                        }
                    }
                    break;
                }

                case EV_UINPUT: {
                    switch (read_event.code) {
                        case UI_FF_UPLOAD: {
                            struct uinput_ff_upload upload;
                            memset(&upload, 0, sizeof(upload));

                            upload.request_id = read_event.value;
                            ioctl(controller_fd, UI_BEGIN_FF_UPLOAD, &upload);

                            current_ff_effect = upload.effect;

                            upload.retval = 0;
                            ioctl(controller_fd, UI_END_FF_UPLOAD, &upload);
                            break;
                        }

                        case UI_FF_ERASE: {
                            struct uinput_ff_erase erase;
                            memset(&erase, 0, sizeof(erase));

                            erase.request_id = read_event.value;
                            ioctl(controller_fd, UI_BEGIN_FF_ERASE, &erase);

                            memset(&current_ff_effect, 0, sizeof(current_ff_effect));

                            erase.retval = 0;
                            ioctl(controller_fd, UI_END_FF_ERASE, &erase);
                            break;
                        }

                        default: {
                            printf("unhandled: unknown event code\n");
                            break;
                        }
                    }
                    break;
                }
            }
        }

        usleep(DATA_UPDATE_RATE);
    }

    destroy_udp_socket(&udp_socket);

    ioctl(controller_fd, UI_DEV_DESTROY);
    close(controller_fd);

    ioctl(motion_fd, UI_DEV_DESTROY);
    close(motion_fd);

    return 0;
}
