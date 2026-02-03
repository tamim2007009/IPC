// producer_yolo_queue.cpp
#include <windows.h>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cstdint>

// Define structure WITHOUT #pragma pack to avoid Windows header issues
struct DetectionRecord {
    int class_id;
    float confidence;
    int x;
    int y;
    int width;
    int height;
};

// ---------------- CONFIG ----------------
constexpr int INPUT_WIDTH = 640;
constexpr int INPUT_HEIGHT = 640;
constexpr int CHANNELS = 3;

constexpr int MAX_DETECTIONS = 200;
constexpr int QUEUE_SIZE = 5;

// YOLO parameters
constexpr float CONF_THRESHOLD = 0.25f;
constexpr float NMS_THRESHOLD = 0.45f;

// Names
constexpr wchar_t SHM_NAME[] = L"Local\\YOLO_QUEUE_SHM";
constexpr wchar_t SEM_EMPTY[] = L"Local\\YOLO_EMPTY";
constexpr wchar_t SEM_FULL[] = L"Local\\YOLO_FULL";
constexpr wchar_t MUTEX_NAME[] = L"Local\\YOLO_MUTEX";

// ---------------- SIZE CALC ----------------
constexpr size_t DET_RECORD_SIZE = 24; // int + float + 4*int

constexpr size_t SLOT_SIZE =
5 * sizeof(int) +                              // header
MAX_DETECTIONS * DET_RECORD_SIZE +             // detections
INPUT_WIDTH * INPUT_HEIGHT * CHANNELS;         // image

constexpr size_t SHM_SIZE =
3 * sizeof(int) +                              // write_idx, read_idx, count
QUEUE_SIZE * SLOT_SIZE;

// ---------------- FUNCTION DECLARATIONS ----------------
void write_frame(
    uint8_t* shm,
    HANDLE semEmpty,
    HANDLE semFull,
    HANDLE mutex,
    int frame_id,
    const cv::Mat& frame640,
    const std::vector<int>& class_ids,
    const std::vector<float>& confs,
    const std::vector<cv::Rect>& boxes
);

std::vector<std::string> load_classes(const std::string& path);
cv::Mat letterbox(const cv::Mat& src, float& scale);

// ---------------- LOAD CLASSES ----------------
std::vector<std::string> load_classes(const std::string& path) {
    std::vector<std::string> classes;
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line))
        classes.push_back(line);
    return classes;
}

// ---------------- LETTERBOX RESIZE ----------------
cv::Mat letterbox(const cv::Mat& src, float& scale) {
    int w = src.cols;
    int h = src.rows;

    scale = std::min(
        static_cast<float>(INPUT_WIDTH) / w,
        static_cast<float>(INPUT_HEIGHT) / h
    );

    int new_w = static_cast<int>(w * scale);
    int new_h = static_cast<int>(h * scale);

    cv::Mat resized;
    cv::resize(src, resized, { new_w, new_h });

    cv::Mat output(INPUT_HEIGHT, INPUT_WIDTH, CV_8UC3, cv::Scalar(114, 114, 114));

    resized.copyTo(output(cv::Rect(0, 0, new_w, new_h)));
    return output;
}

// ---------------- WRITE ONE FRAME ----------------
void write_frame(
    uint8_t* shm,
    HANDLE semEmpty,
    HANDLE semFull,
    HANDLE mutex,
    int frame_id,
    const cv::Mat& frame640,
    const std::vector<int>& class_ids,
    const std::vector<float>& confs,
    const std::vector<cv::Rect>& boxes
) {
    WaitForSingleObject(semEmpty, INFINITE); // waits till setEmpty value becomes > 0
    WaitForSingleObject(mutex, INFINITE);   // if mutex is free then lock it to this process for reading and writing

    int* ctrl = reinterpret_cast<int*>(shm);
    int write_idx = ctrl[0];                // the index of the queue we need to write on in logical level

	uint8_t* slot = shm + 3 * sizeof(int) + write_idx * SLOT_SIZE; // calculate physical slot address which is a circular queue 
    uint8_t* p = slot;

    int num = static_cast<int>(boxes.size());
    if (num > MAX_DETECTIONS) num = MAX_DETECTIONS;

    // Header
    memcpy(p, &frame_id, 4); p += 4;
    memcpy(p, &INPUT_WIDTH, 4);  p += 4;
    memcpy(p, &INPUT_HEIGHT, 4);  p += 4;
    memcpy(p, &CHANNELS, 4); p += 4;
    memcpy(p, &num, 4);      p += 4;

    // Detections - write each field separately
    for (int i = 0; i < MAX_DETECTIONS; i++) {
        if (i < num) {
            memcpy(p, &class_ids[i], 4);           p += 4;
            memcpy(p, &confs[i], 4);               p += 4;
            memcpy(p, &boxes[i].x, 4);             p += 4;
            memcpy(p, &boxes[i].y, 4);             p += 4;
            memcpy(p, &boxes[i].width, 4);         p += 4;
            memcpy(p, &boxes[i].height, 4);        p += 4;
        }
        else {
            memset(p, 0, 24);  // 24 bytes per detection record
            p += 24;
        }
    }

    // Image (BGR 640x640)
    size_t image_size = INPUT_WIDTH * INPUT_HEIGHT * CHANNELS;
    if (frame640.total() * frame640.elemSize() == image_size) {
        memcpy(p, frame640.data, image_size);
    }
    else {
        std::cerr << "Error: Image size mismatch!" << std::endl;
        // Write zeros if size doesn't match
        memset(p, 0, image_size);
    }

    // Advance queue
	ctrl[0] = (write_idx + 1) % QUEUE_SIZE; // update write index for next write
	ctrl[2]++;  // update count

    std::cout << "Written frame " << frame_id << " with " << num << " detections" << std::endl;

	ReleaseMutex(mutex);                  // release mutex so that consumer can read
	ReleaseSemaphore(semFull, 1, nullptr);  // increment semFull count by 1 to indicate that a new slot is available fo the consumer to read 
}

// ---------------- MAIN ----------------
int main() {
    // -------- Load classes --------
    auto class_names = load_classes("coco-classes.txt");
    if (class_names.empty()) {
        std::cerr << "ERROR: coco-classes.txt not found\n";
        return -1;
    }
    std::cout << "Loaded " << class_names.size() << " class names" << std::endl;

    // -------- Create shared memory --------
    HANDLE hMap = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        SHM_SIZE,
        SHM_NAME
    );
    if (!hMap) {
        std::cerr << "CreateFileMapping failed\n";
        return -1;
    }

    
    uint8_t* shm = static_cast<uint8_t*>(
        MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE)
        );

    // -------- Create sync objects --------
    HANDLE semEmpty = CreateSemaphore(nullptr, QUEUE_SIZE, QUEUE_SIZE, SEM_EMPTY); // how many slots are empty if > 0 then producer can write
	HANDLE semFull = CreateSemaphore(nullptr, 0, QUEUE_SIZE, SEM_FULL);            // how many slots are full if > 0 then consumer can read
	HANDLE mutex = CreateMutex(nullptr, FALSE, MUTEX_NAME);                        // mutual exclusion ensures that when when consumer or producer is reading/writing the other process can read/write

    // -------- Init control block --------
    int* ctrl = reinterpret_cast<int*>(shm);
    ctrl[0] = 0; // write_idx
    ctrl[1] = 0; // read_idx
    ctrl[2] = 0; // count

    // -------- Open video --------
    cv::VideoCapture cap("video.mp4");
    if (!cap.isOpened()) {
        std::cerr << "ERROR: Cannot open video\n";

        // Try to open webcam as fallback
        cap.open(0);
        if (!cap.isOpened()) {
            std::cerr << "Cannot open video or webcam" << std::endl;
            return -1;
        }
        std::cout << "Using webcam instead of video file" << std::endl;
    }

    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    std::cout << "Video resolution: " << frame_width << "x" << frame_height << ", FPS: " << fps << std::endl;

    // -------- ONNX Runtime --------
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "yolo");
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    try {
        Ort::Session session(env, L"yolov5s.onnx", opts);
        std::cout << "Loaded YOLO model from yolov5s.onnx" << std::endl;

        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name = session.GetInputNameAllocated(0, allocator);
        auto output_name = session.GetOutputNameAllocated(0, allocator);

        const char* input_names[] = { input_name.get() };
        const char* output_names[] = { output_name.get() };

        std::array<int64_t, 4> input_shape{ 1, 3, INPUT_HEIGHT, INPUT_WIDTH };
        Ort::MemoryInfo mem_info =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        // -------- Video loop --------
        cv::Mat frame;
        int frame_id = 0;

        while (cap.read(frame)) {
            frame_id++;

            // Store original frame for transmission
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(INPUT_WIDTH, INPUT_HEIGHT));

            // -------- Preprocess --------
            float scale = 1.0f;
            cv::Mat input = letterbox(frame, scale);

            cv::cvtColor(input, input, cv::COLOR_BGR2RGB);
            input.convertTo(input, CV_32F, 1.0 / 255.0);

            // -------- NCHW tensor --------
            std::vector<float> input_tensor(INPUT_WIDTH * INPUT_HEIGHT * 3);
            int idx = 0;
            for (int c = 0; c < 3; c++)
                for (int y = 0; y < INPUT_HEIGHT; y++)
                    for (int x = 0; x < INPUT_WIDTH; x++)
                        input_tensor[idx++] = input.at<cv::Vec3f>(y, x)[c];

            Ort::Value input_tensor_ort =
                Ort::Value::CreateTensor<float>(
                    mem_info,
                    input_tensor.data(),
                    input_tensor.size(),
                    input_shape.data(),
                    input_shape.size()
                );

            // -------- Inference --------
            auto outputs = session.Run(
                Ort::RunOptions{ nullptr },
                input_names,
                &input_tensor_ort,
                1,
                output_names,
                1
            );

            float* data = outputs[0].GetTensorMutableData<float>();

            std::vector<int> class_ids;
            std::vector<float> confidences;
            std::vector<cv::Rect> boxes;

            for (int i = 0; i < 25200; i++) {
                float obj_conf = data[4];
                if (obj_conf < CONF_THRESHOLD) {
                    data += 85;
                    continue;
                }

                float* scores = data + 5;
                cv::Mat score_mat(1, class_names.size(), CV_32FC1, scores);

                cv::Point class_id;
                double max_score;
                cv::minMaxLoc(score_mat, nullptr, &max_score, nullptr, &class_id);

                float confidence = obj_conf * static_cast<float>(max_score);
                if (confidence >= CONF_THRESHOLD) {
                    float cx = data[0];
                    float cy = data[1];
                    float w = data[2];
                    float h = data[3];

                    // Scale back to original image coordinates
                    int left = static_cast<int>((cx - 0.5f * w) / scale);
                    int top = static_cast<int>((cy - 0.5f * h) / scale);
                    int width = static_cast<int>(w / scale);
                    int height = static_cast<int>(h / scale);

                    // Clip to image boundaries
                    left = std::max(0, std::min(left, frame_width - 1));
                    top = std::max(0, std::min(top, frame_height - 1));
                    width = std::max(1, std::min(width, frame_width - left));
                    height = std::max(1, std::min(height, frame_height - top));

                    class_ids.push_back(class_id.x);
                    confidences.push_back(confidence);
                    boxes.emplace_back(left, top, width, height);
                }

                data += 85;
            }

            // -------- NMS --------
            std::vector<int> indices;
            cv::dnn::NMSBoxes(
                boxes, confidences,
                CONF_THRESHOLD,
                NMS_THRESHOLD,
                indices
            );

            // -------- Keep only NMS results --------
            std::vector<int> filtered_class_ids;
            std::vector<float> filtered_confidences;
            std::vector<cv::Rect> filtered_boxes;

            for (int i : indices) {
                filtered_class_ids.push_back(class_ids[i]);
                filtered_confidences.push_back(confidences[i]);
                filtered_boxes.push_back(boxes[i]);
            }

            // -------- Debug output --------
            std::cout << "Frame " << frame_id << ": Detected " << filtered_boxes.size() << " objects" << std::endl;
            for (size_t i = 0; i < filtered_boxes.size(); ++i) {
                int class_id = filtered_class_ids[i];
                std::string class_name = (class_id < class_names.size()) ?
                    class_names[class_id] : "Unknown";
                std::cout << "  - " << class_name << " (" << filtered_confidences[i]
                    << ") at [" << filtered_boxes[i].x << "," << filtered_boxes[i].y
                    << "," << filtered_boxes[i].width << "," << filtered_boxes[i].height << "]" << std::endl;
            }

            // -------- Scale boxes to 640x640 for transmission --------
            std::vector<cv::Rect> scaled_boxes;
            float scale_x = static_cast<float>(INPUT_WIDTH) / frame_width;
            float scale_y = static_cast<float>(INPUT_HEIGHT) / frame_height;

            for (const auto& box : filtered_boxes) {
                int x = static_cast<int>(box.x * scale_x);
                int y = static_cast<int>(box.y * scale_y);
                int width = static_cast<int>(box.width * scale_x);
                int height = static_cast<int>(box.height * scale_y);

                // Clip to 640x640 boundaries
                x = std::max(0, std::min(x, INPUT_WIDTH - 1));
                y = std::max(0, std::min(y, INPUT_HEIGHT - 1));
                width = std::max(1, std::min(width, INPUT_WIDTH - x));
                height = std::max(1, std::min(height, INPUT_HEIGHT - y));

                scaled_boxes.emplace_back(x, y, width, height);
            }

            // -------- Write to shared memory --------
            write_frame(
                shm, semEmpty, semFull, mutex,
                frame_id, resized, filtered_class_ids, filtered_confidences, scaled_boxes
            );
        }

        std::cout << "Finished processing video" << std::endl;

    }
    catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
        std::cerr << "Please ensure yolov5s.onnx is in the current directory" << std::endl;
        return -1;
    }

    return 0;
}



// code for running inference and displaying on the same process

//#include <onnxruntime_cxx_api.h>
//#include <opencv2/opencv.hpp>
//#include <iostream>
//#include <fstream>
//#include <vector>
//#include <array>
//
//// --------------------
//// Constants
//// --------------------
//constexpr int INPUT_WIDTH = 640;
//constexpr int INPUT_HEIGHT = 640;
//constexpr float CONF_THRESHOLD = 0.25f;
//constexpr float NMS_THRESHOLD = 0.45f;
//
//// --------------------
//// Load class names
//// --------------------
//std::vector<std::string> load_classes(const std::string& path)
//{
//    std::vector<std::string> classes;
//    std::ifstream ifs(path);
//    std::string line;
//    while (std::getline(ifs, line))
//        classes.push_back(line);
//    return classes;
//}
//
//// --------------------
//// Letterbox resize
//// --------------------
//cv::Mat letterbox(const cv::Mat& src, float& scale)
//{
//    int w = src.cols;
//    int h = src.rows;
//
//    scale = std::min(
//        static_cast<float>(INPUT_WIDTH) / w,
//        static_cast<float>(INPUT_HEIGHT) / h
//    );
//
//    int new_w = static_cast<int>(w * scale);
//    int new_h = static_cast<int>(h * scale);
//
//    cv::Mat resized;
//    cv::resize(src, resized, { new_w, new_h });
//
//    cv::Mat output(INPUT_HEIGHT, INPUT_WIDTH, CV_8UC3,
//        cv::Scalar(114, 114, 114));
//
//    resized.copyTo(output(cv::Rect(0, 0, new_w, new_h)));
//    return output;
//}
//
//// --------------------
//// MAIN
//// --------------------
//int main()
//{
//    // -------- Load classes --------
//    auto class_names = load_classes("coco-classes.txt");
//    if (class_names.empty()) {
//        std::cerr << "ERROR: coco-classes.txt not found\n";
//        return -1;
//    }
//
//    // -------- Open video --------
//    cv::VideoCapture cap("video.mp4");
//    if (!cap.isOpened()) {
//        std::cerr << "ERROR: Cannot open video\n";
//        return -1;
//    }
//
//    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
//    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
//    double fps = cap.get(cv::CAP_PROP_FPS);
//
//    // -------- Output video (optional) --------
//    cv::VideoWriter writer(
//        "output.mp4",
//        cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
//        fps,
//        cv::Size(frame_width, frame_height)
//    );
//
//    // -------- ONNX Runtime --------
//    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "yolo");
//    Ort::SessionOptions opts;
//    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
//
//    Ort::Session session(env, L"yolov5s.onnx", opts);
//    Ort::AllocatorWithDefaultOptions allocator;
//
//    auto input_name = session.GetInputNameAllocated(0, allocator);
//    auto output_name = session.GetOutputNameAllocated(0, allocator);
//
//    const char* input_names[] = { input_name.get() };
//    const char* output_names[] = { output_name.get() };
//
//    std::array<int64_t, 4> input_shape{ 1, 3, INPUT_HEIGHT, INPUT_WIDTH };
//    Ort::MemoryInfo mem_info =
//        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
//
//    // -------- Video loop --------
//    cv::Mat frame;
//    while (cap.read(frame))
//    {
//        float scale = 1.0f;
//        cv::Mat input = letterbox(frame, scale);
//
//        cv::cvtColor(input, input, cv::COLOR_BGR2RGB);
//        input.convertTo(input, CV_32F, 1.0 / 255.0);
//
//        // -------- NCHW tensor --------
//        std::vector<float> input_tensor(INPUT_WIDTH * INPUT_HEIGHT * 3);
//        int idx = 0;
//        for (int c = 0; c < 3; c++)
//            for (int y = 0; y < INPUT_HEIGHT; y++)
//                for (int x = 0; x < INPUT_WIDTH; x++)
//                    input_tensor[idx++] =
//                    input.at<cv::Vec3f>(y, x)[c];
//
//        Ort::Value input_tensor_ort =
//            Ort::Value::CreateTensor<float>(
//                mem_info,
//                input_tensor.data(),
//                input_tensor.size(),
//                input_shape.data(),
//                input_shape.size()
//            );
//
//        // -------- Inference --------
//        auto outputs = session.Run(
//            Ort::RunOptions{ nullptr },
//            input_names,
//            &input_tensor_ort,
//            1,
//            output_names,
//            1
//        );
//
//        float* data = outputs[0].GetTensorMutableData<float>();
//
//        std::vector<int> class_ids;
//        std::vector<float> confidences;
//        std::vector<cv::Rect> boxes;
//
//        for (int i = 0; i < 25200; i++)
//        {
//            float obj_conf = data[4];
//            if (obj_conf < CONF_THRESHOLD) {
//                data += 85;
//                continue;
//            }
//
//            float* scores = data + 5;
//            cv::Mat score_mat(1, class_names.size(),
//                CV_32FC1, scores);
//
//            cv::Point class_id;
//            double max_score;
//            cv::minMaxLoc(score_mat, nullptr,
//                &max_score, nullptr, &class_id);
//
//            float confidence = obj_conf * max_score;
//            if (confidence >= CONF_THRESHOLD)
//            {
//                float cx = data[0];
//                float cy = data[1];
//                float w = data[2];
//                float h = data[3];
//
//                int left = int((cx - 0.5f * w) / scale);
//                int top = int((cy - 0.5f * h) / scale);
//                int width = int(w / scale);
//                int height = int(h / scale);
//
//                class_ids.push_back(class_id.x);
//                confidences.push_back(confidence);
//                boxes.emplace_back(left, top, width, height);
//            }
//
//            data += 85;
//        }
//
//        // -------- NMS --------
//        std::vector<int> indices;
//        cv::dnn::NMSBoxes(
//            boxes, confidences,
//            CONF_THRESHOLD,
//            NMS_THRESHOLD,
//            indices
//        );
//
//        // -------- Draw --------
//        for (int i : indices)
//        {
//            cv::Rect box = boxes[i];
//            std::string label =
//                class_names[class_ids[i]] + " " +
//                cv::format("%.2f", confidences[i]);
//
//            cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
//            cv::putText(
//                frame, label,
//                cv::Point(box.x, box.y - 5),
//                cv::FONT_HERSHEY_SIMPLEX,
//                0.5, cv::Scalar(0, 0, 0), 1
//            );
//        }
//
//        writer.write(frame);
//        cv::imshow("YOLOv5 Video", frame);
//
//        if (cv::waitKey(1) == 27)  // ESC
//            break;
//    }
//
//    cap.release();
//    writer.release();
//    cv::destroyAllWindows();
//
//    std::cout << "Video processing finished.\n";
//    return 0;
//}
