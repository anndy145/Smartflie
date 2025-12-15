#include "VisionEngine.h"
#include <iostream>
#include <vector>

#ifdef ENABLE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#endif

#ifdef ENABLE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

VisionEngine::VisionEngine()
{
}

VisionEngine::~VisionEngine()
{
}

bool VisionEngine::initialize(const std::string& modelPath, int numThreads)
{
#if defined(ENABLE_OPENCV) && defined(ENABLE_ONNX)
    try {
        std::cout << "[VisionEngine] Initializing with model: " << modelPath << std::endl;
        
        // 1. Initialize Environment
        env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "SmartFileVision");
        
        // 2. Session Options
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(numThreads);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        // 3. Load Model
        // Convert string to wstring for Windows path
        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        session = std::make_shared<Ort::Session>(*env, wModelPath.c_str(), sessionOptions);

        // 4. Memory Info (CPU)
        memoryInfo = std::make_shared<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        std::cout << "[VisionEngine] Model loaded successfully!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[VisionEngine] Failed to initialize model: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[VisionEngine] Unknown error loading model." << std::endl;
        return false;
    }
#else
    std::cout << "[VisionEngine] Vision features disabled (Missing libs)." << std::endl;
    return false;
#endif
}

#include <filesystem>
#include <fstream>

std::vector<float> VisionEngine::extractFeatures(const std::string& imagePath)
{
    std::vector<float> features;
#if defined(ENABLE_OPENCV) && defined(ENABLE_ONNX)
    if (!session) return features;

    try {
        // 1. Load Image (Unicode Safe)
        // Read file into buffer first to avoid OpenCV Windows path encoding issues
        std::filesystem::path path(imagePath);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
             // Try standard legacy open if filesystem fails (fallback)
             file.open(imagePath, std::ios::binary | std::ios::ate);
        }
        
        if (!file.is_open()) {
            std::cerr << "[VisionEngine] Failed to open file: " << imagePath << std::endl;
            return features;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (file.read(buffer.data(), size)) {
            cv::Mat rawData(1, size, CV_8UC1, (void*)buffer.data());
            cv::Mat img = cv::imdecode(rawData, cv::IMREAD_COLOR);
            
            if (img.empty()) {
                std::cerr << "[VisionEngine] Failed to decode image: " << imagePath << std::endl;
                return features;
            }
            
            // Proceed with img...
            // 2. Preprocess for CLIP (224x224, Normalize)
            // CLIP Standard Mean/Std: [0.48145466, 0.4578275, 0.40821073], [0.26862954, 0.26130258, 0.27577711]
            cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
            cv::resize(img, img, cv::Size(224, 224));
            
            img.convertTo(img, CV_32F, 1.0 / 255.0); // Scale to 0-1

            // Normalize manually (OpenCV BlobFromImage can do this but let's be explicit for CLIP)
            cv::Mat mean(224, 224, CV_32FC3, cv::Scalar(0.48145466, 0.4578275, 0.40821073));
            cv::Mat std(224, 224, CV_32FC3, cv::Scalar(0.26862954, 0.26130258, 0.27577711));
            cv::subtract(img, mean, img);
            cv::divide(img, std, img);

            // CHW Conversion (HWC -> CHW)
            cv::Mat chw_img;
            cv::dnn::blobFromImage(img, chw_img); 

            // 3. Create Tensor
            std::vector<int64_t> inputShape = {1, 3, 224, 224};
            size_t inputTensorSize = 1 * 3 * 224 * 224;
            
            if (!chw_img.isContinuous()) chw_img = chw_img.clone();
            
            std::vector<float> inputTensorValues(inputTensorSize);
            memcpy(inputTensorValues.data(), chw_img.ptr<float>(), inputTensorSize * sizeof(float));

            auto inputTensor = Ort::Value::CreateTensor<float>(
                *memoryInfo, inputTensorValues.data(), inputTensorSize, inputShape.data(), inputShape.size()
            );

            // 4. Run Inference with Fallback for Node Names
            std::vector<std::pair<const char*, const char*>> namePairs = {
                {"pixel_values", "pooler_output"}, // Xenova Standard
                {"pixel_values", "last_hidden_state"}, // Xenova Alt
                {"pixel_values", "image_embeds"},  // CLIP Standard
                {"input", "output"}                // Generic/Legacy
            };

            for (const auto& pair : namePairs) {
                try {
                    const char* inputNames[] = {pair.first}; 
                    const char* outputNames[] = {pair.second}; 

                    auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
                    
                    // IF success:
                    float* floatarr = outputTensors.front().GetTensorMutableData<float>();
                    size_t outputCount = outputTensors.front().GetTensorTypeAndShapeInfo().GetElementCount();
                    
                    features.assign(floatarr, floatarr + outputCount);
                    return features; // Success, return immediately
                } catch (...) {
                    // Continue to next pair
                }
            }
            std::cerr << "[VisionEngine] Failed to run inference with known node names." << std::endl;
            return features; // Empty if all failed
        } else {
             std::cerr << "[VisionEngine] Failed to read file content." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[VisionEngine] Error: " << e.what() << std::endl;
    }
#endif
    return features;
}

bool VisionEngine::isAvailable() const
{
#if defined(ENABLE_OPENCV) && defined(ENABLE_ONNX)
    return true;
#else
    return false;
#endif
}

bool VisionEngine::isLoaded() const
{
#if defined(ENABLE_OPENCV) && defined(ENABLE_ONNX)
    return (session != nullptr);
#else
    return false;
#endif
}
