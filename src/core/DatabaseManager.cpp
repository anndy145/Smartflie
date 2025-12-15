#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

std::mutex DatabaseManager::mutex;

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager()
{
}

DatabaseManager::~DatabaseManager()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseManager::init(const QString& dbName)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbName);
    }

    if (!db.open()) {
        qCritical() << "Error: connection with database failed" << db.lastError();
        return false;
    }
    
    qDebug() << "Database: connection ok";
    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query;
    
    // Files Table
    if (!query.exec("CREATE TABLE IF NOT EXISTS files ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "path TEXT UNIQUE NOT NULL, "
                    "hash TEXT, "
                    "last_modified INTEGER"
                    ")")) {
        qDebug() << "Error creating files table:" << query.lastError();
        return false;
    }

    // Tags Table
    if (!query.exec("CREATE TABLE IF NOT EXISTS tags ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT UNIQUE NOT NULL"
                    ")")) {
        qDebug() << "Error creating tags table:" << query.lastError();
        return false;
    }

    // File-Tags Relation
    if (!query.exec("CREATE TABLE IF NOT EXISTS file_tags ("
                    "file_id INTEGER, "
                    "tag_id INTEGER, "
                    "PRIMARY KEY (file_id, tag_id), "
                    "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE, "
                    "FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE"
                    ")")) {
        qDebug() << "Error creating file_tags table:" << query.lastError();
        return false;
    }

    // Vectors Table (One-to-One with Files)
    if (!query.exec("CREATE TABLE IF NOT EXISTS vectors ("
                    "file_id INTEGER PRIMARY KEY, "
                    "data BLOB, "
                    "dim INTEGER, "
                    "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE"
                    ")")) {
        qDebug() << "Error creating vectors table:" << query.lastError();
        return false;
    }

    // Full-Text Search Table (FTS5)
    if (!query.exec("CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5("
                    "filename, "
                    "content, "
                    "content='files', " 
                    "content_rowid='id'" 
                    ")")) {
        qWarning() << "FTS5 creation with external content failed, trying simple FTS5:" << query.lastError();
        if (!query.exec("CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5(filename, content)")) {
             qDebug() << "Error creating files_fts table (FTS5 might not be available):" << query.lastError();
        }
    }
    
    return true;
}

bool DatabaseManager::addFile(const QString& path)
{
    if (getFileId(path) != -1) return true; // Already exists

    QSqlQuery query;
    query.prepare("INSERT INTO files (path) VALUES (:path)");
    query.bindValue(":path", path);
    return query.exec();
}

bool DatabaseManager::removeFile(const QString& path)
{
    QSqlQuery query;
    query.prepare("DELETE FROM files WHERE path = :path");
    query.bindValue(":path", path);
    return query.exec();
}

int DatabaseManager::getFileId(const QString& path)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM files WHERE path = :path");
    query.bindValue(":path", path);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}

QStringList DatabaseManager::getAllFiles()
{
    QStringList list;
    QSqlQuery query("SELECT path FROM files");
    while (query.next()) {
        list << query.value(0).toString();
    }
    return list;
}

int DatabaseManager::getOrCreateTagId(const QString& tagName)
{
    // Try find
    QSqlQuery query;
    query.prepare("SELECT id FROM tags WHERE name = :name");
    query.bindValue(":name", tagName);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    
    // Create
    query.prepare("INSERT INTO tags (name) VALUES (:name)");
    query.bindValue(":name", tagName);
    if (query.exec()) {
        return query.lastInsertId().toInt();
    }
    return -1;
}

bool DatabaseManager::addTagToFile(int fileId, int tagId)
{
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO file_tags (file_id, tag_id) VALUES (:fid, :tid)");
    query.bindValue(":fid", fileId);
    query.bindValue(":tid", tagId);
    return query.exec();
}

QStringList DatabaseManager::getTagsForFile(const QString& path)
{
    QStringList tags;
    int fileId = getFileId(path);
    if (fileId == -1) return tags;

    QSqlQuery query;
    query.prepare("SELECT t.name FROM tags t "
                  "JOIN file_tags ft ON t.id = ft.tag_id "
                  "WHERE ft.file_id = :fid");
    query.bindValue(":fid", fileId);
    
    if (query.exec()) {
        while (query.next()) {
            tags << query.value(0).toString();
        }
    }
    return tags;
}

void DatabaseManager::clearTagsForFile(const QString& path)
{
    int fileId = getFileId(path);
    if (fileId == -1) return;

    QSqlQuery query;
    query.prepare("DELETE FROM file_tags WHERE file_id = :fid");
    query.bindValue(":fid", fileId);
    query.exec();
}

bool DatabaseManager::removeTagFromFile(int fileId, int tagId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM file_tags WHERE file_id = :fid AND tag_id = :tid");
    query.bindValue(":fid", fileId);
    query.bindValue(":tid", tagId);
    return query.exec();
}


bool DatabaseManager::saveVector(int fileId, const std::vector<float>& vector)
{
    if (vector.empty()) return false;
    
    QByteArray blob(reinterpret_cast<const char*>(vector.data()), vector.size() * sizeof(float));
    
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO vectors (file_id, data, dim) VALUES (:fid, :data, :dim)");
    query.bindValue(":fid", fileId);
    query.bindValue(":data", blob);
    query.bindValue(":dim", (int)vector.size());
    return query.exec();
}

std::vector<float> DatabaseManager::getVector(int fileId)
{
    std::vector<float> vec;
    QSqlQuery query;
    query.prepare("SELECT data, dim FROM vectors WHERE file_id = :fid");
    query.bindValue(":fid", fileId);
    
    if (query.exec() && query.next()) {
        QByteArray blob = query.value(0).toByteArray();
        int dim = query.value(1).toInt();
        
        vec.resize(dim);
        memcpy(vec.data(), blob.constData(), blob.size());
    }
    return vec;
}

std::map<int, std::vector<float>> DatabaseManager::getAllVectors()
{
    std::map<int, std::vector<float>> result;
    QSqlQuery query("SELECT file_id, data, dim FROM vectors");
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        QByteArray blob = query.value(1).toByteArray();
        int dim = query.value(2).toInt();
        
        std::vector<float> vec(dim);
        memcpy(vec.data(), blob.constData(), blob.size());
        
        result[fileId] = vec;
    }
    return result;
}

#include <cmath>
#include <algorithm>

static float cosineSimilarity(const std::vector<float>& A, const std::vector<float>& B) {
    if (A.size() != B.size() || A.empty()) return 0.0f;
    
    float dot = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;
    
    for (size_t i = 0; i < A.size(); ++i) {
        dot += A[i] * B[i];
        normA += A[i] * A[i];
        normB += B[i] * B[i];
    }
    
    if (normA == 0 || normB == 0) return 0.0f;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}



std::vector<int> DatabaseManager::findSimilarFiles(const std::vector<float>& targetVec, int topK)
{
    if (targetVec.empty()) return {};
    
    std::map<int, std::vector<float>> allVecs = getAllVectors();
    std::vector<std::pair<int, float>> scores;
    
    for (const auto& [id, vec] : allVecs) {
        // Simple dimension check - automatically filters disparate models (e.g. CLIP vs Llama)
        if (vec.size() != targetVec.size()) continue;

        float score = cosineSimilarity(targetVec, vec);
        scores.push_back({id, score});
    }
    
    // Sort descending
    std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    std::vector<int> result;
    for (int i = 0; i < std::min((int)scores.size(), topK); ++i) {
        result.push_back(scores[i].first);
    }
    return result;
}

std::vector<int> DatabaseManager::findSimilarFiles(int targetFileId, int topK)
{
    std::vector<float> targetVec = getVector(targetFileId);
    if (targetVec.empty()) return {};
    
    return findSimilarFiles(targetVec, topK); // Reuse logic
}


QString DatabaseManager::getFilePath(int id)
{
    QSqlQuery query;
    query.prepare("SELECT path FROM files WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

