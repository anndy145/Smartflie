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
        // Store DB in the .smartfile folder (or relative to exec)
        // For now, let's keep it next to metadata.json in the user customized folder is hard
        // Let's store it in a standard location or local directory
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

