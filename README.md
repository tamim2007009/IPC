# Inter-Process Communication (IPC) Assignment
## Shared Memory Communication between C++ and Python

### Assignment Overview
This project demonstrates inter-process communication using shared memory:
- **C++ Program (writer.cpp)**: Creates shared memory and writes data
- **Python Program (reader.py)**: Reads data from the same shared memory segment

---

## Prerequisites

### For C++ Program
- g++ compiler
- POSIX shared memory support (Linux/Unix)

### For Python Program
- Python 3.x
- `sysv_ipc` library

---

## Installation Steps

### Step 1: Install Python Dependencies
```bash
pip install sysv_ipc
```

Or if using pip3:
```bash
pip3 install sysv_ipc
```

### Step 2: Compile the C++ Program
```bash
g++ writer.cpp -o writer
```

---

## Execution Instructions

### Method 1: Sequential Execution (Recommended for Learning)

#### Terminal 1 - Run C++ Program First:
```bash
./writer
```

This will:
- Create a shared memory segment
- Write data to it
- Display the output
- Wait for you to press Enter

#### Terminal 2 - Run Python Program:
```bash
python3 reader.py
```

This will:
- Connect to the existing shared memory
- Read the data written by C++
- Display the output

---

### Method 2: Automated Execution Script

You can also create a shell script to run both programs:

```bash
# Run C++ program in background
./writer &
CPP_PID=$!

# Wait a moment for shared memory to be created
sleep 2

# Run Python program
python3 reader.py

# Wait for C++ program
wait $CPP_PID
```

---

## How It Works

### 1. **Shared Memory Creation (C++)**
```cpp
int shmid = shmget(SHM_KEY, SHM_SIZE, 0666 | IPC_CREAT);
```
- Creates/accesses shared memory using key `5678`
- Size: 1024 bytes
- Permissions: 0666 (read/write for all)

### 2. **Data Structure**
```cpp
struct SharedData {
    int processId;        // 4 bytes
    char message[900];    // 900 bytes
    bool dataReady;       // 1 byte
};
```

### 3. **Writing Data (C++)**
```cpp
SharedData* sharedData = (SharedData*)shmat(shmid, NULL, 0);
sharedData->processId = getpid();
strcpy(sharedData->message, "Hello from C++!");
sharedData->dataReady = true;
```

### 4. **Reading Data (Python)**
```python
memory = sysv_ipc.SharedMemory(SHM_KEY)
raw_data = memory.read()
# Unpack binary data into structure
```

---

## Expected Output

### C++ Program Output:
```
Shared memory created successfully with ID: XXXXX
Attached to shared memory

=== C++ Program Output ===
Process ID: 12345
Message written to shared memory:
Hello from C++ Process!
This is inter-process communication demonstration.
Data is being shared through shared memory segment.
Process ID: 12345
=========================

Data written to shared memory successfully!
Python program can now read this data.
Press Enter to cleanup and exit...
```

### Python Program Output:
```
=== Python Program - Shared Memory Reader ===

Attempting to connect to shared memory with key: 5678
Successfully connected to shared memory (ID: XXXXX)

Reading data from shared memory...

=== Data Retrieved from Shared Memory ===
Data Ready Flag: True
C++ Process ID: 12345

Message from C++ Program:
--------------------------------------------------
Hello from C++ Process!
This is inter-process communication demonstration.
Data is being shared through shared memory segment.
Process ID: 12345
--------------------------------------------------

=== Python Program Output ===
Successfully read XX characters from shared memory
==================================================
```

---

## Troubleshooting

### Problem: "Shared memory segment not found"
**Solution**: Make sure the C++ program is running or has created the shared memory before running Python program.

### Problem: "Permission denied"
**Solution**: Run with appropriate permissions or check shared memory permissions:
```bash
ipcs -m  # List shared memory segments
```

### Problem: Compilation errors in C++
**Solution**: Ensure you're on a POSIX-compatible system (Linux/Unix/macOS)

### Problem: Python module not found
**Solution**: Install sysv_ipc:
```bash
pip3 install sysv_ipc
```

---

## Cleanup

### View existing shared memory segments:
```bash
ipcs -m
```

### Remove a specific shared memory segment:
```bash
ipcrm -m <shmid>
```

### Remove all shared memory segments (use with caution):
```bash
ipcs -m | awk '/^0x/{print $2}' | xargs -I {} ipcrm -m {}
```

---

## Key Concepts Demonstrated

1. **Shared Memory IPC**: Fast communication between processes
2. **Cross-Language Communication**: C++ and Python working together
3. **Binary Data Handling**: Packing/unpacking structures
4. **Process Synchronization**: Coordination between processes
5. **System V IPC**: Traditional Unix IPC mechanism

---

## Assignment Questions to Consider

1. What happens if Python tries to read before C++ writes?
2. How would you implement synchronization (semaphores)?
3. What are the advantages of shared memory over pipes or sockets?
4. How would you modify this to support bidirectional communication?

---

## Additional Enhancements (Optional)

1. Add semaphore-based synchronization
2. Implement multiple readers/writers
3. Add error recovery mechanisms
4. Create a message queue version for comparison
5. Benchmark performance vs other IPC methods

---

## Files in This Project

- `writer.cpp` - C++ program that writes to shared memory
- `reader.py` - Python program that reads from shared memory
- `README.md` - This documentation file

---

## License
Educational/Academic Use

## Author
IPC Assignment Implementation
