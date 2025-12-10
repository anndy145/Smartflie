#include "TagManager.h"
#include "DatabaseManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <set>
#include <nlohmann/json.hpp> // For migration only

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

TagManager::TagManager() {
}

void TagManager::loadTags(const std::string& directory) {
    currentDirectory = directory;
    
    // Create .smartfile directory if not exists
    std::string smartfileDir = currentDirectory + "/.smartfile";
    if (!fs::exists(smartfileDir)) {
        fs::create_directory(smartfileDir);
#ifdef _WIN32
        SetFileAttributesA(smartfileDir.c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
    }

    // Init DB
    std::string dbPath = smartfileDir + "/smartfile.db";
    DatabaseManager::instance().init(QString::fromStdString(dbPath));

    // Check for Migration
    std::string jsonPath = smartfileDir + "/metadata.json";
    if (fs::exists(jsonPath)) {
        migrateJsonToSql(jsonPath);
    }
}

void TagManager::migrateJsonToSql(const std::string& jsonPath) {
    std::cout << "Migrating metadata.json to SQLite..." << std::endl;
    try {
        std::ifstream f(jsonPath);
        nlohmann::json j = nlohmann::json::parse(f);
        f.close();

        for (auto& element : j.items()) {
            std::string filename = element.key();
            // Ensure file exists in DB
            DatabaseManager::instance().addFile(QString::fromStdString(filename));
            int fileId = DatabaseManager::instance().getFileId(QString::fromStdString(filename));
            
            for (const auto& tag : element.value()) {
                std::string tagName = tag.get<std::string>();
                int tagId = DatabaseManager::instance().getOrCreateTagId(QString::fromStdString(tagName));
                DatabaseManager::instance().addTagToFile(fileId, tagId);
            }
        }
        
        // Rename json to .bak to prevent re-migration
        std::string bakPath = jsonPath + ".bak";
        if (fs::exists(bakPath)) fs::remove(bakPath);
        fs::rename(jsonPath, bakPath);
        std::cout << "Migration complete. metadata.json renamed to .bak" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Migration failed: " << e.what() << std::endl;
    }
}

void TagManager::saveTags() {
    // No-op for SQLite (Auto-save)
}

void TagManager::addTag(const std::string& filename, const std::string& tag) {
    auto& db = DatabaseManager::instance();
    db.addFile(QString::fromStdString(filename));
    int fileId = db.getFileId(QString::fromStdString(filename));
    int tagId = db.getOrCreateTagId(QString::fromStdString(tag));
    db.addTagToFile(fileId, tagId);
}

void TagManager::removeTag(const std::string& filename, const std::string& tag) {
    auto& db = DatabaseManager::instance();
    int fileId = db.getFileId(QString::fromStdString(filename));
    int tagId = db.getOrCreateTagId(QString::fromStdString(tag));
    db.removeTagFromFile(fileId, tagId);
}

void TagManager::deleteTag(const std::string& tag) {
    // TODO: Implement global tag deletion in DatabaseManager
    // For now: No-op or we need to query all files
    // Ideally: DELETE FROM tags WHERE name = tag; (CASCADE handles the rest)
    QSqlQuery query;
    query.prepare("DELETE FROM tags WHERE name = :name");
    query.bindValue(":name", QString::fromStdString(tag));
    query.exec();
}

std::vector<std::string> TagManager::getTags(const std::string& filename) const {
    auto qTags = DatabaseManager::instance().getTagsForFile(QString::fromStdString(filename));
    std::vector<std::string> tags;
    for(const auto& t : qTags) tags.push_back(t.toStdString());
    return tags;
}

void TagManager::setTags(const std::string& filename, const std::vector<std::string>& tags) {
    auto& db = DatabaseManager::instance();
    db.addFile(QString::fromStdString(filename));
    db.clearTagsForFile(QString::fromStdString(filename));
    
    int fileId = db.getFileId(QString::fromStdString(filename));
    for(const auto& t : tags) {
        int tagId = db.getOrCreateTagId(QString::fromStdString(t));
        db.addTagToFile(fileId, tagId);
    }
}

void TagManager::renameFile(const std::string& oldFilename, const std::string& newFilename) {
    // Update path in DB
    QSqlQuery query;
    query.prepare("UPDATE files SET path = :new WHERE path = :old");
    query.bindValue(":new", QString::fromStdString(newFilename));
    query.bindValue(":old", QString::fromStdString(oldFilename));
    query.exec();
}

void TagManager::removeFile(const std::string& filename) {
    DatabaseManager::instance().removeFile(QString::fromStdString(filename));
}

std::vector<std::string> TagManager::getAllTags() const {
    // SELECT name FROM tags
    std::vector<std::string> tags;
    QSqlQuery query("SELECT name FROM tags");
    while(query.next()) {
        tags.push_back(query.value(0).toString().toStdString());
    }
    return tags;
}

std::vector<std::string> TagManager::getFilesByTag(const std::string& tag) const {
    // SELECT f.path FROM files f JOIN file_tags ft...
    // Reuse DB Manager logic logic if possible? Or just query here?
    // DatabaseManager doesn't expose this yet.
    // Let's implement query here or add to Manager.
    // Adding to Manager is cleaner but I can access DB via singleton if I wanted?
    // Actually DatabaseManager::instance() gives access to methods, not raw DB unless I expose getDB().
    // I'll query directly assuming I can include QSqlQuery headers.
    
    std::vector<std::string> files;
    QSqlQuery query;
    query.prepare("SELECT f.path FROM files f "
                  "JOIN file_tags ft ON f.id = ft.file_id "
                  "JOIN tags t ON t.id = ft.tag_id "
                  "WHERE t.name = :name");
    query.bindValue(":name", QString::fromStdString(tag));
    if (query.exec()) {
        while(query.next()) {
            files.push_back(query.value(0).toString().toStdString());
        }
    }
    return files;
}
