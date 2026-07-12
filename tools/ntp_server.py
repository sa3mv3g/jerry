import socket, struct, time

NTP_DELTA = 2208988800 # Seconds between 1900 (NTP epoch) and 1970 (UNIX epoch)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 123))
print("[NTP Server] Ready! Waiting for Jerry on UDP Port 123...")

while True:
    data, addr = sock.recvfrom(1024)
    print(f"[NTP Server] Request received from Jerry at {addr[0]}!")
    
    # Extract incoming transmit timestamp (bytes 40-47) to echo as Origin Timestamp
    orig_sec, orig_frac = struct.unpack("!II", data[40:48])
    
    # Get current PC time
    t = time.time() + NTP_DELTA
    cur_sec, cur_frac = int(t), int((t - int(t)) * (2**32))
    
    # Build NTPv4 Server Header (LI=0, VN=4, Mode=4 -> 0x24) + Stratum 1
    header = struct.pack("!BBBBIII", 0x24, 1, 6, 236, 0, 0, 0x4C4F434C)
    
    # Pack Ref, Origin, Receive, and Transmit Timestamps
    timestamps = struct.pack("!IIIIIIII", 
        cur_sec, cur_frac, orig_sec, orig_frac, cur_sec, cur_frac, cur_sec, cur_frac)
    
    sock.sendto(header + timestamps, addr)
    print(f"[NTP Server] Sent time sync reply back to {addr[0]}!")