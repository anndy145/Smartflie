#ifndef LLAMAENGINE_H
#define LLAMAENGINE_H

#include "llama.h"
#include <string>
#include <vector>

class LlamaEngine
{
public:
    LlamaEngine();
    ~LlamaEngine();

    bool loadModel(const std::string& modelPath);
    bool isModelLoaded() const { return model != nullptr; }
    std::string generateResponse(const std::string& prompt);
    std::string suggestTags(const std::string& filename, const std::string& content);

private:
    struct llama_model* model = nullptr;
    struct llama_context* ctx = nullptr;
};

#endif // LLAMAENGINE_H
