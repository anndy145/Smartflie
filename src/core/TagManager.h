#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

class TagManager {
public:
    TagManager();
    
    void loadTags(const std::string& directory);
    void saveTags();
    
    void addTag(const std::string& filename, const std::string& tag);
    void removeTag(const std::string& filename, const std::string& tag);
    void deleteTag(const std::string& tag); // Remove tag from all files
    std::vector<std::string> getTags(const std::string& filename) const;
    
    void setTags(const std::string& filename, const std::vector<std::string>& tags);

    std::vector<std::string> getAllTags() const;
    std::vector<std::string> getFilesByTag(const std::string& tag) const;

private:
    std::string currentDirectory;
    std::string metadataFile;
    nlohmann::json metadata;
    
    std::string getMetadataPath() const;
};

#endif // TAGMANAGER_H
