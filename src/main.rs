use std::net::SocketAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread::JoinHandle;

use pad_motion::protocol::*;
use pad_motion::server::*;

use structopt::StructOpt;

mod gamepad_server;

#[derive(StructOpt)]
#[structopt(name = "Wii U Gamepad Server", about = "A UDP server for the Wii U Gamepad, written in Rust for GNU/Linux.")]
struct CommandLineArguments {
    #[structopt(short, long, help = "Enable sending motion and touch data via DSU.")]
    dsu_server: bool,

    #[structopt(short = "D", long, default_value = "127.0.0.1:26760", help = "Address of the DSU server that the motion and touch data will be sent to.")]
    dsu_server_address: String,

    #[structopt(short = "G", long, default_value = "0.0.0.0:4242", help = "Address that the gamepad data will be received from.")]
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

    // DSU SERVER
    let dsu_server: Option<Arc<Server>>;
    let dsu_server_handle: Option<JoinHandle<()>>;

    if arguments.dsu_server {
        let server = Arc::new(Server::new(None, arguments.dsu_server_address.parse().ok()).unwrap());

        dsu_server_handle = {
            let server = server.clone();
            Some(server.start(running.clone()))
        };
        println!("Created DSU server and moved it to another thread.");

        let controller_info = ControllerInfo {
            slot: 0,
            slot_state: SlotState::Connected,
            device_type: DeviceType::FullGyro,
            ..Default::default()
        };

        server.update_controller_info(controller_info);
        println!("Set DSU controller information.");

        dsu_server = Some(server);
    } else {
        dsu_server = None;
        dsu_server_handle = None;
    };

    // GAMEPAD SERVER
    let gamepad_server = gamepad_server::GamepadServer::new(arguments.gamepad_server_address.parse::<SocketAddr>().unwrap(), arguments.timeout);
    println!("Listening to gamepad...");
    gamepad_server.start(&dsu_server, &running);

    // CLOSE THREAD
    if let Some(handle) = dsu_server_handle {
        handle.join().unwrap();
    }
}
