use std::{net::TcpStream, io::{BufReader, BufRead, Write}};

fn main() {
    let mut stream = TcpStream::connect("127.0.0.1:9999").unwrap();
    let mut reader = BufReader::new(&stream);

    let mut buf = String::new();
    reader.read_line(&mut buf).unwrap();

    println!("{buf}");

    stream.write_all(b"alice\n").unwrap();
    stream.flush().unwrap();

    stream.write_all(b"Just one").unwrap();
    stream.flush().unwrap();

    stream.write_all(b" more thing").unwrap();
    stream.flush().unwrap();

    stream.write_all(b"\n").unwrap();
    stream.flush().unwrap();
}
