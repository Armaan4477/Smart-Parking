#!/usr/bin/env python3
import requests
import time
import random
import json
import argparse
from datetime import datetime

class ESP32Simulator:
    def __init__(self, api_base_url, device_ids, update_interval=10):
        """
        Initialize the ESP32 simulator
        
        :param api_base_url: Base URL for the API
        :param device_ids: List of device IDs to simulate
        :param update_interval: Interval in seconds between updates
        """
        self.api_base_url = api_base_url
        self.device_ids = device_ids
        self.update_interval = update_interval
        self.device_states = {}
        
        # Initialize master device state (ESP32)
        self.device_states["master"] = {
            "systemStatus": "online"
        }
        
        # Initialize slave device states (ESP8266)
        for device_id in self.device_ids:
            self.device_states[device_id] = {
                "isOccupied": False,
                "distance": random.randint(50, 150),
                "hasSensorError": False,
                "systemStatus": "online"
            }
    
    def send_health_ping(self, device_id):
        """
        Send a health ping to the API for a specific device
        
        :param device_id: Device ID to send health ping for
        :return: Success status
        """
        endpoint = f"{self.api_base_url}/api/parking/health"
        
        # For the master device
        if device_id == "master":
            payload = {
                "deviceId": "master",
                "systemStatus": "online",
                "wifiConnected": True,
                "slaveDevices": len(self.device_ids),
                "ip": "192.168.1.100",  # Simulated IP
                "httpPort": 80,
                "wsPort": 81,
                "discoveryMode": False,
                "physicalOverride": False
            }
        else:
            payload = {
                "deviceId": device_id,
                "systemStatus": self.device_states[device_id]["systemStatus"]
            }
        
        try:
            print(f"📡 Sending health ping for Device {device_id}...")
            response = requests.post(endpoint, json=payload, timeout=5)
            if response.status_code == 200:
                print(f"✅ Health ping successful for Device {device_id}")
                return True
            else:
                print(f"❌ Health ping failed for Device {device_id}: {response.status_code}, {response.text}")
                return False
        except Exception as e:
            print(f"❌ Error sending health ping for Device {device_id}: {e}")
            return False
    
    def update_sensor_data(self, device_id):
        """
        Send sensor data update to the API for a specific device
        
        :param device_id: Device ID to send sensor data for
        :return: Success status
        """
        endpoint = f"{self.api_base_url}/api/parking/sensor/{device_id}"
        
        # Get current state
        state = self.device_states[device_id]
        
        # Update sensor data if not in error state
        if not state["hasSensorError"]:
            if random.random() < 0.1:  # 10% chance to change occupancy state
                state["isOccupied"] = not state["isOccupied"]
                # Adjust distance based on occupancy
                if state["isOccupied"]:
                    state["distance"] = random.randint(5, 35)  # Close distance = occupied
                else:
                    state["distance"] = random.randint(50, 150)  # Far distance = vacant
        
        payload = {
            "distance": state["distance"],
            "hasSensorError": state["hasSensorError"],
            "isOffline": False,
            "lastUpdateMs": int(time.time() * 1000),
            "timeSinceUpdateMs": 0
        }
        
        try:
            print(f"📡 Updating sensor data for Device {device_id}...")
            response = requests.post(endpoint, json=payload, timeout=5)
            if response.status_code == 200:
                print(f"✅ Sensor data update successful for Device {device_id}: " + 
                      f"{'Occupied' if state['isOccupied'] else 'Vacant'} (distance: {state['distance']}cm)")
                return True
            else:
                print(f"❌ Sensor data update failed for Device {device_id}: {response.status_code}, {response.text}")
                return False
        except Exception as e:
            print(f"❌ Error updating sensor data for Device {device_id}: {e}")
            return False
    
    def update_device_status(self, device_id):
        """
        Update device status on the API
        
        :param device_id: Device ID to update status for
        :return: Success status
        """
        endpoint = f"{self.api_base_url}/api/parking/{device_id}"
        state = self.device_states[device_id]
        
        payload = {
            "parkingStatus": "occupied" if state["isOccupied"] else "open",
            "sensorError": state["hasSensorError"],
            "systemStatus": state["systemStatus"]
        }
        
        try:
            print(f"📡 Updating device status for Device {device_id}...")
            response = requests.put(endpoint, json=payload, timeout=5)
            if response.status_code == 200:
                print(f"✅ Device status update successful for Device {device_id}")
                return True
            else:
                print(f"❌ Device status update failed for Device {device_id}: {response.status_code}, {response.text}")
                return False
        except Exception as e:
            print(f"❌ Error updating device status for Device {device_id}: {e}")
            return False

    def toggle_device_occupation(self, device_id):
        """
        Manually toggle a device's occupation status for demo purposes
        
        :param device_id: Device ID to toggle status
        """
        if device_id in self.device_states:
            state = self.device_states[device_id]
            state["isOccupied"] = not state["isOccupied"]
            
            # Adjust distance based on occupancy
            if state["isOccupied"]:
                state["distance"] = random.randint(5, 35)
            else:
                state["distance"] = random.randint(50, 150)
            
            print(f"🔄 Device {device_id} status manually toggled to: {'Occupied' if state['isOccupied'] else 'Vacant'}")
            
            # Update status immediately
            self.update_sensor_data(device_id)
            self.update_device_status(device_id)
        else:
            print(f"❌ Device {device_id} not found")

    def toggle_sensor_error(self, device_id):
        """
        Toggle sensor error status for a device
        
        :param device_id: Device ID to toggle error status
        """
        if device_id in self.device_states:
            state = self.device_states[device_id]
            state["hasSensorError"] = not state["hasSensorError"]
            print(f"🔄 Device {device_id} sensor error status toggled to: {state['hasSensorError']}")
            
            # Update status immediately
            self.update_sensor_data(device_id)
        else:
            print(f"❌ Device {device_id} not found")
    
    def toggle_system_status(self, device_id):
        """
        Toggle system status (online/offline) for a device
        
        :param device_id: Device ID to toggle system status
        """
        if device_id in self.device_states:
            state = self.device_states[device_id]
            state["systemStatus"] = "offline" if state["systemStatus"] == "online" else "online"
            print(f"🔄 Device {device_id} system status toggled to: {state['systemStatus']}")
            
            # Update status immediately
            self.send_health_ping(device_id)
        else:
            print(f"❌ Device {device_id} not found")
    
    def run(self):
        """
        Run the simulator in an infinite loop
        """
        last_health_ping = 0
        health_ping_interval = 20  # Seconds
        
        try:
            print(f"🚀 Starting Smart Parking simulator")
            print(f"🔵 Simulating 1 master ESP32 device (device ID: master)")
            print(f"🟠 Simulating {len(self.device_ids)} slave ESP8266 devices (device IDs: {', '.join(map(str, self.device_ids))})")
            print(f"🌐 API Base URL: {self.api_base_url}")
            print(f"⏱️  Update interval: {self.update_interval} seconds")
            print("📋 Available commands:")
            print("  - 'o <device_id>': Toggle occupation status for slave device")
            print("  - 'e <device_id>': Toggle sensor error status for slave device")
            print("  - 's <device_id>': Toggle system status (online/offline) for any device")
            print("  - 's master': Toggle system status of the master ESP32 device")
            print("  - 'q': Quit simulator")
            
            while True:
                # Process user commands (non-blocking)
                import select
                import sys
                
                # Check if input is available
                if select.select([sys.stdin], [], [], 0)[0]:
                    command = input().strip()
                    if command.lower() == 'q':
                        print("Stopping simulator...")
                        break
                    elif command.lower().startswith('o '):
                        try:
                            device_id = command.split()[1]
                            self.toggle_device_occupation(device_id)
                        except (IndexError, ValueError):
                            print("❌ Invalid command format. Use 'o <device_id>'")
                    elif command.lower().startswith('e '):
                        try:
                            device_id = command.split()[1]
                            self.toggle_sensor_error(device_id)
                        except (IndexError, ValueError):
                            print("❌ Invalid command format. Use 'e <device_id>'")
                    elif command.lower().startswith('s '):
                        try:
                            device_id = command.split()[1]
                            self.toggle_system_status(device_id)
                        except (IndexError, ValueError):
                            print("❌ Invalid command format. Use 's <device_id>'")
                
                # Send regular updates
                current_time = time.time()
                
                # Health ping every 20 seconds
                if current_time - last_health_ping >= health_ping_interval:
                    # First send health ping for master device
                    self.send_health_ping("master")
                    
                    # Then send for each slave device
                    for device_id in self.device_ids:
                        self.send_health_ping(device_id)
                    
                    last_health_ping = current_time
                
                # Update sensor data and device status for each device
                for device_id in self.device_ids:
                    self.update_sensor_data(device_id)
                    self.update_device_status(device_id)
                
                # Sleep for update interval
                time.sleep(self.update_interval)
                
        except KeyboardInterrupt:
            print("\nSimulator stopped by user.")
    
def main():
    parser = argparse.ArgumentParser(description='ESP32 Parking Sensor Simulator')
    parser.add_argument('--api', default='https://smart-parking-44.vercel.app', 
                        help='API base URL (default: https://smart-parking-44.vercel.app)')
    parser.add_argument('--devices', nargs='+', default=['1', '2', '3', '4'], 
                        help='List of device IDs to simulate (default: 1,2,3,4 - representing 4 parking slots)')
    parser.add_argument('--interval', type=int, default=10, 
                        help='Update interval in seconds (default: 10)')
    
    args = parser.parse_args()
    
    simulator = ESP32Simulator(
        api_base_url=args.api,
        device_ids=args.devices,
        update_interval=args.interval
    )
    
    simulator.run()

if __name__ == "__main__":
    main()