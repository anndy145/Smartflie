#ifndef DIRECTORY_WATCHER_H
#define DIRECTORY_WATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <QTimer>

class DirectoryWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DirectoryWatcher(QObject *parent = nullptr);
    ~DirectoryWatcher();

    void addPath(const QString &path);
    void removePath(const QString &path);
    QStringList directories() const;

signals:
    void fileChanged(const QString &path);
    void directoryChanged(const QString &path);

    // Friendlier signals for the UI/Logic
    void fileAdded(const QString &path);
    void fileDeleted(const QString &path);

private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);
    
    // Debounce/Scan slot
    void scanDirectory(const QString &path);

private:
    QFileSystemWatcher *watcher;
    QSet<QString> monitoredPaths;
    
    // Cache the state of directory to detect Added/Removed files
    // Map: Directory Path -> Set of Filenames
    QMap<QString, QSet<QString>> dirContents;

    void updateDirectoryContent(const QString &path);
};

#endif // DIRECTORY_WATCHER_H
