#!/usr/bin/env python3
"""
Modbus TCP Server Robustness and Edge Case Tests

These tests verify the device's behavior against malformed, fragmented,
and aggressive connection patterns that caused field failures.
"""

import time
import pytest
from tests.integration.raw_client import RawModbusClient

def assert_device_responsive(host: str, port: int, unit_id: int):
    """Invariant helper to ensure the device is still alive and answering."""
    client = RawModbusClient(host, port, timeout=2.0)
    assert client.connect(), "Failed to connect for health check"
    
    req = client.build_read_holding_registers(unit_id, 0, 1)
    client.send_raw(req)
    resp = client.recv_response(timeout=2.0)
    
    assert resp is not None, "Device did not answer health check"
    client.close()

@pytest.mark.hardware
class TestModbusRobustness:
    
    def test_hostage_connection(self, modbus_host, modbus_port, unit_id, idle_timeout_s):
        """
        Verify the server closes an idle connection and accepts new ones.
        
        Test steps:
        1. Connect client A and abandon it silently (no FIN).
        2. Attempt to connect client B immediately (will queue in backlog).
        3. Wait for the idle timeout on the device.
        4. Client B should now be accepted and served.
        """
        client_a = RawModbusClient(modbus_host, modbus_port)
        assert client_a.connect()
        
        # We send one request just to prove it works
        req_a = client_a.build_read_holding_registers(unit_id, 0, 1)
        client_a.send_raw(req_a)
        assert client_a.recv_response() is not None
        
        # "Abandon" without closing socket
        # Note: in Python, the socket remains open until garbage collected
        
        client_b = RawModbusClient(modbus_host, modbus_port, timeout=idle_timeout_s + 5.0)
        assert client_b.connect()
        
        req_b = client_b.build_read_holding_registers(unit_id, 0, 1)
        start_time = time.time()
        client_b.send_raw(req_b)
        
        resp = client_b.recv_response(timeout=idle_timeout_s + 5.0)
        elapsed = time.time() - start_time
        
        assert resp is not None, "Client B was never served after Client A abandoned"
        # The response should take roughly `idle_timeout_s` as client B waits in the backlog
        print(f"Client B served after {elapsed:.2f}s (idle timeout is {idle_timeout_s}s)")
        
        client_b.close()
        client_a.close() # Clean up

    def test_pipelined_adus(self, modbus_host, modbus_port, unit_id):
        """
        Verify the server can parse multiple requests sent in a single packet.
        """
        client = RawModbusClient(modbus_host, modbus_port)
        assert client.connect()
        
        tid1 = 100
        tid2 = 101
        req1 = client.build_read_holding_registers(unit_id, 0, 1, tid=tid1)
        req2 = client.build_read_holding_registers(unit_id, 1, 1, tid=tid2)
        
        # Send back-to-back in one payload
        client.send_raw(req1 + req2)
        
        resp1 = client.recv_response(expected_tid=tid1)
        assert resp1 is not None, "Did not receive first pipelined response"
        
        resp2 = client.recv_response(expected_tid=tid2)
        assert resp2 is not None, "Did not receive second pipelined response"
        
        client.close()

    def test_segmented_adu(self, modbus_host, modbus_port, unit_id):
        """
        Verify the server can reassemble a single ADU split across packets.
        """
        client = RawModbusClient(modbus_host, modbus_port)
        assert client.connect()
        
        req = client.build_read_holding_registers(unit_id, 0, 1, tid=200)
        
        # Split the request in half
        split_point = 4
        client.send_raw(req[:split_point])
        time.sleep(0.5)
        client.send_raw(req[split_point:])
        
        resp = client.recv_response(expected_tid=200)
        assert resp is not None, "Did not receive response to segmented ADU"
        
        client.close()

    def test_malformed_mbap(self, modbus_host, modbus_port, unit_id):
        """
        Verify the server survives an oversized or invalid length field.
        """
        client = RawModbusClient(modbus_host, modbus_port)
        assert client.connect()
        
        # Build a valid request, but mutate the length to be larger than the rx buffer (e.g. 500)
        req = bytearray(client.build_read_holding_registers(unit_id, 0, 1, tid=300))
        req[4:6] = (500).to_bytes(2, byteorder='big')
        
        client.send_raw(bytes(req))
        # The device should drop it and log, meaning we get no response
        resp = client.recv_response(timeout=2.0)
        assert resp is None
        
        client.close()
        
        # Prove the device is still alive
        assert_device_responsive(modbus_host, modbus_port, unit_id)

    def test_reconnect_churn(self, modbus_host, modbus_port, unit_id):
        """
        Verify the server survives rapid connect/disconnect cycles.
        """
        for _ in range(50):
            client = RawModbusClient(modbus_host, modbus_port)
            assert client.connect()
            
            req = client.build_read_holding_registers(unit_id, 0, 1)
            client.send_raw(req)
            resp = client.recv_response(timeout=1.0)
            
            assert resp is not None
            client.close()
        
        assert_device_responsive(modbus_host, modbus_port, unit_id)
