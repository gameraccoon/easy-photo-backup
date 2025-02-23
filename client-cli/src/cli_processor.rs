pub(crate) struct CliProcessor {
    receiver: std::sync::mpsc::Receiver<String>,
    sender: std::sync::mpsc::Sender<String>,
}

impl CliProcessor {
    pub fn new(
        receiver: std::sync::mpsc::Receiver<String>,
        sender: std::sync::mpsc::Sender<String>,
    ) -> CliProcessor {
        CliProcessor { receiver, sender }
    }

    pub fn start(&mut self) {
        let mut buffer = String::new();
        loop {
            buffer.clear();
            match std::io::stdin().read_line(&mut buffer) {
                Ok(bytes_read) => {
                    if bytes_read == 0 {
                        break;
                    }
                }
                Err(e) => {
                    println!(
                        "Failed to read from stdin, closing the client connection: {}",
                        e
                    );
                    break;
                }
            };

            if buffer.trim() == ".q" {
                break;
            }

            match self.sender.send(buffer.trim().to_string()) {
                Ok(_) => {}
                Err(e) => {
                    println!("Failed to send data to the network thread: {}", e);
                    break;
                }
            };
        }

        std::process::exit(0);
    }
}
