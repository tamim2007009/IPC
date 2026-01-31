# Inter-Process Communication (IPC) - Shared Memory Implementation
## Cross-Platform Communication between C++ and Python

---

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [Theory - Shared Memory Concepts](#theory---shared-memory-concepts)
3. [Architecture](#architecture)
4. [Code Explanation](#code-explanation)
5. [Installation & Setup](#installation--setup)
6. [Execution Instructions](#execution-instructions)
7. [Data Structure Details](#data-structure-details)
8. [How It Works](#how-it-works)
9. [Memory Layout](#memory-layout)
10. [Troubleshooting](#troubleshooting)

---

## Project Overview

This project demonstrates **Inter-Process Communication (IPC)** using **shared memory** as the communication mechanism. Two separate processes—a C++ writer and a Python reader—communicate by accessing the same memory region without using traditional IPC methods like pipes, sockets, or message queues.

### Key Features
- **Cross-Platform**: Uses Windows-specific APIs (CreateFileMapping) instead of POSIX APIs
- **Efficient Data Exchange**: Direct memory access without serialization overhead
- **Heterogeneous Languages**: Demonstrates communication between C++ and Python
- **Structured Data**: Shares complex data types with multiple fields and arrays
- **Real-Time Data**: Data updates are immediately visible to the reader process

### Project Components
| Component | Description | Language |
|-----------|-------------|----------|
| `writer.cpp` | Creates shared memory segment and writes data | C++ |
| `reader.py` | Connects to shared memory and reads data | Python |
| `QUICKSTART.md` | Quick setup and execution guide | Markdown |
| `README.md` | This comprehensive documentation | Markdown |

---

## Theory - Shared Memory Concepts

### What is Shared Memory?

Shared memory is an IPC mechanism that allows multiple processes to access the same region of physical memory. Unlike pipes or sockets that require copying data, shared memory provides direct memory access, making it one of the fastest IPC methods.

#### Key Advantages
1. **Performance**: Minimal overhead—direct memory access
2. **Simplicity**: No complex serialization needed
3. **Speed**: Faster than network-based IPC methods
4. **Real-time**: Immediate data visibility across processes

#### Key Disadvantages
1. **Synchronization**: Requires additional mechanisms (mutexes, semaphores)
2. **Complexity**: Memory layout alignment and binary compatibility issues
3. **Security**: Less isolated than other IPC methods
4. **Debugging**: Harder to trace data corruption

### Operating System Implementation

**Windows (This Project)**:
- Uses `CreateFileMapping()` API to create mapped memory regions
- Managed through the kernel's paging system
- Named objects accessible across process boundaries
- Automatic cleanup with handle closure

**Linux/POSIX**:
- Uses `shmget()`, `shmat()`, `shmdt()` system calls
- System V IPC style or POSIX API
- Persistent until explicitly removed
- Requires manual cleanup

### Memory Model

```
Process 1 (C++)         Kernel Space        Process 2 (Python)
┌─────────────────┐                        ┌─────────────────┐
│ Virtual Address │                        │ Virtual Address │
│ Space 1         │                        │ Space 2         │
└────────┬────────┘                        └────────┬────────┘
         │                                          │
         └──────────────────┬──────────────────────┘
                            │
                   ┌────────▼────────┐
                   │ Shared Memory   │
                   │ Physical Memory │
                   └─────────────────┘
```

Both processes map their virtual address spaces to the same physical memory location.

---

## Architecture

### System Architecture

```
┌──────────────────┐              ┌──────────────────┐
│   C++ Writer     │              │  Python Reader   │
│  (writer.cpp)    │              │  (reader.py)     │
│                  │              │                  │
│ • Create SharedData structure   │ • Define SharedData class
│ • Fill with values              │ • Unpack binary data
│ • Write to memory               │ • Display results
│ • Wait for user input           │ • Close connection
└─────────┬────────┘              └────────┬─────────┘
          │                                │
          └────────────────┬───────────────┘
                           │
            ┌──────────────▼──────────────┐
            │  Shared Memory Segment      │
            │  "Local\IPCSharedMemory"    │
            │  Size: 1024 bytes           │
            │                             │
            │  Contains:                  │
            │  • Process ID (4 bytes)     │
            │  • Message (500 bytes)      │
            │  • Data Ready Flag (1 byte) │
            │  • Counter (4 bytes)        │
            │  • Temperature (8 bytes)    │
            │  • Coordinates (12 bytes)   │
            │  • Username (50 bytes)      │
            │  • Timestamp (8 bytes)      │
            │  • Data Array (40 bytes)    │
            └────────────────────────────┘
```

---

## Code Explanation

### C++ Writer (writer.cpp)

#### 1. **Shared Data Structure Definition**
```cpp
struct SharedData {
    int processId;           // 4 bytes - ID of the C++ process
    char message[500];       // 500 bytes - Main message string
    bool dataReady;          // 1 byte - Flag indicating data validity
    int counter;             // 4 bytes - Integer counter
    double temperature;      // 8 bytes - Floating-point temperature
    float coordinates[3];    // 12 bytes - 3D coordinates (X, Y, Z)
    char userName[50];       // 50 bytes - Username string
    long long timestamp;     // 8 bytes - System timestamp
    int dataArray[10];       // 40 bytes - Array of 10 integers
};
```

**Memory Layout Explanation**:
- Total size: ~627 bytes (with alignment padding)
- Fixed offsets for binary compatibility with Python
- Includes various data types to demonstrate mixed-type communication

#### 2. **Creating Shared Memory**
```cpp
HANDLE hMapFile = CreateFileMappingA(
    INVALID_HANDLE_VALUE,    // Use paging file (virtual memory)
    NULL,                    // Default security attributes
    PAGE_READWRITE,          // Read/write access permissions
    0,                       // High-order DWORD of size (unused)
    SHM_SIZE,                // Low-order DWORD of size (1024 bytes)
    SHM_NAME);               // Name: "Local\IPCSharedMemory"
```

**Parameters Explanation**:
- `INVALID_HANDLE_VALUE`: Indicates we're using the system's paging file, not a physical file
- `PAGE_READWRITE`: Both processes can read and write
- `SHM_NAME`: Must be unique and matched with the Python reader

**Return Value**:
- Success: HANDLE to the file mapping object
- Failure: NULL (use GetLastError() for error code)

#### 3. **Mapping Memory to Process Address Space**
```cpp
SharedData* sharedData = (SharedData*)MapViewOfFile(
    hMapFile,                // Handle from CreateFileMapping
    FILE_MAP_ALL_ACCESS,     // Read/write access to mapped region
    0,                       // Offset (high DWORD)
    0,                       // Offset (low DWORD) - start at beginning
    SHM_SIZE);               // Size to map
```

**What Happens**:
- Maps a view (virtual address) in the current process to the shared memory
- Returns a pointer to the mapped memory region
- Allows direct memory access through normal C++ pointers

#### 4. **Writing Data**
```cpp
sharedData->processId = GetCurrentProcessId();
strcpy(sharedData->message, "Hello from C++ Process!\n...");
sharedData->counter = 42;
sharedData->temperature = 23.5;
// ... more data assignments
sharedData->dataReady = true;  // Signal data is ready
```

**Key Points**:
- Direct pointer manipulation writes to shared memory
- Changes are immediately visible to Python process
- `dataReady` flag is set last to ensure all data is written

#### 5. **Cleanup**
```cpp
UnmapViewOfFile(sharedData);  // Unmap the view
CloseHandle(hMapFile);         // Close the handle
```

**Cleanup Process**:
- `UnmapViewOfFile()`: Removes the mapping from current process
- `CloseHandle()`: Closes the file mapping object
- Shared memory persists until all handles are closed

---

### Python Reader (reader.py)

#### 1. **SharedData Class Definition**
```python
class SharedData:
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
```

**Mirrors C++ Structure**:
- Same field names and order as C++ struct
- Python objects initialized to default values
- Will be populated by unpacking binary data

#### 2. **Binary Data Unpacking**
```python
@staticmethod
def unpack_from_bytes(data):
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
```

**Unpacking Process**:
- `struct.unpack()`: Converts binary bytes to Python types
- Format codes: `'i'` = int, `'d'` = double, `'f'` = float, etc.
- `offset`: Tracks current position in binary buffer
- Null-terminated strings: Stop at first \x00 byte

**Format Code Reference**:
| Code | Type | Size |
|------|------|------|
| `i` | Integer | 4 bytes |
| `d` | Double | 8 bytes |
| `f` | Float | 4 bytes |
| `Q` | Unsigned long long | 8 bytes |
| `fff` | 3 Floats | 12 bytes |

#### 3. **Accessing Shared Memory (Windows)**
```python
shm = mmap.mmap(-1, SHM_SIZE, SHM_NAME)
raw_data = shm.read(SHM_SIZE)
shared_data = SharedData.unpack_from_bytes(raw_data)
shm.close()
```

**Memory Mapping on Windows**:
- `mmap.mmap()`: Opens a named memory-mapped object
- `-1`: Indicates use of page file (not a physical file)
- `SHM_NAME`: Must match the C++ program's name
- `shm.read()`: Reads entire shared memory region into bytes
- `shm.close()`: Closes the mapping

**Error Handling**:
```python
except FileNotFoundError:
    print("Shared memory segment not found!")
    print("Please run the C++ program first")
```

---

## Installation & Setup

### Prerequisites

#### Windows System
- Windows 7 or later
- Visual Studio or MinGW GCC compiler
- Python 3.x installed
- Administrator access (not always required but recommended)

#### Required Software
| Component | Minimum Version | Purpose |
|-----------|-----------------|---------|
| GCC/G++ | 5.0 | Compile C++ code |
| Python | 3.6 | Run Python reader |
| pip | Latest | Install Python packages |

### Step 1: Install C++ Compiler

**Option A: MinGW-w64 (Recommended for Windows)**
```bash
# Download from: https://www.mingw-w64.org/
# Or use scoop/chocolatey:
choco install mingw
# or
scoop install gcc
```

**Option B: Visual Studio Build Tools**
```bash
# Install from: https://visualstudio.microsoft.com/downloads/
# Select "Desktop development with C++"
```

**Verify Installation**:
```bash
g++ --version
gcc --version
```

### Step 2: Install Python 3.x

**Option A: Direct Download**
- Visit https://www.python.org/downloads/
- Download Python 3.9 or later
- Run installer with "Add Python to PATH" checked

**Option B: Using Package Manager**
```bash
# Using Chocolatey
choco install python

# Using Scoop
scoop install python
```

**Verify Installation**:
```bash
python --version
python -m pip --version
```

### Step 3: Install Python Dependencies

```bash
# Install sysv_ipc or mmap (mmap is built-in, may need other packages)
pip install --upgrade pip
pip install sysv_ipc  # For POSIX systems (if applicable)

# Note: Windows uses mmap which is part of Python standard library
```

### Step 4: Compile C++ Program

Navigate to project directory:
```bash
cd C:\Users\tamim\Desktop\IPC
```

Compile:
```bash
# Basic compilation
g++ writer.cpp -o writer.exe

# With debugging symbols
g++ -g writer.cpp -o writer.exe

# With optimizations
g++ -O2 writer.cpp -o writer.exe
```

**Output**:
- Creates `writer.exe` executable
- Size typically 100-200 KB depending on compilation flags

---

## Execution Instructions

### Method 1: Manual Sequential Execution (Recommended for Learning)

**Step 1: Terminal 1 - Run C++ Writer**
```bash
cd C:\Users\tamim\Desktop\IPC
.\writer.exe
```

**Expected Output**:
```
Shared memory created successfully
Mapped view of shared memory

=== C++ Program Output ===
Process ID: 12345
Counter: 42
Temperature: 23.5 °C
Coordinates: [10.5, 20.3, 30.8]
Username: WindowsUser
Timestamp: 98765 ms
Data Array: [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]

Message written to shared memory:
--------------------------------------------------
Hello from C++ Process!
This is inter-process communication demonstration.
Data is being shared through shared memory segment.
Process ID: 12345
--------------------------------------------------

Data written to shared memory successfully!
Python program can now read this data.
Press Enter to cleanup and exit...
```

**Step 2: Terminal 2 - Run Python Reader**

While C++ program is running:
```bash
cd C:\Users\tamim\Desktop\IPC
python reader.py
```

**Expected Output**:
```
=== Python Program - Shared Memory Reader ===

Attempting to connect to shared memory: Local\IPCSharedMemory
Successfully connected to shared memory

Reading data from shared memory...

=== Data Retrieved from Shared Memory ===
Data Ready Flag: True
C++ Process ID: 12345
Counter: 42
Temperature: 23.5 °C
Coordinates: [10.5, 20.300000429153442, 30.799999237060547]
Username: WindowsUser
Timestamp: 98765 ms
Data Array: [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]

Message from C++ Program:
--------------------------------------------------
Hello from C++ Process!
This is inter-process communication demonstration.
Data is being shared through shared memory segment.
Process ID: 12345
--------------------------------------------------

=== Python Program Output ===
Successfully read 98 characters from shared memory
Python Process ID: 54321
==================================================

Shared memory connection closed.
```

**Step 3: Return to Terminal 1**

Press Enter in the C++ terminal to cleanup and exit.

---

## Data Structure Details

### Complete Binary Layout

The `SharedData` structure is laid out in memory as follows:

```
Offset  Size  Field              Type        Description
------  ----  -----              ----        -----------
0       4     processId          int         Process ID of writer
4       500   message            char[500]   Main message text
504     1     dataReady          bool        Data validity flag
505     3     [padding]          bytes       Alignment padding
508     4     counter            int         Counter value
512     8     temperature        double      Temperature value
520     12    coordinates        float[3]    X, Y, Z coordinates
532     50    userName           char[50]    Username string
582     6     [padding]          bytes       Alignment padding
588     8     timestamp          long long   System timestamp
596     40    dataArray          int[10]     Array of integers
636     ---   [end of struct]    ---         Total: ~640 bytes
```

### Alignment and Padding

**Why Padding?**
- CPUs work faster with data aligned to natural boundaries
- `int` (4 bytes) should start at 4-byte boundaries
- `double` (8 bytes) should start at 8-byte boundaries
- Compiler adds padding to maintain alignment

**Example**:
```cpp
struct {
    bool b;      // 1 byte  (offset 0)
    // 3 bytes padding (offsets 1-3)
    int i;       // 4 bytes (offset 4) - naturally aligned
}
```

### Platform Considerations

**Windows 32-bit vs 64-bit**:
- Pointer sizes differ, but this struct uses fixed-size types
- Should be binary compatible across platforms
- Long long is 8 bytes on both platforms

**Endianness**:
- This code assumes little-endian (Intel/ARM)
- Big-endian systems (PowerPC, SPARC) need byte swapping
- Use `socket.htons()` / `socket.ntohl()` for portable code

---

## How It Works

### Complete Data Flow

```
1. C++ STARTUP
   ├─ Call CreateFileMapping() with SHM_NAME
   ├─ Kernel creates shared memory object
   ├─ Returns HANDLE to file mapping
   ├─ Call MapViewOfFile() to get pointer
   ├─ Virtual address space mapped to physical memory
   └─ Ready for data writes

2. C++ DATA WRITING
   ├─ Write processId: sharedData->processId = GetCurrentProcessId()
   ├─ Write message: strcpy(sharedData->message, "Hello...")
   ├─ Write numbers: sharedData->temperature = 23.5
   ├─ Write arrays: for loop filling dataArray
   └─ Set flag: sharedData->dataReady = true

3. WAITING & SYNCHRONIZATION
   ├─ C++ waits for user input (cin.get())
   ├─ Python program runs during this wait
   ├─ Both programs access same physical memory
   ├─ No serialization/deserialization needed
   └─ Data transfer is immediate

4. PYTHON STARTUP
   ├─ Call mmap.mmap(-1, SHM_SIZE, SHM_NAME)
   ├─ System finds existing mapping by name
   ├─ Maps same physical memory to Python process
   ├─ Returns file object for memory access
   └─ Ready for data reads

5. PYTHON DATA READING
   ├─ Read raw bytes: shm.read(SHM_SIZE)
   ├─ Parse structure: unpack_from_bytes(raw_data)
   ├─ Extract processId using struct.unpack('i', ...)
   ├─ Extract message using byte slicing and decoding
   ├─ Extract numbers using appropriate format codes
   ├─ Extract arrays using format codes like 'fff' or '10i'
   └─ Display all retrieved values

6. CLEANUP
   ├─ C++ calls UnmapViewOfFile(sharedData)
   ├─ C++ calls CloseHandle(hMapFile)
   ├─ Python calls shm.close()
   ├─ Kernel frees memory resources
   ├─ Shared memory object destroyed
   └─ Both processes exit
```

### Memory Access Pattern

```
Time →

C++ Process          Python Process
┌───────────────┐    
│ CreateMapping │    
└───────┬───────┘    
        │            
   ┌────▼────┐       
   │MapView   │       
   └────┬────┘       
        │            
   ┌────▼──────────┐  
   │Write Data     │  
   └────┬──────────┘  
        │             ┌─────────────────┐
        │             │ Open Mapping    │
        │             └────────┬────────┘
        │                      │
        │             ┌────────▼────────┐
        │             │ Read Raw Bytes  │
        │             └────────┬────────┘
        │                      │
        │             ┌────────▼────────┐
        │             │Unpack & Display │
        │             └────────┬────────┘
        │                      │
        │             ┌────────▼────────┐
        │             │Close Mapping    │
        │             └─────────────────┘
        │
   ┌────▼──────────┐
   │ Wait for Input│
   └────┬──────────┘
        │
   ┌────▼──────────┐
   │ Cleanup       │
   └───────────────┘
```

---

## Memory Layout

### Visualization of Memory in Shared Segment

```
C++ Writes                Python Reads
    ↓                         ↓
┌──────────────────────────────────────────────┐
│ Shared Memory Segment (1024 bytes)           │
├──────────────────────────────────────────────┤
│ [0-3]       : ProcessId        12345         │
├──────────────────────────────────────────────┤
│ [4-503]     : Message          "Hello..."    │
├──────────────────────────────────────────────┤
│ [504]       : DataReady Flag    1 (true)     │
│ [505-507]   : [Padding]        0x00         │
├──────────────────────────────────────────────┤
│ [508-511]   : Counter          42           │
├──────────────────────────────────────────────┤
│ [512-519]   : Temperature      23.5         │
├──────────────────────────────────────────────┤
│ [520-523]   : Coord[0]         10.5         │
│ [524-527]   : Coord[1]         20.3         │
│ [528-531]   : Coord[2]         30.8         │
├──────────────────────────────────────────────┤
│ [532-581]   : Username         "WindowsUser"│
│ [582-587]   : [Padding]        0x00         │
├──────────────────────────────────────────────┤
│ [588-595]   : Timestamp        98765        │
├──────────────────────────────────────────────┤
│ [596-635]   : DataArray[0-9]   [10,20,...] │
├──────────────────────────────────────────────┤
│ [636-1023]  : [Unused space]   0x00         │
└──────────────────────────────────────────────┘
```

### Virtual Address Space Mapping

```
C++ Process Virtual Address Space:
┌─────────────────────┐
│ Stack               │ (High addresses)
├─────────────────────┤
│ Heap                │
├─────────────────────┤
│ Data                │
├─────────────────────┤
│ Text (Code)         │
├─────────────────────┤
│ MMapped Region ◄────┼─────┐
│ (Shared Memory)     │     │
└─────────────────────┘     │
                            │
                Physical Memory (Kernel)
                     ↓
                ┌─────────┐
                │ Shared  │
                │ Memory  │
                │ Buffer  │
                └─────────┘
                     ↑
                     │
┌─────────────────────┘
│
Python Process Virtual Address Space:
│
├─────────────────────┐
│ Stack               │ (High addresses)
├─────────────────────┤
│ Heap                │
├─────────────────────┤
│ Data                │
├─────────────────────┤
│ Text (Code)         │
├─────────────────────┤
│ MMapped Region ◄────┘
│ (Shared Memory)
└─────────────────────┘
```

---

## Troubleshooting

### Common Issues and Solutions

#### 1. "Shared memory segment not found!"
**Cause**: Python reader starts before C++ writer
**Solution**: 
- Start C++ writer first
- Wait until it says "Press Enter to cleanup and exit..."
- Then start Python reader in another terminal

#### 2. Compilation Error: "Cannot find windows.h"
**Cause**: MinGW or Visual Studio not installed
**Solution**:
```bash
# Install MinGW-w64
choco install mingw

# Verify
g++ --version
```

#### 3. Data corruption or garbage values
**Cause**: Struct alignment mismatch
**Solution**:
- Ensure C++ uses same packing as Python assumes
- Use `#pragma pack(1)` if needed for C++
- Verify memory layout matches exactly

#### 4. "Failed to create shared memory. Error: 5"
**Cause**: Permission denied or named object already exists
**Solution**:
- Run as administrator
- Close all running instances and retry
- Restart computer if issue persists

#### 5. Encoding errors when reading message
**Cause**: Non-UTF-8 characters in shared memory
**Solution**:
- Python uses `errors='ignore'` for robustness
- Ensure C++ writes valid UTF-8 text
- Use `encode('utf-8')` explicitly in C++

#### 6. Python ImportError: "No module named 'struct'"
**Cause**: Struct module not in Python installation (very rare)
**Solution**:
- `struct` is built-in; reinstall Python
```bash
python -m pip install --upgrade pip
python -c "import struct; print(struct.__file__)"
```

### Debugging Strategies

#### Strategy 1: Verify Shared Memory Creation
**C++ Side**:
```cpp
if (hMapFile == NULL) {
    std::cerr << "Error: " << GetLastError() << std::endl;
    // Error codes: 5 = access denied, 87 = invalid parameter
}
```

#### Strategy 2: Check Binary Layout
**Python Side**:
```python
# Add after reading raw_data
print(f"Raw data first 20 bytes: {raw_data[:20].hex()}")
print(f"Total bytes read: {len(raw_data)}")
```

#### Strategy 3: Print Offset Positions
```python
# In unpack_from_bytes after each field:
print(f"After processId: offset={offset}")
print(f"ProcessId value: {shared.process_id}")
```

#### Strategy 4: Use System Tools

**Windows - View Shared Memory Objects**:
```powershell
# Using WinObj or Sysinternals tools
wmic os list

# Check process info
tasklist | findstr writer
```

**Python - Inspect Variables**:
```python
import pprint
pprint.pprint(vars(shared_data))
```

---

## Performance Characteristics

### Latency and Throughput

| Metric | Value | Notes |
|--------|-------|-------|
| Memory Copy Time (1KB) | <1 μs | Direct memory access |
| Creation Overhead | ~0.1 ms | One-time cost |
| Read/Write Time | Negligible | Direct pointer access |
| Maximum Throughput | >1 GB/s | Limited by system bus |

### Comparison with Other IPC Methods

| Method | Latency | Throughput | Setup Time |
|--------|---------|-----------|------------|
| Shared Memory | Very Low (<1 μs) | Very High (GB/s) | Low (1-2 ms) |
| Pipes | Low (10-100 μs) | High (10-100 MB/s) | Low |
| Sockets | High (100 μs-1 ms) | Medium (1-100 MB/s) | Medium |
| Message Queue | Medium (10-100 μs) | Medium (10-100 MB/s) | Medium |
| Files | High (1-10 ms) | Variable | High |

---

## Extensions and Improvements

### Possible Enhancements

1. **Add Synchronization**
   - Implement mutexes for thread safety
   - Use events/conditions for handshaking

2. **Bidirectional Communication**
   - Allow Python to write back to C++
   - Implement message passing protocol

3. **Multiple Processes**
   - Support more than 2 communicating processes
   - Ring buffer for multiple readers

4. **Larger Data Transfer**
   - Support variable-sized payloads
   - Implement chunked reading/writing

5. **Error Handling**
   - Add checksums for data integrity
   - Implement recovery mechanisms

6. **Cross-Platform Support**
   - Use POSIX APIs on Linux
   - Conditional compilation with #ifdef

---

## References

### Windows API Documentation
- [CreateFileMapping](https://docs.microsoft.com/windows/win32/api/winbase/nf-winbase-createfilemappinga)
- [MapViewOfFile](https://docs.microsoft.com/windows/win32/api/winbase/nf-winbase-mapviewoffile)
- [UnmapViewOfFile](https://docs.microsoft.com/windows/win32/api/winbase/nf-winbase-unmapviewoffile)

### Python Documentation
- [mmap — Memory-mapped file objects](https://docs.python.org/3/library/mmap.html)
- [struct — Interpret bytes as packed binary data](https://docs.python.org/3/library/struct.html)

### IPC Concepts
- [InterProcess Communication on Wikipedia](https://en.wikipedia.org/wiki/Inter-process_communication)
- [Shared Memory IPC Tutorial](https://www.geeksforgeeks.org/ipc-using-shared-memory/)
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
