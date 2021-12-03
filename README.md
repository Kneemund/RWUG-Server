## RWUG Server

A UDP server for the Wii U GamePad, which emulates a controller and is written in C for GNU/Linux. \
Running the [client](https://github.com/Kneemund/RWUG-Client) requires a jailbroken Wii U with Homebrew.

The server uses the Wii U GamePad's accelerometer and gyroscope data for emulating a second evdev device, which is called "Wii U GamePad IMU". \
This can be combined with [evdevhook](https://github.com/v1993/evdevhook) to host a DSU/Cemuhook server, as most emulators rely on this to get motion data. \
The profile should look like this:

```json
{
    "profiles": {
        "RWUG": {
            "gyroSensitivity": 1.0,
            "accel": "x+y+z+",
            "gyro": "x+y+z+"
        }
    },
    "devices": [
        {
            "name": "Wii U GamePad IMU",
            "profile": "RWUG"
        }
    ]
}
```
