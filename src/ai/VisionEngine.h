#ifndef VISIONENGINE_H
#define VISIONENGINE_H

#include <string>
#include <vector>
#include <memory> 

#ifdef ENABLE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

class VisionEngine
{
public:
    VisionEngine();
    ~VisionEngine();

    bool initialize(const std::string& modelPath, int numThreads = 4);
    std::vector<float> extractFeatures(const std::string& imagePath);

    bool isAvailable() const;
    bool isLoaded() const;

private:
#ifdef ENABLE_ONNX
    // ONNX Runtime internal state
    std::shared_ptr<Ort::Env> env;
    std::shared_ptr<Ort::Session> session;
    std::shared_ptr<Ort::MemoryInfo> memoryInfo;
#endif
};

#endif // VISIONENGINE_H
