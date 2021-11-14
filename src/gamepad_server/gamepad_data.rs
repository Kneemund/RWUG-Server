use evdev_rs::enums::*;
use pad_motion::protocol::*;
use serde::Deserialize;

const BUTTON_A: u32 = 0x8000;
const BUTTON_B: u32 = 0x4000;
const BUTTON_X: u32 = 0x2000;
const BUTTON_Y: u32 = 0x1000;
const BUTTON_LEFT: u32 = 0x0800;
const BUTTON_RIGHT: u32 = 0x0400;
const BUTTON_UP: u32 = 0x0200;
const BUTTON_DOWN: u32 = 0x0100;
const BUTTON_ZL: u32 = 0x0080;
const BUTTON_ZR: u32 = 0x0040;
const BUTTON_L: u32 = 0x0020;
const BUTTON_R: u32 = 0x0010;
const BUTTON_PLUS: u32 = 0x0008;
const BUTTON_MINUS: u32 = 0x0004;
const BUTTON_HOME: u32 = 0x0002;
const BUTTON_STICK_R: u32 = 0x00020000;
const BUTTON_STICK_L: u32 = 0x00040000;

// const BUTTON_SYNC: u32 = 0x0001;
// const BUTTON_TV: u32 = 0x00010000;

pub const GAMEPAD_BUTTON_DATA: &[(u32, &EventCode)] = &[
    (BUTTON_A, &EventCode::EV_KEY(EV_KEY::BTN_EAST)),
    (BUTTON_B, &EventCode::EV_KEY(EV_KEY::BTN_SOUTH)),
    (BUTTON_X, &EventCode::EV_KEY(EV_KEY::BTN_WEST)),
    (BUTTON_Y, &EventCode::EV_KEY(EV_KEY::BTN_NORTH)),
    (BUTTON_LEFT, &EventCode::EV_KEY(EV_KEY::BTN_DPAD_LEFT)),
    (BUTTON_RIGHT, &EventCode::EV_KEY(EV_KEY::BTN_DPAD_RIGHT)),
    (BUTTON_UP, &EventCode::EV_KEY(EV_KEY::BTN_DPAD_UP)),
    (BUTTON_DOWN, &EventCode::EV_KEY(EV_KEY::BTN_DPAD_DOWN)),
    (BUTTON_ZL, &EventCode::EV_KEY(EV_KEY::BTN_TL2)),
    (BUTTON_ZR, &EventCode::EV_KEY(EV_KEY::BTN_TR2)),
    (BUTTON_L, &EventCode::EV_KEY(EV_KEY::BTN_TL)),
    (BUTTON_R, &EventCode::EV_KEY(EV_KEY::BTN_TR)),
    (BUTTON_PLUS, &EventCode::EV_KEY(EV_KEY::BTN_START)),
    (BUTTON_MINUS, &EventCode::EV_KEY(EV_KEY::BTN_SELECT)),
    (BUTTON_HOME, &EventCode::EV_KEY(EV_KEY::BTN_MODE)),
    (BUTTON_STICK_R, &EventCode::EV_KEY(EV_KEY::BTN_THUMBR)),
    (BUTTON_STICK_L, &EventCode::EV_KEY(EV_KEY::BTN_THUMBL)),
];

#[allow(non_snake_case)]
#[derive(Deserialize, Debug)]
pub struct GamepadDataValues {
    accX: f32,
    accY: f32,
    accZ: f32,
    gyroX: f32,
    gyroY: f32,
    gyroZ: f32,
    tpTouch: u8,
    tpX: u16,
    tpY: u16,
    pub lStickX: f32,
    pub lStickY: f32,
    pub rStickX: f32,
    pub rStickY: f32,
    pub hold: u32,
}

#[allow(non_snake_case)]
#[derive(Deserialize, Debug)]
pub struct GamepadData {
    pub wiiUGamePad: GamepadDataValues,
}

impl GamepadData {
    pub fn to_controller_data(&self, elapsed_microseconds: u64) -> ControllerData {
        ControllerData {
            connected: true,

            // u64 is big enough to run the server for 584942 years.
            // Running it longer than that will work too, because unsigned integers wrap and don't overflow.
            motion_data_timestamp: elapsed_microseconds,

            // gyroscope in deg/s
            // The gamepad values seem to be in the range of [-1, 1] instead of [-360, 360].
            // Some axes appear to be flipped.
            gyroscope_pitch: -self.wiiUGamePad.gyroX * 360.0,
            gyroscope_yaw: -self.wiiUGamePad.gyroY * 360.0,
            gyroscope_roll: self.wiiUGamePad.gyroZ * 360.0,

            // accelerometer in g
            // The y axis is always near -1, making the raw gamepad values plausible.
            // Some axes appear to be flipped.
            accelerometer_x: -self.wiiUGamePad.accX,
            accelerometer_y: self.wiiUGamePad.accY,
            accelerometer_z: -self.wiiUGamePad.accZ,

            // left_stick_x: (self.wiiUGamePad.lStickX * 128.0 + 128.0) as u8,
            // left_stick_y: (self.wiiUGamePad.lStickY * 128.0 + 128.0) as u8,
            // right_stick_x: (self.wiiUGamePad.rStickX * 128.0 + 128.0) as u8,
            // right_stick_y: (self.wiiUGamePad.rStickY * 128.0 + 128.0) as u8,

            // The id might have to be incremented after every touch, but it seems to work like this too.
            first_touch: TouchData {
                active: self.wiiUGamePad.tpTouch == 1,
                id: self.wiiUGamePad.tpTouch,
                position_x: self.wiiUGamePad.tpX,
                position_y: self.wiiUGamePad.tpY,
            },

            ..Default::default()
        }
    }
}
