# QUICK START GUIDE - IPC Assignment

## 🚀 Fastest Way to Run

### Option 1: Automated Test (Recommended)
```bash
./run_test.sh
```

### Option 2: Using Makefile
```bash
# Install dependencies and compile
make install-deps
make compile

# Run C++ program (Terminal 1)
make run-cpp

# Run Python program (Terminal 2)
make run-python
```

### Option 3: Manual Execution
```bash
# 1. Install dependencies
pip3 install sysv_ipc

# 2. Compile C++
g++ writer.cpp -o writer

# 3. Terminal 1 - Run C++
./writer

# 4. Terminal 2 - Run Python
python3 reader.py
```

---

## 📁 Files in This Project

| File | Description |
|------|-------------|
| `writer.cpp` | C++ program that writes to shared memory |
| `reader.py` | Python program that reads from shared memory |
| `README.md` | Complete documentation |
| `Makefile` | Build automation |
| `run_test.sh` | Automated test script |
| `QUICKSTART.md` | This file |

---

## 🔧 Common Commands

```bash
# Compile C++
make compile

# Clean everything
make distclean

# Show shared memory status
make show-shm

# Get help
make help
```

---

## ✅ What You Should See

**C++ Output:**
```
Shared memory created successfully with ID: 12345
Attached to shared memory

=== C++ Program Output ===
Process ID: 67890
Message written to shared memory:
Hello from C++ Process!
...
```

**Python Output:**
```
=== Python Program - Shared Memory Reader ===

Successfully connected to shared memory (ID: 12345)

=== Data Retrieved from Shared Memory ===
Data Ready Flag: True
C++ Process ID: 67890
...
```

---

## 🐛 Troubleshooting

**"Shared memory not found"**
→ Run C++ program first

**"sysv_ipc module not found"**
→ Run: `pip3 install sysv_ipc`

**"Permission denied"**
→ Check file permissions: `chmod +x run_test.sh reader.py`

---

## 📚 Learn More

See `README.md` for:
- Detailed explanations
- How it works
- Advanced concepts
- Assignment questions

---

## 🎯 Assignment Checklist

- [ ] C++ program creates shared memory
- [ ] C++ program writes data to shared memory
- [ ] Python program reads from shared memory
- [ ] Python program displays the data
- [ ] Both programs work together successfully
- [ ] Code is well-commented
- [ ] Documentation is complete

---

**Good luck with your assignment! 🎓**
