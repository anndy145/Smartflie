#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QSplitter>
#include <QScrollArea>
#include <QToolBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QFutureWatcher>
#include <QtConcurrent>
#include "GraphWidget.h"
#include "../ai/LlamaEngine.h"
#include "../core/TagManager.h"
#include "../core/DirectoryWatcher.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFolder();
    void scanFiles();
    void loadModel();
    void analyzeFile();
    void onAnalysisFinished();
    void saveTags();
    void openFile(QListWidgetItem* item); // Double click
    void renameFile(); // Context menu
    void deleteFile(); // Context menu
    void showContextMenu(const QPoint &pos); // Right click
    void addTag();
    void removeTag();
    void removeGlobalTag();
    void filterFiles(const QString &text);
    void onFileSelected(QListWidgetItem *item);
    void onTagSelected(QListWidgetItem *item);
    void onTabChanged(int index);
    // Zooming
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void updateImageDisplay(); // Applies current scale/pixmap to label

private:
    // UI Components
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QToolBar *toolbar;
    QCheckBox *chkRecursive;
    QTabWidget *tabWidget;
    QScrollArea *scrollArea;
    
    // Tab 1: Explorer
    QWidget *explorerTab;
    QSplitter *mainSplitter;
    
    // Left Panel (Tags)
    QWidget *leftPanel;
    QListWidget *tagListWidget;
    QPushButton *btnLeftAddTag;
    QPushButton *btnLeftRemoveTag;
    
    // Middle Panel (Files)
    QWidget *middlePanel;
    QLineEdit *txtSearch;
    QListWidget *fileList;
    
    // Right Panel (Details)
    QWidget *rightPanel;
    QLabel *lblPreviewImage;
    QTextEdit *txtPreviewText;
    QLabel *lblTags;
    QLabel *lblStatus;
    QPushButton *btnAnalyzeFile;
    QPushButton *btnSaveTags;
    QPushButton *btnAddTag;
    QPushButton *btnRemoveTag;

    // Tab 2: Graph
    GraphWidget *graphWidget;

    // Data
    QString currentPath;
    LlamaEngine llamaEngine;
    TagManager tagManager;
    QFutureWatcher<std::string> *watcher;
    
    // State
    QPixmap currentPreviewPixmap; // Store original for resizing logic
    double scaleFactor = 1.0;

    // Watcher
    DirectoryWatcher dirWatcher;

private slots:
    void onMoniFileAdded(const QString &path);
    void onMoniFileDeleted(const QString &path);

    // Vector UI
    void onGenerateEmbedding(); // Context menu action
    void onFindSimilar();       // Context menu action

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupToolbar();
    void setupLayout();
    void updateTagList();
    void updateFilePreview(const QString& filePath);
    void updateTagDisplay(const QString& filename);
};

#endif // MAINWINDOW_H
