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
    if (ctx) {
        llama_free(ctx);
        ctx = nullptr;
    }
    if (model) {
        llama_model_free(model);
        model = nullptr;
    }

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 100; // Try to use GPU
    model = llama_model_load_from_file(modelPath.c_str(), model_params);

    if (!model) {
        std::cerr << "Failed to load model from " << modelPath << std::endl;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 32768; // Support up to 32k context (Qwen standard)
    ctx_params.n_batch = 2048; // Batch size for processing
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
    const int n_prompt = -llama_tokenize(vocab, prompt.c_str(), prompt.length(), NULL, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(vocab, prompt.c_str(), prompt.length(), prompt_tokens.data(), n_prompt, true, true) < 0) {
        return "Error: Tokenization failed";
    }

    // Check context limit
    if (n_prompt >= llama_n_ctx(ctx)) {
        return "Error: Prompt too long for context window";
    }

    // 2. Decode Prompt in Chunks
    int n_batch = llama_n_batch(ctx);
    llama_batch batch = llama_batch_init(n_batch, 0, 1); 

    int processed = 0;
    while (processed < n_prompt) {
        int n_chunk = n_prompt - processed;
        if (n_chunk > n_batch) n_chunk = n_batch;
        
        // Reset batch for this chunk
        batch.n_tokens = 0;
        
        for (int i = 0; i < n_chunk; ++i) {
            batch_add(batch, prompt_tokens[processed + i], processed + i, {0}, false);
        }
        processed += n_chunk;
        
        // Logical update: set logits only for the very last token of the entire prompt
        if (processed == n_prompt) {
            batch.logits[batch.n_tokens - 1] = true;
        }

        if (llama_decode(ctx, batch) != 0) {
            llama_batch_free(batch);
            return "Error: llama_decode failed during prompt processing";
        }
    }
    
    int n_curr = batch.n_tokens + (processed - batch.n_tokens); // Logic check: n_curr should be n_prompt
    n_curr = n_prompt; // Force correct pos
    
    llama_batch_free(batch); 

    // 4. Sample loop
    std::stringstream response_ss;
    int n_predict = 256; 
    
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
    // Qwen / ChatML Format
    // Format: <|im_start|>system\n...\n<|im_end|>\n<|im_start|>user\n...\n<|im_end|>\n<|im_start|>assistant\n
    
    // Increase limit to 16000 chars (approx fits in 8k context)
    std::string safeContent = content.empty() ? "(No content)" : content.substr(0, 16000);
    
    std::string prompt = 
        "<|im_start|>system\n"
        "You are a strict file tagging assistant. Your ONLY job is to output a comma-separated list of tags in Traditional Chinese (繁體中文).\n"
        "Rules:\n"
        "1. Output ONLY the tags. No introductory text. No explanations.\n"
        "2. Suggest exactly 3-5 tags.\n"
        "3. Tags must be concise (max 4 words).\n"
        "4. Do NOT output full sentences.\n"
        "Example Input:\n"
        "Filename: report.pdf\n"
        "Content: Q3 Financial Summary...\n"
        "Example Output:\n"
        "財務報告, 第三季, 業績\n"
        "<|im_end|>\n"
        "<|im_start|>user\n"
        "Filename: " + filename + "\n"
        "Content Preview: " + safeContent + "\n"
        "<|im_end|>\n"
        "<|im_start|>assistant\n";

    return generateResponse(prompt);
}
