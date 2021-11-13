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
// const STICK_R_EMULATION_LEFT: u32 = 0x04000000;
// const STICK_R_EMULATION_RIGHT: u32 = 0x02000000;
// const STICK_R_EMULATION_UP: u32 = 0x01000000;
// const STICK_R_EMULATION_DOWN: u32 = 0x00800000;
// const STICK_L_EMULATION_LEFT: u32 = 0x40000000;
// const STICK_L_EMULATION_RIGHT: u32 = 0x20000000;
// const STICK_L_EMULATION_UP: u32 = 0x10000000;
// const STICK_L_EMULATION_DOWN: u32 = 0x08000000;

pub struct GamepadButton(pub u32, pub EventCode);

pub const GAMEPAD_BUTTON_DATA: [GamepadButton; 17] = [
    GamepadButton(BUTTON_A, EventCode::EV_KEY(EV_KEY::BTN_EAST)),
    GamepadButton(BUTTON_B, EventCode::EV_KEY(EV_KEY::BTN_SOUTH)),
    GamepadButton(BUTTON_X, EventCode::EV_KEY(EV_KEY::BTN_WEST)),
    GamepadButton(BUTTON_Y, EventCode::EV_KEY(EV_KEY::BTN_NORTH)),
    GamepadButton(BUTTON_LEFT, EventCode::EV_KEY(EV_KEY::BTN_DPAD_LEFT)),
    GamepadButton(BUTTON_RIGHT, EventCode::EV_KEY(EV_KEY::BTN_DPAD_RIGHT)),
    GamepadButton(BUTTON_UP, EventCode::EV_KEY(EV_KEY::BTN_DPAD_UP)),
    GamepadButton(BUTTON_DOWN, EventCode::EV_KEY(EV_KEY::BTN_DPAD_DOWN)),
    GamepadButton(BUTTON_ZL, EventCode::EV_KEY(EV_KEY::BTN_TL2)),
    GamepadButton(BUTTON_ZR, EventCode::EV_KEY(EV_KEY::BTN_TR2)),
    GamepadButton(BUTTON_L, EventCode::EV_KEY(EV_KEY::BTN_TL)),
    GamepadButton(BUTTON_R, EventCode::EV_KEY(EV_KEY::BTN_TR)),
    GamepadButton(BUTTON_PLUS, EventCode::EV_KEY(EV_KEY::BTN_START)),
    GamepadButton(BUTTON_MINUS, EventCode::EV_KEY(EV_KEY::BTN_SELECT)),
    GamepadButton(BUTTON_HOME, EventCode::EV_KEY(EV_KEY::BTN_MODE)),
    GamepadButton(BUTTON_STICK_R, EventCode::EV_KEY(EV_KEY::BTN_THUMBR)),
    GamepadButton(BUTTON_STICK_L, EventCode::EV_KEY(EV_KEY::BTN_THUMBL)),
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
