#![allow(unused_imports, dead_code, unused_variables, unused_mut)]
use std::collections::HashSet;
use std::io::Write;
use tokio::io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader};

use std::sync::atomic::AtomicU32;
use std::sync::Arc;
use tokio::sync::RwLock;

use tokio::net::TcpListener;
use tokio::sync::broadcast;

#[derive(Debug, Clone)]
struct Msg {
    user: String,
    data: String,
}

#[tokio::main]
async fn main() {
    let listener = TcpListener::bind("127.0.0.1:9999").await.unwrap();
    let thread_number = AtomicU32::new(0);

    let (tx, rx): (broadcast::Sender<Msg>, broadcast::Receiver<Msg>) = broadcast::channel(10);

    let users: Arc<RwLock<HashSet<String>>> = Arc::new(RwLock::new(HashSet::new()));

    loop {
        let (stream, socket_addr) = listener.accept().await.unwrap();

        tokio::spawn({
            let thread_number = thread_number.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
            let mut output_file = std::fs::File::create(format!("output-{thread_number}")).unwrap();

            let tx = tx.clone();
            let mut rx = tx.subscribe();
            let users = users.clone();

            async move {
                let mut stream = BufReader::new(stream);

                println!("new connection: {thread_number}");

                let mut user = String::new();

                stream
                    .write_all(b"Welcome to budgetchat! What shall I call you?\n")
                    .await
                    .unwrap();

                loop {
                    let mut buf = String::with_capacity(1024);

                    tokio::select! {
                        Ok(msg) = rx.recv() => {
                            if user.is_empty() {
                                continue;
                            }

                            let prefix_msg = format!("[{user}]");
                            let prefix_join = format!("* {user}");

                            if msg.user != user {
                                stream.write_all(msg.data.as_bytes()).await.unwrap();
                            }
                        }
                        Ok(len) = stream.read_line(&mut buf) => {
                            output_file.write_all(buf.as_bytes()).unwrap();

                            match len {
                                0 => {
                                    let data = format!("* {user} has left the room\n");

                                    println!("{data}");
                                    let msg = Msg {
                                        user: user.clone(),
                                        data: data,
                                    };

                                    tx.send(msg).unwrap();
                                    println!("closing connection: {thread_number}");
                                    users.write().await.remove(&user);

                                    break;
                                }
                                _ => {
                                    println!("buf: {buf}");
                                    // check if user is set or not
                                    if user.is_empty() {
                                        user = buf.trim().to_string();
                                        println!("user: {user}");
                                        // check if user is alphanum
                                        if !user.chars().all(char::is_alphanumeric) || user.is_empty() {
                                            stream
                                                .write_all(
                                                    b"Invalid username. Please use only alphanumeric characters.",
                                                )
                                                .await
                                                .unwrap();
                                            println!("closing connection: {thread_number}");
                                            return;
                                        }
                                        {
                                            let mut users = users.write().await;

                                            let mut temp = String::from("* The room contains: ");
                                            for u in users.iter() {
                                                temp.push_str(&u);
                                                temp.push_str(", ");
                                            }
                                            // ini jelek
                                            temp.pop();
                                            temp.pop();
                                            temp.push_str("\n");

                                            println!("temp: {temp}");
                                            stream.write_all(temp.as_bytes()).await.unwrap();

                                            users.insert(user.clone());
                                        }
                                        let data = format!("* {user} has entered the room\n");
                                        let msg = Msg {
                                            user: user.clone(),
                                            data: data,
                                        };
                                        tx.send(msg).unwrap();

                                    } else {
                                        buf = buf.trim().to_string();
                                        let data = format!("[{user}] {buf}");
                                        println!("{data}");
                                        let msg = Msg {
                                            user: user.clone(),
                                            data: data,
                                        };
                                        tx.send(msg).unwrap();

                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
    }
}
