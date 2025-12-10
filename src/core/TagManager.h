#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <string>
#include <vector>
#include <map>


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
    
    // File operations support
    void renameFile(const std::string& oldFilename, const std::string& newFilename);
    void removeFile(const std::string& filename);

    std::vector<std::string> getAllTags() const;
    std::vector<std::string> getFilesByTag(const std::string& tag) const;



private:
    std::string currentDirectory;
    
    // Migration helper
    void migrateJsonToSql(const std::string& jsonPath);
};

#endif // TAGMANAGER_H
