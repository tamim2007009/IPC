#include <iostream>
#include <windows.h>
#include <cstring>

#define SHM_SIZE 1024  // Shared memory size
#define SHM_NAME "Local\\IPCSharedMemory"  // Name for shared memory

struct SharedData {
    int processId;
    char message[500];
    bool dataReady;
    int counter;
    double temperature;
    float coordinates[3];
    char userName[50];
    long long timestamp;
    int dataArray[10];
};

int main() {
    // Create shared memory segment
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        SHM_SIZE,                // maximum object size (low-order DWORD)
        SHM_NAME);               // name of mapping object
    
    if (hMapFile == NULL) {
        std::cerr << "Failed to create shared memory. Error: " << GetLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Shared memory created successfully" << std::endl;
    
    // Map view of file
    SharedData* sharedData = (SharedData*)MapViewOfFile(
        hMapFile,                // handle to map object
        FILE_MAP_ALL_ACCESS,     // read/write permission
        0,
        0,
        SHM_SIZE);
    
    if (sharedData == NULL) {
        std::cerr << "Failed to map view of file. Error: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }
    
    std::cout << "Mapped view of shared memory" << std::endl;
    
    // Generate some output data
    sharedData->processId = GetCurrentProcessId();
    strcpy(sharedData->message, 
           "Hello from C++ Process!\n"
           "This is inter-process communication demonstration.\n"
           "Data is being shared through shared memory segment.\n"
           "Process ID: ");
    
    // Append PID to message
    char pidStr[20];
    sprintf(pidStr, "%d", sharedData->processId);
    strcat(sharedData->message, pidStr);
    
    // Write additional data
    sharedData->counter = 42;
    sharedData->temperature = 23.5;
    sharedData->coordinates[0] = 10.5f;
    sharedData->coordinates[1] = 20.3f;
    sharedData->coordinates[2] = 30.8f;
    strcpy(sharedData->userName, "WindowsUser");
    sharedData->timestamp = GetTickCount();
    
    // Fill data array with sample values
    for (int i = 0; i < 10; i++) {
        sharedData->dataArray[i] = (i + 1) * 10;
    }
    
    sharedData->dataReady = true;
    
    std::cout << "\n=== C++ Program Output ===" << std::endl;
    std::cout << "Process ID: " << sharedData->processId << std::endl;
    std::cout << "Counter: " << sharedData->counter << std::endl;
    std::cout << "Temperature: " << sharedData->temperature << " °C" << std::endl;
    std::cout << "Coordinates: [" << sharedData->coordinates[0] << ", " 
              << sharedData->coordinates[1] << ", " << sharedData->coordinates[2] << "]" << std::endl;
    std::cout << "Username: " << sharedData->userName << std::endl;
    std::cout << "Timestamp: " << sharedData->timestamp << " ms" << std::endl;
    std::cout << "Data Array: [";
    for (int i = 0; i < 10; i++) {
        std::cout << sharedData->dataArray[i];
        if (i < 9) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
    std::cout << "\nMessage written to shared memory:" << std::endl;
    std::cout << sharedData->message << std::endl;
    std::cout << "=========================" << std::endl;
    
    std::cout << "\nData written to shared memory successfully!" << std::endl;
    std::cout << "Python program can now read this data." << std::endl;
    std::cout << "Press Enter to cleanup and exit..." << std::endl;
    std::cin.get();
    
    // Unmap view and close handle
    UnmapViewOfFile(sharedData);
    CloseHandle(hMapFile);
    
    std::cout << "C++ program exiting..." << std::endl;
    
    return 0;
}
