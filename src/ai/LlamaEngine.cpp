#include "LlamaEngine.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>

// Helper to add token to batch
static void batch_add(llama_batch & batch, llama_token id, llama_pos pos, const std::vector<llama_seq_id> & seq_ids, bool logits) {
    batch.token   [batch.n_tokens] = id;
    batch.pos     [batch.n_tokens] = pos;
    batch.n_seq_id[batch.n_tokens] = seq_ids.size();
    for (size_t i = 0; i < seq_ids.size(); ++i) {
        batch.seq_id[batch.n_tokens][i] = seq_ids[i];
    }
    batch.logits  [batch.n_tokens] = logits;
    batch.n_tokens++;
}

LlamaEngine::LlamaEngine()
{
    llama_backend_init();
}

LlamaEngine::~LlamaEngine()
{
    if (ctx) llama_free(ctx);
    if (model) llama_model_free(model);
    llama_backend_free();
}

bool LlamaEngine::loadModel(const std::string& modelPath)
{
    if (model) {
        llama_model_free(model);
        model = nullptr;
    }
    if (ctx) {
        llama_free(ctx);
        ctx = nullptr;
    }

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 100; // Try to use GPU
    model = llama_model_load_from_file(modelPath.c_str(), model_params);

    if (!model) {
        std::cerr << "Failed to load model from " << modelPath << std::endl;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048; 
    ctx = llama_init_from_model(model, ctx_params);

    if (!ctx) {
        std::cerr << "Failed to create context" << std::endl;
        return false;
    }

    return true;
}

std::string LlamaEngine::generateResponse(const std::string& prompt)
{
    if (!ctx || !model) return "Error: Model not loaded";

    // Clear KV cache
    llama_memory_t mem = llama_get_memory(ctx);
    llama_memory_seq_rm(mem, -1, -1, -1);

    const llama_vocab* vocab = llama_model_get_vocab(model);

    // 1. Tokenize
    const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.length(), NULL, 0, true, false);
    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(vocab, prompt.c_str(), prompt.length(), prompt_tokens.data(), n_prompt, true, false) < 0) {
        return "Error: Tokenization failed";
    }

    // 2. Initial Batch
    llama_batch batch = llama_batch_init(2048, 0, 1);
    for(int i=0; i<n_prompt; i++) {
        batch_add(batch, prompt_tokens[i], i, {0}, false);
    }
    batch.logits[batch.n_tokens - 1] = true;

    // 3. Decode
    if (llama_decode(ctx, batch) != 0) {
         llama_batch_free(batch);
        return "Error: llama_decode failed";
    }
    
    int n_curr = batch.n_tokens; 
    llama_batch_free(batch); 

    // 4. Sample loop
    std::stringstream response_ss;
    int n_predict = 128;
    
    auto sparams = llama_sampler_chain_default_params();
    struct llama_sampler * smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    llama_token new_token_id = 0;

    for (int i = 0; i < n_predict; ++i) {
        new_token_id = llama_sampler_sample(smpl, ctx, -1);

        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        char buf[256];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n >= 0) {
            std::string piece(buf, n);
            response_ss << piece;
        }

        llama_batch batch_one = llama_batch_init(1, 0, 1);
        batch_add(batch_one, new_token_id, n_curr, {0}, true);
        n_curr++;

        if (llama_decode(ctx, batch_one) != 0) {
            llama_batch_free(batch_one);
            break;
        }
        llama_batch_free(batch_one);
    }
    
    llama_sampler_free(smpl);

    return response_ss.str();
}

std::string LlamaEngine::suggestTags(const std::string& filename, const std::string& content)
{
    std::string prompt;
    if (content.empty()) {
        prompt = "System: You are a file tagging assistant. Your goal is to provide concise tags.\n"
                 "User: Analyze the filename '" + filename + "' and suggest relevant keywords or tags.\n"
                 "Rules:\n"
                 "1. Suggest up to 5 tags.\n"
                 "2. Use TRADITIONAL CHINESE (繁體中文) for general terms. Keep proper nouns in English.\n"
                 "3. Tags must be short.\n"
                 "4. Output ONLY a comma-separated list.\n"
                 "Assistant: Tags:";
    } else {
        std::string safeContent = content.substr(0, 1000); 
        prompt = "System: You are a file tagging assistant. Your goal is to provide concise tags.\n"
                 "User: Analyze the filename '" + filename + "' and the following file content preview:\n\n" + safeContent + "\n\n"
                 "Rules:\n"
                 "1. Suggest up to 5 relevant keywords/tags.\n"
                 "2. Use TRADITIONAL CHINESE (繁體中文) for general terms.\n"
                 "3. Tags must be SHORT.\n"
                 "4. Output ONLY a comma-separated list.\n"
                 "Assistant: Tags:";
    }
    
    return generateResponse(prompt);
}
