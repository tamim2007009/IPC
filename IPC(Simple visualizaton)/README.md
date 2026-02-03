# YOLO Object Detection with Inter-Process Communication (IPC)

This project demonstrates **real-time object detection** using YOLOv5 with **Inter-Process Communication (IPC)** between a C++ Producer and a Python Consumer on Windows.

## 📋 Project Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SYSTEM ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────┐                        ┌─────────────────┐       │
│   │   PRODUCER      │                        │   CONSUMER      │       │
│   │   (C++)         │                        │   (Python)      │       │
│   │                 │                        │                 │       │
│   │ • Video Input   │                        │ • Read Frames   │       │
│   │ • YOLO Inference│──── Shared Memory ────▶│ • Draw Boxes    │       │
│   │ • Detection     │    (Windows mmap)      │ • Display       │       │
│   │ • Write Frames  │                        │                 │       │
│   └─────────────────┘                        └─────────────────┘       │
│                                                                         │
│                    Synchronization via:                                 │
│                    • Semaphores (Empty/Full)                            │
│                    • Mutex (Mutual Exclusion)                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 🔧 Components

### Producer (C++ - `producer/test.cpp`)

- Reads video frames from `video.mp4` (or webcam fallback)
- Runs YOLOv5 inference using ONNX Runtime
- Writes detection results + frame images to shared memory
- Libraries: OpenCV, ONNX Runtime

### Consumer (Python - `consumer/consumer_shm.py`)

- Reads frames and detections from shared memory
- Draws bounding boxes with class labels
- Displays real-time detection visualization
- Libraries: OpenCV, NumPy, pywin32

---

## 🔄 Inter-Process Communication (IPC) Mechanism

### 1. Shared Memory Architecture

The system uses **Windows Named Shared Memory** for fast, low-latency data transfer between processes.

#### Shared Memory Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SHARED MEMORY STRUCTURE                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  CONTROL BLOCK (12 bytes)                                              │
│  ┌─────────────┬─────────────┬─────────────┐                           │
│  │ write_idx   │ read_idx    │ count       │                           │
│  │ (4 bytes)   │ (4 bytes)   │ (4 bytes)   │                           │
│  └─────────────┴─────────────┴─────────────┘                           │
│                                                                         │
│  CIRCULAR QUEUE (5 Slots)                                              │
│  ┌─────────┬─────────┬─────────┬─────────┬─────────┐                   │
│  │ Slot 0  │ Slot 1  │ Slot 2  │ Slot 3  │ Slot 4  │                   │
│  └─────────┴─────────┴─────────┴─────────┴─────────┘                   │
│                                                                         │
│  Each Slot Contains:                                                    │
│  ┌──────────────────────────────────────────────────┐                  │
│  │ HEADER (20 bytes)                                │                  │
│  │ • frame_id (4 bytes)                             │                  │
│  │ • width (4 bytes) - always 640                   │                  │
│  │ • height (4 bytes) - always 640                  │                  │
│  │ • channels (4 bytes) - always 3                  │                  │
│  │ • num_detections (4 bytes)                       │                  │
│  ├──────────────────────────────────────────────────┤                  │
│  │ DETECTIONS (200 × 24 bytes = 4,800 bytes)        │                  │
│  │ Each detection:                                  │                  │
│  │ • class_id (4 bytes, int)                        │                  │
│  │ • confidence (4 bytes, float)                    │                  │
│  │ • x, y, width, height (16 bytes, 4 ints)         │                  │
│  ├──────────────────────────────────────────────────┤                  │
│  │ IMAGE DATA (640 × 640 × 3 = 1,228,800 bytes)     │                  │
│  │ • BGR format, uint8 pixels                       │                  │
│  └──────────────────────────────────────────────────┘                  │
│                                                                         │
│  Total Slot Size: 20 + 4,800 + 1,228,800 = 1,233,620 bytes            │
│  Total SHM Size: 12 + (5 × 1,233,620) = 6,168,112 bytes (~5.88 MB)    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2. Synchronization Primitives

The system uses three Windows synchronization objects:

| Object                | Name               | Purpose                                           |
| --------------------- | ------------------ | ------------------------------------------------- |
| **Semaphore (Empty)** | `Local\YOLO_EMPTY` | Tracks available slots for writing (initial: 5)   |
| **Semaphore (Full)**  | `Local\YOLO_FULL`  | Tracks slots with data ready to read (initial: 0) |
| **Mutex**             | `Local\YOLO_MUTEX` | Ensures exclusive access during read/write        |

### 3. Producer-Consumer Pattern with Circular Queue

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CIRCULAR BUFFER OPERATION                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Initial State:                                                         │
│  ┌───┬───┬───┬───┬───┐                                                 │
│  │   │   │   │   │   │  write_idx = 0, read_idx = 0, count = 0         │
│  └───┴───┴───┴───┴───┘                                                 │
│    ▲                                                                    │
│    └── Both pointers start here                                         │
│                                                                         │
│  After Producer writes 3 frames:                                        │
│  ┌───┬───┬───┬───┬───┐                                                 │
│  │ F1│ F2│ F3│   │   │  write_idx = 3, read_idx = 0, count = 3         │
│  └───┴───┴───┴───┴───┘                                                 │
│    ▲           ▲                                                        │
│    │           └── write_idx (next write position)                      │
│    └── read_idx (next read position)                                    │
│                                                                         │
│  After Consumer reads 2 frames:                                         │
│  ┌───┬───┬───┬───┬───┐                                                 │
│  │   │   │ F3│   │   │  write_idx = 3, read_idx = 2, count = 1         │
│  └───┴───┴───┴───┴───┘                                                 │
│            ▲   ▲                                                        │
│            │   └── write_idx                                            │
│            └── read_idx                                                 │
│                                                                         │
│  Wraparound Example:                                                    │
│  ┌───┬───┬───┬───┬───┐                                                 │
│  │ F8│   │   │ F6│ F7│  write_idx = 1, read_idx = 3, count = 3         │
│  └───┴───┴───┴───┴───┘                                                 │
│    ▲           ▲                                                        │
│    │           └── read_idx                                             │
│    └── write_idx (wrapped around)                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📝 Detailed IPC Flow

### Producer Write Operation (C++)

```cpp
void write_frame(...) {
    // Step 1: Wait for empty slot
    WaitForSingleObject(semEmpty, INFINITE);  // Decrements semEmpty

    // Step 2: Acquire exclusive access
    WaitForSingleObject(mutex, INFINITE);     // Lock mutex

    // Step 3: Get write position
    int write_idx = ctrl[0];
    uint8_t* slot = shm + 12 + write_idx * SLOT_SIZE;

    // Step 4: Write data
    // - Header (frame_id, width, height, channels, num_detections)
    // - Detection records (class_id, confidence, bbox)
    // - Image data (BGR pixels)

    // Step 5: Update control block
    ctrl[0] = (write_idx + 1) % QUEUE_SIZE;   // Advance write pointer
    ctrl[2]++;                                 // Increment count

    // Step 6: Release resources
    ReleaseMutex(mutex);                       // Unlock mutex
    ReleaseSemaphore(semFull, 1, nullptr);     // Signal data available
}
```

### Consumer Read Operation (Python)

```python
def read_frame():
    # Step 1: Wait for data
    win32event.WaitForSingleObject(semFull, INFINITE)   # Decrements semFull

    # Step 2: Acquire exclusive access
    win32event.WaitForSingleObject(mutex, INFINITE)     # Lock mutex

    # Step 3: Get read position
    write_idx, read_idx, count = struct.unpack("iii", shm.read(12))
    slot_start = 12 + (read_idx * SLOT_SIZE)

    # Step 4: Read data
    # - Header
    # - Detections
    # - Image

    # Step 5: Update control block
    new_read_idx = (read_idx + 1) % QUEUE_SIZE   # Advance read pointer
    count -= 1                                     # Decrement count

    # Step 6: Release resources
    win32event.ReleaseMutex(mutex)                 # Unlock mutex
    win32event.ReleaseSemaphore(semEmpty, 1)       # Signal slot free

    return img, detections, frame_id
```

---

## 🔒 Synchronization Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     SYNCHRONIZATION TIMELINE                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  PRODUCER                           CONSUMER                            │
│  ════════                           ════════                            │
│                                                                         │
│  ┌─────────────────┐                                                   │
│  │ Wait(semEmpty)  │◄─── Has empty slots? (semEmpty > 0)               │
│  └────────┬────────┘                                                   │
│           │ Yes                                                         │
│           ▼                                                             │
│  ┌─────────────────┐                                                   │
│  │ Wait(mutex)     │◄─── Acquire lock                                  │
│  └────────┬────────┘                                                   │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────┐                                                   │
│  │ WRITE DATA      │                                                   │
│  │ to shared mem   │                                                   │
│  └────────┬────────┘                                                   │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────┐                                                   │
│  │ Release(mutex)  │──── Release lock                                  │
│  └────────┬────────┘                                                   │
│           │                                                             │
│           ▼                                                             │
│  ┌─────────────────┐                ┌─────────────────┐                │
│  │Signal(semFull)  │───────────────▶│ Wait(semFull)   │                │
│  └─────────────────┘  "Data ready!" └────────┬────────┘                │
│                                              │                          │
│                                              ▼                          │
│                                     ┌─────────────────┐                │
│                                     │ Wait(mutex)     │                │
│                                     └────────┬────────┘                │
│                                              │                          │
│                                              ▼                          │
│                                     ┌─────────────────┐                │
│                                     │ READ DATA       │                │
│                                     │ from shared mem │                │
│                                     └────────┬────────┘                │
│                                              │                          │
│                                              ▼                          │
│                                     ┌─────────────────┐                │
│                                     │ Release(mutex)  │                │
│                                     └────────┬────────┘                │
│                                              │                          │
│  ┌─────────────────┐                         ▼                          │
│  │ Wait(semEmpty)  │◄───────────────Signal(semEmpty)                   │
│  └─────────────────┘  "Slot free!"  └─────────────────┘                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Running the Project

### Prerequisites

#### Producer (C++)

- Visual Studio 2019/2022
- OpenCV 4.x
- ONNX Runtime
- YOLOv5s ONNX model (`yolov5s.onnx`)

#### Consumer (Python)

```bash
pip install -r requirements.txt
```

Key dependencies:

- `opencv-python`
- `numpy`
- `pywin32`

### Execution Steps

1. **Start the Producer first** (creates shared memory):

   ```bash
   cd producer
   # Build and run the Visual Studio project
   # Or run: assignment_cpp.exe
   ```

2. **Start the Consumer** (connects to shared memory):

   ```bash
   cd consumer
   python consumer_shm.py
   ```

3. **Controls**:
   - `ESC` - Exit
   - `SPACE` - Pause/Resume

---

## 📊 Configuration Constants

| Parameter        | Value | Description              |
| ---------------- | ----- | ------------------------ |
| `INPUT_WIDTH`    | 640   | Frame width              |
| `INPUT_HEIGHT`   | 640   | Frame height             |
| `CHANNELS`       | 3     | BGR color channels       |
| `MAX_DETECTIONS` | 200   | Max detections per frame |
| `QUEUE_SIZE`     | 5     | Circular buffer slots    |
| `CONF_THRESHOLD` | 0.25  | Confidence threshold     |
| `NMS_THRESHOLD`  | 0.45  | NMS IoU threshold        |

---

## 🎯 Why This IPC Approach?

### Advantages

| Feature            | Benefit                                             |
| ------------------ | --------------------------------------------------- |
| **Shared Memory**  | Zero-copy data transfer, minimal latency            |
| **Circular Queue** | Handles speed differences between producer/consumer |
| **Semaphores**     | Prevents buffer overflow/underflow                  |
| **Mutex**          | Ensures data integrity during concurrent access     |
| **Cross-Language** | C++ performance + Python flexibility                |

### Trade-offs

| Consideration         | Detail                                 |
| --------------------- | -------------------------------------- |
| **Windows-Specific**  | Uses Win32 API (not portable to Linux) |
| **Fixed Buffer Size** | Memory pre-allocated (~6 MB)           |
| **Local Only**        | Single machine communication           |

---

## 📁 File Structure

```
project/
├── producer/                      # C++ Producer
│   ├── test.cpp                   # Main producer code
│   ├── yolov5s.onnx              # YOLO model
│   ├── coco-classes.txt          # Class labels
│   ├── video.mp4                 # Input video
│   └── assignment_cpp.vcxproj    # VS project
│
├── consumer/                      # Python Consumer
│   ├── consumer_shm.py           # Main consumer code
│   ├── coco-classes.txt          # Class labels
│   └── requirements.txt          # Python dependencies
│
└── README.md                     # This file
```

---

## 🔍 Troubleshooting

| Issue                  | Solution                                           |
| ---------------------- | -------------------------------------------------- |
| Consumer can't connect | Ensure producer is running first                   |
| No detections shown    | Check `yolov5s.onnx` exists in producer folder     |
| Video not found        | Place `video.mp4` in producer folder or use webcam |
| Permission errors      | Run as administrator                               |
| Memory errors          | Ensure both apps use same size constants           |

---

## 📚 References

- [Windows Shared Memory (MSDN)](https://docs.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory)
- [Semaphores (MSDN)](https://docs.microsoft.com/en-us/windows/win32/sync/using-semaphore-objects)
- [YOLOv5](https://github.com/ultralytics/yolov5)
- [ONNX Runtime](https://onnxruntime.ai/)
