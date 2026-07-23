#!/usr/bin/env python3
"""
Raw Socket Modbus Client Helper

Provides a client that bypasses pymodbus to intentionally craft malformed,
fragmented, and pipelined Modbus TCP packets for robustness testing.
"""

import socket
import struct
import time
from typing import Optional, Tuple


class RawModbusClient:
    def __init__(self, host: str, port: int = 502, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self._next_tid = 1

    def connect(self) -> bool:
        """Establish TCP connection to the device."""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(self.timeout)
            self.sock.connect((self.host, self.port))
            # Disable Nagle's algorithm so we can control exact packet boundaries
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            return True
        except (socket.timeout, OSError):
            if self.sock:
                self.sock.close()
                self.sock = None
            return False

    def close(self):
        """Gracefully close the connection."""
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def abandon(self):
        """Silently drop the connection without sending FIN/RST.
        Requires OS-level support (e.g. iptables) in reality, but for a simple
        script we just discard the socket object to simulate the app dying.
        Note: Python's GC might still close it, so we mock it by just re-initing.
        """
        self.sock = None

    def build_read_holding_registers(self, unit_id: int, address: int, count: int, tid: Optional[int] = None) -> bytes:
        """Craft a standard Read Holding Registers (FC 03) request."""
        if tid is None:
            tid = self._next_tid
            self._next_tid = (self._next_tid + 1) & 0xFFFF

        protocol_id = 0
        pdu = struct.pack(">BHH", 3, address, count)
        length = len(pdu) + 1  # +1 for unit_id
        
        mbap = struct.pack(">HHHB", tid, protocol_id, length, unit_id)
        return mbap + pdu

    def send_raw(self, data: bytes):
        """Send raw bytes over the socket."""
        if not self.sock:
            raise RuntimeError("Socket not connected")
        self.sock.sendall(data)

    def recv_response(self, expected_tid: Optional[int] = None, timeout: Optional[float] = None) -> Optional[bytes]:
        """Receive a Modbus TCP response and optionally validate the TID."""
        if not self.sock:
            raise RuntimeError("Socket not connected")

        old_timeout = self.sock.gettimeout()
        if timeout is not None:
            self.sock.settimeout(timeout)

        try:
            # Read MBAP header (7 bytes)
            mbap_raw = self.sock.recv(7)
            if len(mbap_raw) < 7:
                return None
            
            tid, pid, length, uid = struct.unpack(">HHHB", mbap_raw)
            
            # Read PDU
            pdu_len = length - 1
            pdu_raw = self.sock.recv(pdu_len)
            
            if expected_tid is not None and tid != expected_tid:
                print(f"Warning: Expected TID {expected_tid}, got {tid}")
                
            return mbap_raw + pdu_raw
            
        except socket.timeout:
            return None
        finally:
            self.sock.settimeout(old_timeout)
