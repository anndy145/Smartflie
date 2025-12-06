#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox> // Added
#include <QSplitter>
#include <QScrollArea>
#include <QToolBar>
#include <QTabWidget>
#include <QTextEdit> // Added back
#include <QFutureWatcher> // Added back
#include <QtConcurrent> // Added back
#include "GraphWidget.h"
#include "../ai/LlamaEngine.h" // Added back
#include "../core/TagManager.h" // Added back

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
    void addTag();
    void removeTag();
    void removeGlobalTag();
    void filterFiles(const QString &text);
    void onFileSelected(QListWidgetItem *item);
    void onTagSelected(QListWidgetItem *item);
    void onTabChanged(int index);

private:
    // UI Components
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QToolBar *toolbar;
    QCheckBox *chkRecursive;
    QTabWidget *tabWidget;
    
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

    void setupToolbar();
    void setupLayout();
    void updateTagList();
    void updateFilePreview(const QString& filePath);
    void updateTagDisplay(const QString& filename);
};

#endif // MAINWINDOW_H
