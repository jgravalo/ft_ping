# ft_ping

Replica of the `ping` command in C using raw sockets, with IPv4 support, round-trip time statistics, and ICMP error display in verbose mode. This project is based on the behavior and output of `ping` from `inetutils-2.0`.

---

## 📦 Features

- Sends ICMP Echo Request packets and waits for Echo Reply.
- Calculates and displays:
  - Round-trip time (RTT)
  - Packet loss percentage
  - Min/avg/max/mdev
- Implements raw socket usage (`SOCK_RAW`) with manual packet construction.
- Handles interrupt signal (`Ctrl+C`) to print final statistics.
- Uses `select()` for timeout management and responsiveness.
- Implements ICMP checksum manually.

---

## 🔧 Options

| Option | Description                                               |
|--------|-----------------------------------------------------------|
| `-v`   | Verbose mode. Shows ICMP error messages (e.g., unreachable). |
| `-?`   | Prints help message and exits.                            |

---

## ✅ Requirements

- Linux or Unix-based system
- Root privileges (`sudo`) to use raw sockets
- GCC compiler

---

## 🚀 Usage

```bash
make
sudo ./ft_ping [options] <hostname/IP>

## Output

PING google.com (142.250.185.14) 56(84) bytes of data.
64 bytes from 142.250.185.14: icmp_seq=0 ttl=118 time=15.74 ms
64 bytes from 142.250.185.14: icmp_seq=1 ttl=118 time=14.93 ms
^C
--- google.com ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 2002ms
rtt min/avg/max/mdev = 14.933/15.337/15.741/0.404 ms
