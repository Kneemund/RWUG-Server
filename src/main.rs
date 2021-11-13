use std::net::SocketAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use pad_motion::protocol::*;
use pad_motion::server::*;

use structopt::StructOpt;

mod gamepad_server;

#[derive(StructOpt)]
#[structopt(name = "Wii U Gamepad Server", about = "A UDP server for the Wii U Gamepad, written in Rust for GNU/Linux.")]
struct CommandLineArguments {
    #[structopt(short, long = "motion-server-address", default_value = "127.0.0.1:26760", help = "Address that the cemuhook motion data will be sent to.")]
    motion_server_address: String,

    #[structopt(short, long = "gamepad-server-address", default_value = "0.0.0.0:4242", help = "Address that the gamepad data will be received from.")]
    gamepad_server_address: String,

    #[structopt(short, long, default_value = "5", help = "Time in seconds after which the UDP socket will refresh if no datagrams were received.")]
    timeout: u64,
}

fn main() {
    let arguments = CommandLineArguments::from_args();

    // SIGINT HANDLER
    let running = Arc::new(AtomicBool::new(true));

    {
        let running = running.clone();
        ctrlc::set_handler(move || {
            running.store(false, Ordering::SeqCst);
            println!("\nExiting gracefully...");
        })
        .expect("Failed to set SIGINT handler.");
    }

    // MOTION SERVER
    let motion_server = Arc::new(Server::new(None, arguments.motion_server_address.parse().ok()).unwrap());
    let motion_server_thread_join_handle = {
        let motion_server = motion_server.clone();
        motion_server.start(running.clone())
    };
    println!("Created motion server and moved it to another thread.");

    let controller_info = ControllerInfo {
        slot_state: SlotState::Connected,
        device_type: DeviceType::FullGyro,
        connection_type: ConnectionType::Bluetooth,
        ..Default::default()
    };

    motion_server.update_controller_info(controller_info);
    println!("Set cemuhook controller information.");

    // GAMEPAD SERVER
    let gamepad_server = gamepad_server::GamepadServer::new(motion_server, arguments.gamepad_server_address.parse::<SocketAddr>().unwrap(), arguments.timeout);
    println!("Created gamepad server.");
    gamepad_server.start(&running);

    // CLOSE THREAD
    motion_server_thread_join_handle.join().unwrap();
}
