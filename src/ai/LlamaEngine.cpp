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
    if (model) llama_free_model(model);
    llama_backend_free();
}

bool LlamaEngine::loadModel(const std::string& modelPath)
{
    if (ctx) {
        llama_free(ctx);
        ctx = nullptr;
    }
    if (model) {
        llama_free_model(model);
        model = nullptr;
    }

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 100; // Try to use GPU
    model = llama_load_model_from_file(modelPath.c_str(), model_params);

    if (!model) {
        std::cerr << "Failed to load model from " << modelPath << std::endl;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 8192; // Increased to 8k (Balance between OOM and functionality)
    ctx_params.n_batch = 2048; 
    ctx_params.embeddings = true; 
    ctx = llama_new_context_with_model(model, ctx_params);

    if (!ctx) {
        std::cerr << "Failed to create context (OOM?)" << std::endl;
        llama_free_model(model);
        model = nullptr;
        return false;
    }

    return true;
}

std::string LlamaEngine::generateResponse(const std::string& prompt)
{
    if (!ctx || !model) return "Error: Model not loaded";

    // 1. Tokenize
    int n_prompt = llama_tokenize(model, prompt.c_str(), prompt.length(), NULL, 0, true, true);
    if (n_prompt < 0) n_prompt = -n_prompt;
    
    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(model, prompt.c_str(), prompt.length(), prompt_tokens.data(), n_prompt, true, true) < 0) {
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
    
    int n_curr = processed; 
    
    // We keep 'batch' allocated but clear it to reuse for sampling
    // Note: older batch content is irrelevant for next decode, but context retains KV
    
    // 4. Sample loop
    std::stringstream response_ss;
    int n_predict = 256; 
    
    llama_token new_token_id = 0;
    const llama_model * model_ptr = llama_get_model(ctx);
    int n_vocab = llama_n_vocab(model_ptr);

    try {
        for (int i = 0; i < n_predict; ++i) {
             // Safe Logits Access
             float* all_logits = llama_get_logits(ctx);
             if (!all_logits) {
                 std::cerr << "Error: null logits during sampling" << std::endl;
                 break; 
             }

             // Greedy sampling
             float max_val = -1e9;
             int max_idx = 0;
             for (int v = 0; v < n_vocab; ++v) {
                 if (all_logits[v] > max_val) {
                     max_val = all_logits[v];
                     max_idx = v;
                 }
             }
             new_token_id = max_idx;

             if (llama_token_is_eog(model, new_token_id)) { 
                 break;
             }

             char buf[256];
             int n = llama_token_to_piece(model, new_token_id, buf, sizeof(buf), true);
             if (n > 0) {
                 std::string piece(buf, n);
                 response_ss << piece;
             }

             // Decode next token
             // Re-use batch structure (it's just a struct wrapper)
             // We need to re-init it for size 1
             // But wait, we freed it outside earlier? NO, let's NOT free it until end
             // The previous code freed it at 'int n_curr = processed; llama_batch_free(batch);'
             
             // We must NOT reuse a freed batch. We should allocate a new one or keep the old one.
             // Best to just use a small single-token batch for valid generation.
             
             llama_batch batch_one = llama_batch_init(1, 0, 1);
             batch_add(batch_one, new_token_id, n_curr, {0}, true);
             
             int ret = llama_decode(ctx, batch_one);
             llama_batch_free(batch_one);

             if (ret != 0) {
                 std::cerr << "Error: llama_decode failed during generation" << std::endl;
                 break;
             }
             n_curr++;
        }
    } catch (...) {
        std::cerr << "Exception during generation" << std::endl;
    }

    // Cleanup the original large batch
    llama_batch_free(batch);
    
    return response_ss.str();
}

std::string LlamaEngine::suggestTags(const std::string& filename, const std::string& content)
{
    // Limit content to fit in context window (Safe limit)
    // 4000 chars is roughly 2000-3000 tokens (Chinese/English mixed)
    // Prompt overhead is small (~100 tokens)
    std::string safeContent = content.empty() ? "(No content)" : content.substr(0, 4000);
    
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

std::vector<float> LlamaEngine::getEmbeddings(const std::string& text)
{
    if (!ctx || !model) return {};
    
    try {
        // Tokenize
        int n_prompt = llama_tokenize(model, text.c_str(), text.length(), NULL, 0, true, true);
        if (n_prompt < 0) n_prompt = -n_prompt;
        
        std::vector<llama_token> prompt_tokens(n_prompt);
        if (llama_tokenize(model, text.c_str(), text.length(), prompt_tokens.data(), n_prompt, true, true) < 0) {
            return {};
        }

        if (n_prompt >= llama_n_ctx(ctx)) {
            n_prompt = llama_n_ctx(ctx) - 1;
            prompt_tokens.resize(n_prompt);
        }

        // Process batch
        llama_batch batch = llama_batch_init(n_prompt, 0, 1);
        for (int i = 0; i < n_prompt; ++i) {
            batch_add(batch, prompt_tokens[i], i, {0}, (i == n_prompt - 1)); 
        }

        if (llama_decode(ctx, batch) != 0) {
            llama_batch_free(batch);
            return {};
        }
        
        // Get embeddings
        int n_embd = llama_n_embd(model); 
        std::vector<float> result(n_embd);
        
        // Use llama_get_embeddings_ith
        // Note: For older llama.cpp versions or specific builds, we use generic get_embeddings
        const float* emb = nullptr; 
        
        // Try specific ith embedding first if API supports it (b3196 does)
        emb = llama_get_embeddings_ith(ctx, batch.n_tokens - 1);
        
        if (!emb) {
             // Fallback
             emb = llama_get_embeddings(ctx);
        }

        if (emb) {
            memcpy(result.data(), emb, n_embd * sizeof(float));
        } else {
            // Log error
            std::cerr << "Failed to retrieve embeddings" << std::endl;
            result.clear();
        }

        llama_batch_free(batch);
        return result;
    } catch (...) {
        std::cerr << "Exception in getEmbeddings" << std::endl;
        return {};
    }
}
