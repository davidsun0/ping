# Cloudflare Systems Internship Application

Author: David Sun
Github: [ping](https://github.com/whetfire/ping)

## Requirements met
- Written in C
- CLI tool with usage: `$ ./ping [host]`
- Sends ICMP echo requests infintely (once per second)
- Reports round trip statistics or loss if request times out

## Extra features
- DNS lookup: `$ ./ping cloudflare.com`
- Set the TTL for each packet with -t: `$ ./ping -t 32 1.1.1.1`
- Set a deadline in milliseconds with -w: `$ ./ping -w 500 1.1.1.1`

## Usage and notes

- Build with `$ make` or `$ gcc ping.c -o ping`
- Ping must be run as super user because it uses raw sockets
