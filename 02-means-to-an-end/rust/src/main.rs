use std::i64;
use std::io::prelude::*;
use std::io::BufReader;
use std::net::TcpListener;
use std::sync::atomic::AtomicU32;

fn main() {
    let listener = TcpListener::bind("127.0.0.1:9999").unwrap();
    let thread_number = AtomicU32::new(0);

    for stream in listener.incoming() {
        let mut db = vec![0i64; i32::MAX as usize];
        let stream = stream.unwrap();

        std::thread::spawn({
            let thread_number = thread_number.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
            let mut output_file = std::fs::File::create(format!("output-{thread_number}")).unwrap();

            move || {
                let mut reader = BufReader::new(&stream);
                let mut writer = &stream;

                println!("new connection: {thread_number}");
                loop {
                    let mut buffer = [0; 9];
                    match reader.read_exact(&mut buffer) {
                        Ok(_) => {
                            output_file.write_all(&buffer).unwrap();
                            println!("received: {:?}", buffer);
                            match buffer[0] {
                                b'I' => {
                                    let time = i32::from_be_bytes([
                                        buffer[1], buffer[2], buffer[3], buffer[4],
                                    ]) as usize;
                                    let price = i32::from_be_bytes([
                                        buffer[5], buffer[6], buffer[7], buffer[8],
                                    ]);

                                    println!("time: {}, price: {}", time, price);

                                    // asume time is unique. add to db
                                    if db[time] != 0 {
                                        println!("time: {time} already exists");
                                    }

                                    db[time] = price as i64;
                                }
                                b'Q' => {
                                    let mintime = i32::from_be_bytes([
                                        buffer[1], buffer[2], buffer[3], buffer[4],
                                    ])
                                    .max(0);

                                    let maxtime = i32::from_be_bytes([
                                        buffer[5], buffer[6], buffer[7], buffer[8],
                                    ])
                                    .max(0);

                                    println!("mintime: {}, maxtime: {}", mintime, maxtime);

                                    let mintime = mintime as usize;
                                    let maxtime = maxtime as usize;

                                    if mintime > maxtime {
                                        let resp = 0i32.to_be_bytes();
                                        writer.write(&resp).unwrap();
                                        continue;
                                    }

                                    let count =
                                        db[mintime..=maxtime].iter().filter(|&x| *x != 0).count();
                                    println!("count: {count}");

                                    let mean = match count {
                                        0 => 0,
                                        _ => {
                                            (db[mintime..=maxtime].iter().sum::<i64>()
                                                / count as i64)
                                                as i32
                                        }
                                    };

                                    let resp = mean.to_be_bytes();
                                    println!("response: {mean}");

                                    writer.write(&resp).unwrap();
                                }
                                _ => {
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
            }
        });
    }
}
