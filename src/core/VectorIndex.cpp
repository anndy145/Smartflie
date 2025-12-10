#include "VectorIndex.h"
#include <iostream>
#include <fstream>
#include <filesystem>

VectorIndex::VectorIndex(int dim, int maxElements) 
    : dim(dim), maxElements(maxElements)
{
    // Use L2 (Euclidean) distance
    space = std::make_unique<hnswlib::L2Space>(dim);
    // Initialize HNSW index
    // M=16, ef_construction=200 are default recommendations
    alg_hnsw = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), maxElements, 16, 200);
}

VectorIndex::~VectorIndex()
{
}

void VectorIndex::addItem(int id, const std::vector<float>& vec)
{
    if (vec.size() != dim) {
        std::cerr << "VectorIndex Error: Dim mismatch. Expected " << dim << ", got " << vec.size() << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    try {
        alg_hnsw->addPoint(vec.data(), (hnswlib::labeltype)id);
    } catch (const std::runtime_error& e) {
        std::cerr << "VectorIndex Error adding point: " << e.what() << std::endl;
        // Resize could be handled here if needed
    }
}

std::vector<std::pair<int, float>> VectorIndex::search(const std::vector<float>& queryVec, int k)
{
    if (queryVec.size() != dim) return {};

    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::pair<int, float>> results;
    
    try {
        // Query HNSW
        auto pq = alg_hnsw->searchKnn(queryVec.data(), k);
        
        while (!pq.empty()) {
            auto item = pq.top();
            pq.pop();
            // item.second is distance, item.first is label (id)
            // Note: HNSW returns distance, not similarity directly. 
            // Smaller L2 distance = Higher similarity.
            results.push_back({ (int)item.second, item.first }); 
        }
        
        // Reverse to get closest first (HNSW returns furthest in PQ first?)
        // PriorityQueue top is largest distance if default? 
        // Actually searchKnn returns a PQ where top is the *furthest* of the K neighbors.
        // So popping gives us Furthest -> Closest. We want Closest -> Furthest.
        std::reverse(results.begin(), results.end());
        
    } catch (const std::exception& e) {
        std::cerr << "VectorIndex Error searching: " << e.what() << std::endl;
    }
    
    return results;
}

bool VectorIndex::save(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);
    try {
        alg_hnsw->saveIndex(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "VectorIndex Save Failed: " << e.what() << std::endl;
        return false;
    }
}

bool VectorIndex::load(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!std::filesystem::exists(path)) return false;
    
    try {
        alg_hnsw->loadIndex(path, space.get(), maxElements);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "VectorIndex Load Failed: " << e.what() << std::endl;
        return false;
    }
}

int VectorIndex::getCurrentCount() const
{
    return alg_hnsw->cur_element_count;
}
