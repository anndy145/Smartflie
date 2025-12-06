#include "TagManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

TagManager::TagManager() {
    metadata = nlohmann::json::object();
}

void TagManager::loadTags(const std::string& directory) {
    currentDirectory = directory;
    metadataFile = getMetadataPath();
    metadata.clear();

    if (fs::exists(metadataFile)) {
        try {
            std::ifstream f(metadataFile);
            metadata = nlohmann::json::parse(f);
        } catch (const std::exception& e) {
            std::cerr << "Error loading metadata: " << e.what() << std::endl;
            metadata = nlohmann::json::object();
        }
    } else {
        metadata = nlohmann::json::object();
    }
}

void TagManager::saveTags() {
    if (currentDirectory.empty()) return;

    std::string smartfileDir = currentDirectory + "/.smartfile";
    if (!fs::exists(smartfileDir)) {
        fs::create_directory(smartfileDir);
    }
#ifdef _WIN32
    SetFileAttributesA(smartfileDir.c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif

    try {
        std::ofstream f(metadataFile);
        f << metadata.dump(4);
    } catch (const std::exception& e) {
        std::cerr << "Error saving metadata: " << e.what() << std::endl;
    }
}

void TagManager::addTag(const std::string& filename, const std::string& tag) {
    if (!metadata.contains(filename)) {
        metadata[filename] = nlohmann::json::array();
    }
    
    // Check if tag already exists
    bool exists = false;
    for (const auto& t : metadata[filename]) {
        if (t.get<std::string>() == tag) {
            exists = true;
            break;
        }
    }
    
    if (!exists) {
        metadata[filename].push_back(tag);
        saveTags();
    }
}

void TagManager::removeTag(const std::string& filename, const std::string& tag) {
    if (metadata.contains(filename)) {
        auto& tags = metadata[filename];
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            if (it->get<std::string>() == tag) {
                tags.erase(it);
                saveTags();
                break;
            }
        }
    }
}

void TagManager::deleteTag(const std::string& tag) {
    bool changed = false;
    for (auto& element : metadata.items()) {
        auto& tags = element.value();
        for (auto it = tags.begin(); it != tags.end(); ) {
            if (it->get<std::string>() == tag) {
                it = tags.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    if (changed) {
        saveTags();
    }
}

std::vector<std::string> TagManager::getTags(const std::string& filename) const {
    std::vector<std::string> tags;
    if (metadata.contains(filename)) {
        for (const auto& t : metadata[filename]) {
            tags.push_back(t.get<std::string>());
        }
    }
    return tags;
}

void TagManager::setTags(const std::string& filename, const std::vector<std::string>& tags) {
    metadata[filename] = tags;
    saveTags();
}

std::vector<std::string> TagManager::getAllTags() const {
    std::set<std::string> uniqueTags;
    for (auto& element : metadata.items()) {
        for (const auto& tag : element.value()) {
            uniqueTags.insert(tag.get<std::string>());
        }
    }
    return std::vector<std::string>(uniqueTags.begin(), uniqueTags.end());
}

std::vector<std::string> TagManager::getFilesByTag(const std::string& tag) const {
    std::vector<std::string> files;
    for (auto& element : metadata.items()) {
        for (const auto& t : element.value()) {
            if (t.get<std::string>() == tag) {
                files.push_back(element.key());
                break;
            }
        }
    }
    return files;
}

std::string TagManager::getMetadataPath() const {
    return currentDirectory + "/.smartfile/metadata.json";
}
