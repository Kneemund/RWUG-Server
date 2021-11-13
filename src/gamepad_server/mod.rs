use std::net::{SocketAddr, UdpSocket};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use evdev_rs::{enums::*, AbsInfo, DeviceWrapper, InputEvent, TimeVal, UInputDevice, UninitDevice};
use pad_motion::server::*;

mod gamepad_data;
use gamepad_data::{GamepadData, GAMEPAD_BUTTON_DATA};

const PORT: u16 = 4242;
const TIMEOUT_SECONDS: u64 = 5;

const STICK_RADIUS: i32 = 32767;
const NO_TIME: TimeVal = TimeVal { tv_sec: 0, tv_usec: 0 };

fn register_axes(device: &UninitDevice) {
    let stick_info = AbsInfo {
        maximum: STICK_RADIUS,
        minimum: -STICK_RADIUS,
        flat: 0,
        fuzz: 0,
        resolution: 0,
        value: 0,
    };

    device.enable_event_type(&EventType::EV_ABS).unwrap();
    device.enable_event_code(&EventCode::EV_ABS(EV_ABS::ABS_X), Some(&stick_info)).unwrap();
    device.enable_event_code(&EventCode::EV_ABS(EV_ABS::ABS_Y), Some(&stick_info)).unwrap();
    device.enable_event_code(&EventCode::EV_ABS(EV_ABS::ABS_RX), Some(&stick_info)).unwrap();
    device.enable_event_code(&EventCode::EV_ABS(EV_ABS::ABS_RY), Some(&stick_info)).unwrap();
}

fn register_buttons(device: &UninitDevice) {
    device.enable_event_type(&EventType::EV_KEY).unwrap();
    for button in GAMEPAD_BUTTON_DATA.iter() {
        device.enable_event_code(&button.1, None).unwrap();
    }
}

pub struct GamepadServer {
    socket: UdpSocket,
    motion_server: Arc<Server>,
    uinput_device: UInputDevice,
    created_time: Instant,
}

impl GamepadServer {
    pub fn new(motion_server: Arc<Server>, address: SocketAddr, timeout: u64) -> GamepadServer {
        let socket = UdpSocket::bind(address).expect("Failed to bind UDP socket.");
        socket.set_read_timeout(Some(Duration::new(timeout, 0))).unwrap();
        println!("Bound UDP socket at port {} with a timeout of {} seconds.", PORT, TIMEOUT_SECONDS);

        let uninit_device = UninitDevice::new().expect("Failed to create evdev device.");
        println!("Created evdev device.");

        uninit_device.set_name("Wii U Gamepad");
        register_buttons(&uninit_device);
        register_axes(&uninit_device);
        uninit_device.enable_event_type(&EventType::EV_MSC).unwrap();
        uninit_device.enable_event_code(&EventCode::EV_MSC(EV_MSC::MSC_TIMESTAMP), None).unwrap();
        println!("Configured evdev device.");

        let uinput_device = UInputDevice::create_from_device(&uninit_device).expect("Failed to create UInput device.");
        println!("Created UInput device at {}.", uinput_device.devnode().unwrap_or("NONE"));

        return GamepadServer {
            socket,
            created_time: Instant::now(),
            uinput_device,
            motion_server,
        };
    }

    pub fn start(&self, countinue_running: &Arc<AtomicBool>) {
        let mut buffer = [0; 1024];
        while countinue_running.load(Ordering::SeqCst) {
            match self.socket.recv_from(&mut buffer) {
                Ok((number_of_bytes, _)) => {
                    let elapsed_microseconds = self.created_time.elapsed().as_micros() as u64;
                    let data = serde_json::from_slice(&buffer[..number_of_bytes]).expect("Failed to parse incoming UDP datagram.");

                    self.update_button_data(&data);
                    self.update_joystick_data(&data);
                    self.synchronize(elapsed_microseconds as u16);

                    self.motion_server.update_controller_data(0, data.to_controller_data(elapsed_microseconds));
                }
                _ => (),
            }
        }
    }

    fn update_button_data(&self, data: &GamepadData) {
        for button in GAMEPAD_BUTTON_DATA.iter() {
            self.uinput_device
                .write_event(&InputEvent::new(&NO_TIME, &button.1, ((data.wiiUGamePad.hold & button.0) != 0) as i32))
                .unwrap();
        }
    }

    fn update_joystick_data(&self, data: &GamepadData) {
        self.uinput_device
            .write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_ABS(EV_ABS::ABS_X), (data.wiiUGamePad.lStickX * STICK_RADIUS as f32) as i32))
            .unwrap();

        self.uinput_device
            .write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_ABS(EV_ABS::ABS_Y), (data.wiiUGamePad.lStickY * -STICK_RADIUS as f32) as i32))
            .unwrap();

        self.uinput_device
            .write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_ABS(EV_ABS::ABS_RX), (data.wiiUGamePad.rStickX * STICK_RADIUS as f32) as i32))
            .unwrap();

        self.uinput_device
            .write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_ABS(EV_ABS::ABS_RY), (data.wiiUGamePad.rStickY * -STICK_RADIUS as f32) as i32))
            .unwrap();
    }

    fn synchronize(&self, elapsed_microseconds: u16) {
        // This wraps around at i32::MAX to 0 automatically: u128 -> u16 -> i32
        self.uinput_device
            .write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_MSC(EV_MSC::MSC_TIMESTAMP), elapsed_microseconds as i32))
            .unwrap();

        self.uinput_device.write_event(&InputEvent::new(&NO_TIME, &EventCode::EV_SYN(EV_SYN::SYN_REPORT), 0)).unwrap();
    }
}
