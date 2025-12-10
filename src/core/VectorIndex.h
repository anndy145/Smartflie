#ifndef VECTORINDEX_H
#define VECTORINDEX_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>

// HNSW Lib
#include "hnswlib/hnswlib.h"

class VectorIndex {
public:
    VectorIndex(int dim = 4096, int maxElements = 10000);
    ~VectorIndex();

    // 1. Add vector to index
    void addItem(int id, const std::vector<float>& vec);

    // 2. Search K-Nearest Neighbors
    std::vector<std::pair<int, float>> search(const std::vector<float>& queryVec, int k = 5);

    // 3. Persistence
    bool save(const std::string& path);
    bool load(const std::string& path);

    // 4. Maintenance
    int getCurrentCount() const;

private:
    int dim;
    int maxElements;
    std::unique_ptr<hnswlib::L2Space> space;
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> alg_hnsw;
    std::mutex mutex;
};

#endif // VECTORINDEX_H
