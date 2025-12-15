#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStringList>
#include <QVariant>
#include <mutex>

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "smartfile.db");
    
    // Schema
    bool createTables();

    // File Operations
    bool addFile(const QString& path);
    bool removeFile(const QString& path);
    int getFileId(const QString& path);
    QString getFilePath(int id);
    QStringList getAllFiles();

    // Tag Operations
    int getOrCreateTagId(const QString& tagName);
    bool addTagToFile(int fileId, int tagId);
    bool removeTagFromFile(int fileId, int tagId);
    QStringList getTagsForFile(const QString& path);
    void clearTagsForFile(const QString& path);

    // Vector Operations
    bool saveVector(int fileId, const std::vector<float>& vector);
    std::vector<float> getVector(int fileId);
    std::map<int, std::vector<float>> getAllVectors(); // For searching
    
    // Similarity Search
    std::vector<int> findSimilarFiles(int targetFileId, int topK = 5);
    std::vector<int> findSimilarFiles(const std::vector<float>& targetVec, int topK = 5);

private:
    DatabaseManager();
    ~DatabaseManager();
    
    QSqlDatabase db;
    static std::mutex mutex;
};

#endif // DATABASE_MANAGER_H
