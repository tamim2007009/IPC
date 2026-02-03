# consumer_queue.py
import mmap
import struct
import cv2
import numpy as np
import win32event
import win32con
import time

# ---------------- CONFIG ----------------
INPUT_W = 640
INPUT_H = 640
CHANNELS = 3

MAX_DETECTIONS = 200
QUEUE_SIZE = 5

# Shared Memory & Sync Names
SHM_NAME   = "Local\\YOLO_QUEUE_SHM"
SEM_EMPTY  = "Local\\YOLO_EMPTY"
SEM_FULL   = "Local\\YOLO_FULL"
MUTEX_NAME = "Local\\YOLO_MUTEX"

SEMAPHORE_ALL_ACCESS = 0x1F0003 # access level
MUTEX_ALL_ACCESS = 0x1F0001  # access level

# ---------------- SIZE CALCULATIONS ----------------
DET_RECORD_SIZE = 24  # int + float + 4*int = 4 + 4 + 16 = 24 bytes
HEADER_SIZE = 20
CONTROL_SIZE = 12

# Slot Size calculation must match C++ exactly
SLOT_DATA_SIZE = HEADER_SIZE + (MAX_DETECTIONS * DET_RECORD_SIZE) + (INPUT_W * INPUT_H * CHANNELS)
SHM_TOTAL_SIZE = CONTROL_SIZE + (QUEUE_SIZE * SLOT_DATA_SIZE)

# Load COCO class names
def load_coco_classes(filename="coco-classes.txt"):
    classes = []
    try:
        with open(filename, 'r') as f:
            classes = [line.strip() for line in f.readlines()]
        print(f"Loaded {len(classes)} classes from {filename}")
    except FileNotFoundError:
        print(f"Warning: {filename} not found. Using default COCO classes.")
        # Default COCO classes (first 20 for example)
        classes = [
            "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
            "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
            "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
            "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
            "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
            "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
            "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
            "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
            "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
            "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
            "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
            "toothbrush"
        ]
    return classes

# Load classes
CLASSES = load_coco_classes()

# Color palette for different classes
COLORS = np.random.uniform(0, 255, size=(len(CLASSES), 3))

# ---------------- INITIALIZATION ----------------
print("Waiting for Producer...")
while True:
    try:
        shm = mmap.mmap(-1, SHM_TOTAL_SIZE, tagname=SHM_NAME, access=mmap.ACCESS_WRITE)
        semEmpty = win32event.OpenSemaphore(SEMAPHORE_ALL_ACCESS, False, SEM_EMPTY) # how many slots are avaialbe for the producer to write
        semFull  = win32event.OpenSemaphore(SEMAPHORE_ALL_ACCESS, False, SEM_FULL) # how many slots are filled with data for the consumer to read 
        mutex    = win32event.OpenMutex(MUTEX_ALL_ACCESS, False, MUTEX_NAME)
        
        print(f"Connected! SHM Size: {SHM_TOTAL_SIZE} bytes")
        print(f"Detection record size: {DET_RECORD_SIZE} bytes")
        print(f"Slot data size: {SLOT_DATA_SIZE} bytes")
        break
    except Exception as e:
        print(f"Waiting for producer... Error: {e}")
        time.sleep(0.5)

# ---------------- READ FRAME ----------------
def read_frame():
    win32event.WaitForSingleObject(semFull, win32event.INFINITE) # if greater than 0 than proceed, else wait till > 0
    win32event.WaitForSingleObject(mutex, win32event.INFINITE)  # wait till mutex is available

    try:
        # 1. Read Control Block
        shm.seek(0)
        write_idx, read_idx, count = struct.unpack("iii", shm.read(CONTROL_SIZE))

        # 2. Calculate Offset
        slot_start = CONTROL_SIZE + (read_idx * SLOT_DATA_SIZE)
        
        # 3. Read Slot Header
        shm.seek(slot_start)
        frame_id, w, h, c, num = struct.unpack("iiiii", shm.read(HEADER_SIZE))

        if num > MAX_DETECTIONS: 
            num = MAX_DETECTIONS
        
        # 4. Skip to detections section
        det_start = slot_start + HEADER_SIZE
        shm.seek(det_start)
        
        # 5. Read detections
        dets = []
        for i in range(MAX_DETECTIONS):
            data = shm.read(DET_RECORD_SIZE)
            if i < num:
                # Use little-endian to match Windows
                cid, conf, x, y, bw, bh = struct.unpack("<ifiiii", data)
                dets.append((cid, conf, x, y, bw, bh))
        
        # 6. Read Image
        img_offset = det_start + (MAX_DETECTIONS * DET_RECORD_SIZE)
        shm.seek(img_offset)
        
        image_size = INPUT_W * INPUT_H * CHANNELS
        raw_image_data = shm.read(image_size)
        
        if len(raw_image_data) != image_size:
            print(f"Error: Image data size mismatch. Got {len(raw_image_data)}, expected {image_size}")
            img = np.zeros((INPUT_H, INPUT_W, CHANNELS), dtype=np.uint8)
        else:
            img = np.frombuffer(raw_image_data, dtype=np.uint8).reshape((INPUT_H, INPUT_W, CHANNELS)).copy()

        # 7. Update indices
        new_read_idx = (read_idx + 1) % QUEUE_SIZE
        shm.seek(4)
        shm.write(struct.pack("i", new_read_idx))  # the next read index
        shm.seek(8)
        shm.write(struct.pack("i", count - 1))     # decrement count to indicate the no of frames in the buffer that can be consumed

    finally:  
        win32event.ReleaseMutex(mutex)             # release mutex so that producer can write again
        win32event.ReleaseSemaphore(semEmpty, 1)   # indicate that one more slot is available for producer to write

    return img, dets, frame_id

# ---------------- MAIN LOOP ----------------
cv2.namedWindow("YOLO Real-Time Detection", cv2.WINDOW_NORMAL)
cv2.resizeWindow("YOLO Real-Time Detection", 800, 800)

print("Starting Consumer Loop...")
print("Press ESC to exit")

frame_count = 0
fps_start_time = time.time()
fps_frame_count = 0
current_fps = 0

while True:
    try:
        # Calculate FPS
        fps_frame_count += 1
        if fps_frame_count >= 30:
            current_fps = fps_frame_count / (time.time() - fps_start_time)
            fps_start_time = time.time()
            fps_frame_count = 0
        
        # Read frame from shared memory
        frame, dets, fid = read_frame()
        print(f"Read frame ID: {fid} with {len(dets)} detections")
        frame_count += 1
        
        # Convert BGR to RGB for display
        display_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # Draw detections
        for cid, conf, x, y, w, h in dets:
            # Get class name and color
            class_name = CLASSES[cid] if cid < len(CLASSES) else f"Class_{cid}"
            color = COLORS[cid % len(COLORS)]
            
            # Convert color from RGB to BGR for OpenCV
            bgr_color = (int(color[2]), int(color[1]), int(color[0]))
            
            # Draw bounding box
            cv2.rectangle(display_frame, (x, y), (x + w, y + h), bgr_color, 2)
            
            # Create label
            label = f"{class_name}: {conf:.2f}"
            
            # Calculate text size
            (label_width, label_height), baseline = cv2.getTextSize(
                label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1
            )
            
            # Draw label background
            cv2.rectangle(
                display_frame,
                (x, y - label_height - baseline - 5),
                (x + label_width, y),
                bgr_color,
                -1
            )
            
            # Draw label text
            cv2.putText(
                display_frame,
                label,
                (x, y - baseline - 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (255, 255, 255),
                1
            )
        
        # Add info overlay
        cv2.putText(
            display_frame,
            f"Frame: {fid}",
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 255, 0),
            2
        )
        
        cv2.putText(
            display_frame,
            f"FPS: {current_fps:.1f}",
            (10, 60),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 255, 0),
            2
        )
        
        cv2.putText(
            display_frame,
            f"Detections: {len(dets)}",
            (10, 90),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 255, 0),
            2
        )
        
        # Display
        cv2.imshow("YOLO Real-Time Detection", display_frame)
        
        # Print debug info occasionally
        if frame_count % 30 == 0:
            print(f"Processed {frame_count} frames. Last frame: {fid} with {len(dets)} detections")
        
        # Handle key press
        key = cv2.waitKey(1) & 0xFF
        if key == 27:  # ESC
            print("ESC pressed, exiting...")
            break
        elif key == ord(' '):  # Space to pause
            print("Paused. Press any key to continue...")
            cv2.waitKey(0)
            
    except KeyboardInterrupt:
        print("Interrupted by user")
        break
    except Exception as e:
        print(f"Error in main loop: {e}")
        import traceback
        traceback.print_exc()
        break

print("Cleaning up...")
cv2.destroyAllWindows()
try:
    shm.close()
except:
    pass

print(f"Total frames processed: {frame_count}")