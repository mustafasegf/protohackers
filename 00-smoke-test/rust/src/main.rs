use std::io::BufRead;
use std::io::BufReader;
use std::io::Read;
use std::io::Write;
use std::net::TcpListener;

fn main() {
    let listener = TcpListener::bind("127.0.0.1:9999").unwrap();

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        std::thread::spawn(move || {
            let mut reader = BufReader::new(&stream);
            let mut writer = &stream;

            loop {
                let mut buffer = Vec::with_capacity(1024);

                match reader.read_to_end(&mut buffer) {
                    Ok(0) => break,
                    Ok(_) => {
                        println!("received: {}", String::from_utf8_lossy(&buffer));
                        writer.write_all(&buffer).unwrap();
                    }
                    Err(e) => {
                        eprintln!("error: {}", e);
                        writer.write_all(b"error\n").unwrap();
                        break;
                    }
                }
            }
        });
    }
}
