#!/usr/bin/env python3
"""
Modbus TCP Server Missing FIN (Half-Open Socket) Reproduction Script

This script programmatically reproduces the firmware-side missing-FIN bug:
1. Connects Client A (port 502) and holds it open/silent, holding the device hostage.
2. Connects Client B immediately (succeeds at TCP layer, sits unaccepted in backlog).
3. Client B sends a Modbus request (Read Holding Registers).
4. Client B waits for a response (times out because the device is blocked on Client A).
5. Client B closes its sending side (sends FIN).
6. Client B checks if the device sends its FIN back:
   - If the bug is present: The device never sends its FIN. Client B's socket read will
     block/time out (the connection is stuck half-open/half-closed in CLOSE_WAIT).
   - If fixed: The device sends its FIN, and Client B's read returns EOF (0 bytes) instantly.
"""

import sys
import socket
import time

def run_reproduction(ip: str, port: int, unit_id: int):
    # A standard Modbus TCP ADU: Read Holding Registers (FC 0x03, Addr 0, Count 1)
    modbus_request = b"\x00\x01\x00\x00\x00\x06" + bytes([unit_id]) + b"\x03\x00\x00\x00\x01"

    print(f"[*] Target device: {ip}:{port} (Unit ID: {unit_id})")
    print("[*] STEP 1: Connecting Client A to hold the sequential Modbus thread hostage...")
    try:
        sock_a = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_a.settimeout(5.0)
        sock_a.connect((ip, port))
        print("[+] Client A connected successfully.")
    except Exception as e:
        print(f"[-] ERROR: Failed to connect Client A: {e}")
        sys.exit(1)

    print("[*] Sending initial request on Client A to verify responsiveness...")
    try:
        sock_a.sendall(modbus_request)
        resp_a = sock_a.recv(1024)
        if len(resp_a) > 0:
            print(f"[+] Client A received response ({len(resp_a)} bytes): {resp_a.hex()}")
        else:
            print("[-] ERROR: Client A received empty response.")
            sys.exit(1)
    except Exception as e:
        print(f"[-] ERROR: Client A request failed: {e}")
        sock_a.close()
        sys.exit(1)

    print("\n[*] STEP 2: Connecting Client B immediately (will queue in listen backlog)...")
    try:
        sock_b = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_b.settimeout(2.0)
        sock_b.connect((ip, port))
        print("[+] Client B TCP handshake succeeded (completed by background stack).")
    except Exception as e:
        print(f"[-] ERROR: Failed to connect Client B: {e}")
        sock_a.close()
        sys.exit(1)

    print("[*] Sending Modbus request on Client B...")
    try:
        sock_b.sendall(modbus_request)
    except Exception as e:
        print(f"[-] ERROR: Client B failed to send: {e}")
        sock_a.close()
        sock_b.close()
        sys.exit(1)

    print("[*] Waiting for Client B response (expecting timeout as task is blocked)...")
    try:
        resp_b = sock_b.recv(1024)
        if len(resp_b) > 0:
            print(f"[!] Warning: Client B actually received data: {resp_b.hex()}")
            print("[!] This implies the server is not blocked/sequential or slot was available!")
        else:
            print("[-] Received empty response on Client B.")
    except socket.timeout:
        print("[+] Client B timed out as expected (confirmed device is blocked).")

    print("\n[*] STEP 3: Closing Client B sending-half cleanly (sending FIN to device)...")
    try:
        sock_b.shutdown(socket.SHUT_WR)
        print("[+] Client B sent FIN to device.")
    except Exception as e:
        print(f"[-] ERROR: Client B failed to shutdown: {e}")
        sock_a.close()
        sock_b.close()
        sys.exit(1)

    print("[*] STEP 4: Verifying if the device sends its FIN back...")
    print("[*] Reading from Client B's socket. (Instant EOF = OK, Blocking/Timeout = BUG)...")
    start_time = time.time()
    try:
        # If the device sends its FIN back, recv() should return b"" (EOF) instantly.
        # If the bug is present, recv() will block and time out after 2.0 seconds.
        sock_b.settimeout(2.0)
        final_read = sock_b.recv(1024)
        elapsed = time.time() - start_time
        if final_read == b"":
            print(f"\n[✓] SUCCESS: Instant EOF received after {elapsed:.3f}s!")
            print("[✓] The device cleanly sent its own FIN back. Socket closed fully.")
        else:
            print(f"\n[!] Unexpected data received ({len(final_read)} bytes): {final_read.hex()}")
    except socket.timeout:
        elapsed = time.time() - start_time
        print(f"\n[❌] BUG CONFIRMED: Client B's read timed out after {elapsed:.3f}s!")
        print("[❌] The device ACKed Client B's FIN but NEVER sent its own FIN back.")
        print("[❌] Connection B is trapped in CLOSE_WAIT (half-closed) on the device!")

    print("\n[*] Cleaning up...")
    sock_a.close()
    sock_b.close()
    print("[*] Done.")

if __name__ == "__main__":
    ip = "192.168.0.105"
    port = 502
    unit_id = 1

    if len(sys.argv) > 1:
        ip = sys.argv[1]
    if len(sys.argv) > 2:
        try:
            port = int(sys.argv[2])
        except ValueError:
            print("[-] Invalid port number, using 502")
    if len(sys.argv) > 3:
        try:
            unit_id = int(sys.argv[3])
        except ValueError:
            print("[-] Invalid Unit ID, using 1")

    run_reproduction(ip, port, unit_id)
