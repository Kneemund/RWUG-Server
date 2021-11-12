## WiiUGamepadServer
A UDP server for the Wii U Gamepad, which emulates a controller and is written in C for GNU/Linux. \
Running the [client](https://github.com/Crayon2000/MiisendU-Wii-U) requires a jailbroken Wii U with Homebrew.

This branch also has support for the wii u gamepad's accelerometer and gyroscope, accessible via the "Wii U Gamepad IMU" edev device. \
You might also want to use [evdevhook](https://github.com/v1993/evdevhook) to host a DSU/Cemuhook server, as most emulators rely on this to get motion data. \
The profile used should look like this : 
```
{
        "profiles": {
                "wiiUGamepadServer": {
                        "gyroSensitivity": 1.0,
                        "accel": "x+y+z+",
                        "gyro": "x+y-z+"
                }
        },
        "devices": [
                {
                        "name": "Wii U Gamepad IMU",
                        "profile": "wiiUGamepadServer"
                }
        ]
}
```
