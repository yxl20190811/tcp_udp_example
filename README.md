Network Log Forwarder
📦 Network Log Forwarder

A lightweight, modular C implementation for collecting and forwarding logs over TCP/UDP. Designed to bridge legacy TCP-based clients with modern UDP-only logging backends.

C / POSIX
License: MIT
📦 Overview
This project provides three core utilities:

udp_server: Listens on a UDP port and appends all received datagrams to a log file.
tcp_server: Accepts TCP connections and forwards every received byte to a configured UDP endpoint (acts as a TCP-to-UDP bridge).
test_client: Sends formatted log messages via TCP or UDP for testing and simulation.
Built with simplicity, reliability, and POSIX compliance in mind—ideal for embedded systems, legacy integration, or custom logging pipelines.

🗂️ Project Structure
```text
.
├── .vscode/                 # VS Code debug & build configs
├── bin/                     # Compiled executables (generated)
├── src/
│   ├── udp_server.c         # UDP log collector
│   ├── tcp_server.c         # TCP-to-UDP forwarder (multi-threaded)
│   ├── test_client.c        # Test client with auto-formatted logs
│   ├── send_all.h           # Reliable TCP send utility (header)
│   └── send_all.c           # Reliable TCP send utility (implementation)
└── Makefile                 # Build automation
```

All source code resides in src/. Binaries are output to bin/ upon build.

⚙️ Build
Ensure you have a standard C toolchain (gcc, make) installed.

bash
编辑
git clone <repo-url>
cd <project-dir>
make
✅ Output:

bin/udp_server, bin/tcp_server, bin/test_client

To clean build artifacts:

bash
编辑
make clean
🚀 Usage
1. Start the UDP Log Collector
bash
编辑
./bin/udp_server <udp_port> <log_file>
Example:

bash
编辑
./bin/udp_server 5140 /var/log/app.log
Listens on UDP port 5140
Appends all incoming datagrams to /var/log/app.log
Runs indefinitely until terminated
2. (Optional) Start the TCP-to-UDP Bridge
bash
编辑
./bin/tcp_server <tcp_listen_port> <udp_target_host> <udp_target_port>
Example:

bash
编辑
./bin/tcp_server 9999 127.0.0.1 5140
Accepts TCP clients on port 9999
Forwards all received data to 127.0.0.1:5140 over UDP
Supports concurrent clients via pthreads
💡 Use this when your clients only support TCP but your logging backend is UDP-only.

3. Send Test Logs
bash
编辑
./bin/test_client [tcp|udp] <host> <port> "<message>"
Examples:

bash
编辑
# Send via UDP directly to udp_server.
./bin/test_client udp 127.0.0.1 5140 "System started"

# Send via TCP to tcp_server (which forwards to UDP).
./bin/test_client tcp 127.0.0.1 9999 "Legacy device heartbeat"
Log Format
text
编辑
[YYYY-MM-DD HH:MM:SS][user_message][source_file][line_number]
Example output in log file:

text
编辑
[2025-12-07 10:30:45][System started][/path/to/test_client.c][68]
🔧 How It Works
Architecture
text
编辑
+--------------+     +------------------+     +------------------+
| TCP Client   | --> | tcp_server       | --> |                  |
| (e.g., old   | TCP | (TCP→UDP bridge) | UDP | udp_server       |
| device)      |     +------------------+     | → log file       |
+--------------+                              +------------------+
                                               ^
                                               |
+--------------+ ----------------------------> |
| UDP Client   | UDP                           |
| (modern app) |                               |
+--------------+
Key Features
Reliable TCP Sending: The send_all() utility ensures complete transmission even if send() returns partial writes.
Thread-Safe TCP Handling: Each TCP client runs in its own detached thread.
Real-Time Logging: UDP server disables stdio buffering for immediate disk writes.
Zero External Dependencies: Pure POSIX sockets and standard C library.
🛑 Limitations & Considerations
No Encryption or Auth: Intended for trusted networks only.
UDP Is Unreliable: Datagrams may be dropped under load—unsuitable for critical audit logs.
Thread Per Connection: tcp_server scales linearly with clients; not ideal for >1k concurrent connections.
IPv4 Only: No IPv6 support in current version.
🛠️ Development
VS Code Integration
The .vscode/ folder includes:

tasks.json: Build with Ctrl+Shift+B
launch.json: Debug any binary with F5
Extending the System
Possible enhancements:

Add TLS support for secure TCP transport
Implement log rotation in udp_server
Support configuration files
Add metrics (e.g., message rate, client count)
Migrate to epoll/kqueue for high-concurrency TCP
📜 License
MIT License — see LICENSE for details.

