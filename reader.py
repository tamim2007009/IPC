import mmap
import struct
import sys
import os

# Must match the C++ program
SHM_NAME = "Local\\IPCSharedMemory"
SHM_SIZE = 1024

class SharedData:
    """Structure to match C++ SharedData struct"""
    def __init__(self):
        self.process_id = 0
        self.message = ""
        self.data_ready = False
        self.counter = 0
        self.temperature = 0.0
        self.coordinates = [0.0, 0.0, 0.0]
        self.user_name = ""
        self.timestamp = 0
        self.data_array = [0] * 10
    
    @staticmethod
    def unpack_from_bytes(data):
        """Unpack binary data from shared memory"""
        # Structure layout:
        # int processId (4 bytes)
        # char message[500] (500 bytes)
        # bool dataReady (1 byte)
        # padding (3 bytes for alignment)
        # int counter (4 bytes)
        # double temperature (8 bytes)
        # float coordinates[3] (12 bytes)
        # char userName[50] (50 bytes)
        # padding (6 bytes for alignment)
        # long long timestamp (8 bytes)
        # int dataArray[10] (40 bytes)
        
        shared = SharedData()
        offset = 0
        
        # Extract process ID (4 bytes)
        shared.process_id = struct.unpack('i', data[offset:offset+4])[0]
        offset += 4
        
        # Extract message (500 bytes)
        message_bytes = data[offset:offset+500]
        null_pos = message_bytes.find(b'\x00')
        if null_pos != -1:
            message_bytes = message_bytes[:null_pos]
        shared.message = message_bytes.decode('utf-8', errors='ignore')
        offset += 500
        
        # Extract dataReady flag (1 byte)
        shared.data_ready = bool(data[offset])
        offset += 1
        
        # Skip padding (3 bytes for alignment)
        offset += 3
        
        # Extract counter (4 bytes)
        shared.counter = struct.unpack('i', data[offset:offset+4])[0]
        offset += 4
        
        # Extract temperature (8 bytes)
        shared.temperature = struct.unpack('d', data[offset:offset+8])[0]
        offset += 8
        
        # Extract coordinates (3 floats = 12 bytes)
        shared.coordinates = list(struct.unpack('fff', data[offset:offset+12]))
        offset += 12
        
        # Extract userName (50 bytes)
        user_bytes = data[offset:offset+50]
        null_pos = user_bytes.find(b'\x00')
        if null_pos != -1:
            user_bytes = user_bytes[:null_pos]
        shared.user_name = user_bytes.decode('utf-8', errors='ignore')
        offset += 50
        
        # Skip padding (6 bytes for alignment)
        offset += 6
        
        # Extract timestamp (8 bytes)
        shared.timestamp = struct.unpack('Q', data[offset:offset+8])[0]
        offset += 8
        
        # Extract data array (10 ints = 40 bytes)
        shared.data_array = list(struct.unpack('10i', data[offset:offset+40]))
        
        return shared

def main():
    print("=== Python Program - Shared Memory Reader ===\n")
    
    try:
        # Connect to existing shared memory
        print(f"Attempting to connect to shared memory: {SHM_NAME}")
        
        # Open existing shared memory
        shm = mmap.mmap(-1, SHM_SIZE, SHM_NAME)
        print(f"Successfully connected to shared memory")
        
        # Read data from shared memory
        print("\nReading data from shared memory...")
        raw_data = shm.read(SHM_SIZE)
        
        # Unpack the data
        shared_data = SharedData.unpack_from_bytes(raw_data)
        
        # Display the output
        print("\n=== Data Retrieved from Shared Memory ===")
        print(f"Data Ready Flag: {shared_data.data_ready}")
        print(f"C++ Process ID: {shared_data.process_id}")
        print(f"Counter: {shared_data.counter}")
        print(f"Temperature: {shared_data.temperature} °C")
        print(f"Coordinates: {shared_data.coordinates}")
        print(f"Username: {shared_data.user_name}")
        print(f"Timestamp: {shared_data.timestamp} ms")
        print(f"Data Array: {shared_data.data_array}")
        print(f"\nMessage from C++ Program:")
        print("-" * 50)
        print(shared_data.message)
        print("-" * 50)
        
        print("\n=== Python Program Output ===")
        print(f"Successfully read {len(shared_data.message)} characters from shared memory")
        print(f"Python Process ID: {os.getpid()}")
        print("=" * 50)
        
        # Close shared memory
        shm.close()
        print("\nShared memory connection closed.")
        
    except FileNotFoundError:
        print("\nError: Shared memory segment not found!")
        print("Please run the C++ program first to create the shared memory.")
        sys.exit(1)
    
    except Exception as e:
        print(f"\nError occurred: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
