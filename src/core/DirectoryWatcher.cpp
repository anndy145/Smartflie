#include "DirectoryWatcher.h"
#include <QDir>
#include <QDebug>
#include <filesystem>

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
{
    watcher = new QFileSystemWatcher(this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryWatcher::onDirectoryChanged);
    connect(watcher, &QFileSystemWatcher::fileChanged, this, &DirectoryWatcher::onFileChanged);
}

DirectoryWatcher::~DirectoryWatcher()
{
}

void DirectoryWatcher::addPath(const QString &path)
{
    if (monitoredPaths.contains(path)) return;

    QFileInfo info(path);
    if (!info.exists() || !info.isDir()) return;

    watcher->addPath(path);
    monitoredPaths.insert(path);
    
    // Initial snapshot
    updateDirectoryContent(path);

    qDebug() << "DirectoryWatcher: Monitoring" << path;
}

void DirectoryWatcher::removePath(const QString &path)
{
    if (!monitoredPaths.contains(path)) return;

    watcher->removePath(path);
    monitoredPaths.remove(path);
    dirContents.remove(path);
    
    qDebug() << "DirectoryWatcher: Stopped monitoring" << path;
}

QStringList DirectoryWatcher::directories() const
{
    return monitoredPaths.values();
}

void DirectoryWatcher::onDirectoryChanged(const QString &path)
{
    qDebug() << "Directory Change Detected:" << path;
    
    // Compare new state vs old state to find added/removed files
    // Note: QFileSystemWatcher doesn't tell us *what* changed, only *that* it changed.
    
    QSet<QString> oldFiles = dirContents.value(path);
    QSet<QString> newFiles;

    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QStringList entries = dir.entryList();
    
    for (const QString &entry : entries) {
        newFiles.insert(entry);
    }
    
    // Find Added
    QSet<QString> added = newFiles - oldFiles;
    for (const QString &f : added) {
        QString fullPath = dir.absoluteFilePath(f);
        qDebug() << "  [+] File Added:" << fullPath;
        emit fileAdded(fullPath);
    }

    // Find Deleted
    QSet<QString> removed = oldFiles - newFiles;
    for (const QString &f : removed) {
        QString fullPath = dir.absoluteFilePath(f);
        qDebug() << "  [-] File Deleted:" << fullPath;
        emit fileDeleted(fullPath);
    }

    // Update Cache
    dirContents[path] = newFiles;
    
    emit directoryChanged(path);
}

void DirectoryWatcher::onFileChanged(const QString &path)
{
    qDebug() << "File Modified:" << path;
    emit fileChanged(path);
}

void DirectoryWatcher::updateDirectoryContent(const QString &path)
{
    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QStringList entries = dir.entryList();
    
    QSet<QString> currentFiles;
    for (const QString &entry : entries) {
        currentFiles.insert(entry);
    }
    
    dirContents[path] = currentFiles;
}

void DirectoryWatcher::scanDirectory(const QString &path)
{
    updateDirectoryContent(path);
}

