use std::io::BufRead;
use std::io::BufReader;
use std::io::Write;
use std::net::TcpListener;

#[derive(serde::Deserialize, serde::Serialize)]
struct Data {
    method: String,
    number: serde_json::Number,
}

#[derive(serde::Deserialize, serde::Serialize)]
struct Response {
    method: String,
    prime: bool,
}

fn is_prime(n: i64) -> bool {
    if n <= 1 {
        return false;
    }
    for i in 2..=(n as f64).sqrt() as i64 {
        if n % i == 0 {
            return false;
        }
    }
    true
}

fn main() {
    let listener = TcpListener::bind("127.0.0.1:9999").unwrap();

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        std::thread::spawn(move || {
            let mut reader = BufReader::new(&stream);
            let mut writer = &stream;

            loop {
                let mut buffer = String::with_capacity(1024 * 1024);
                match reader.read_line(&mut buffer) {
                    Ok(0) => break,
                    Ok(_) => {
                        print!("{buffer}");
                        match serde_json::from_str::<Data>(&buffer) {
                            Ok(data) => {
                                if data.method != "isPrime" {
                                    eprintln!("unknown method: {}", data.method);
                                    writer.write_all(b"unknown method\n").unwrap();
                                    continue;
                                }
                                let prime = match data.number.as_i64() {
                                    Some(n) => is_prime(n),
                                    _ => false,
                                };

                                let response = Response {
                                    method: data.method,
                                    prime,
                                };
                                let response =
                                    serde_json::to_string(&response).unwrap().to_owned() + "\n";

                                writer.write_all(response.as_bytes()).unwrap();
                            }
                            Err(e) => {
                                eprintln!("error: {}", e);
                                writer.write_all(b"error\n").unwrap();
                                break;
                            }
                        }
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

