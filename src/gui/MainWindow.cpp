#include "MainWindow.h"
#include "../core/DocumentParser.h"
#include "../core/DatabaseManager.h"
#include "../core/FileScanner.h"
#include "../core/DocumentParser.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog> 
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <fstream>
#include <algorithm>
#include <set>
#include <QTreeWidgetItem>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setupToolbar();
    setupLayout();

    // Initialize watcher
    watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, this, &MainWindow::onAnalysisFinished);

    resize(1200, 800);
    setWindowTitle("Smart File Organizer");
    
    // Connect Watcher
    connect(&dirWatcher, &DirectoryWatcher::fileAdded, this, &MainWindow::onMoniFileAdded);
    connect(&dirWatcher, &DirectoryWatcher::fileDeleted, this, &MainWindow::onMoniFileDeleted);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupToolbar()
{
    toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    QAction *openAction = new QAction("開啟資料夾 (Open Folder)", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFolder);
    toolbar->addAction(openAction);

    chkRecursive = new QCheckBox("遞迴掃描 (Recursive)", this);
    chkRecursive->setChecked(false);
    toolbar->addWidget(chkRecursive);

    chkShowHidden = new QCheckBox("顯示隱藏檔 (Show Hidden)", this);
    chkShowHidden->setChecked(false);
    toolbar->addWidget(chkShowHidden);

    toolbar->addSeparator();
    // Note: Recursive check now mostly affects searching or tagging operations, 
    // as the Tree View is lazy-loaded by default to prevent freezing.

    toolbar->addSeparator();

    QAction *loadModelAction = new QAction("載入模型 (Load Model)", this);
    connect(loadModelAction, &QAction::triggered, this, &MainWindow::loadModel);
    toolbar->addAction(loadModelAction);
}

void MainWindow::setupLayout()
{
    tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);

    // === Tab 1: Explorer ===
    explorerTab = new QWidget(this);
    QHBoxLayout *explorerLayout = new QHBoxLayout(explorerTab);
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    explorerLayout->addWidget(mainSplitter);
    
    // --- Left Panel (Tags) ---
    leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->addWidget(new QLabel("標籤列表 (Tags)"));
    
    tagListWidget = new QListWidget(this);
    connect(tagListWidget, &QListWidget::itemClicked, this, &MainWindow::onTagSelected);
    leftLayout->addWidget(tagListWidget);
    
    // Left Panel Actions
    QHBoxLayout *leftActionLayout = new QHBoxLayout();
    btnLeftAddTag = new QPushButton("新增", this);
    btnLeftAddTag->setToolTip("新增標籤 (Add Tag)");
    connect(btnLeftAddTag, &QPushButton::clicked, this, &MainWindow::addTag);
    leftActionLayout->addWidget(btnLeftAddTag);

    btnLeftRemoveTag = new QPushButton("移除", this);
    btnLeftRemoveTag->setToolTip("移除標籤 (Global Delete Tag)");
    connect(btnLeftRemoveTag, &QPushButton::clicked, this, &MainWindow::removeGlobalTag);
    leftActionLayout->addWidget(btnLeftRemoveTag);
    
    leftLayout->addLayout(leftActionLayout);
    
    mainSplitter->addWidget(leftPanel);

    // --- Middle Panel (Files) ---
    middlePanel = new QWidget(this);
    QVBoxLayout *midLayout = new QVBoxLayout(middlePanel);
    midLayout->addWidget(new QLabel("檔案列表 (Files)"));

    txtSearch = new QLineEdit(this);
    txtSearch->setPlaceholderText("搜尋檔案... (Search)");
    connect(txtSearch, &QLineEdit::textChanged, this, &MainWindow::filterFiles);
    connect(chkRecursive, &QCheckBox::stateChanged, this, &MainWindow::scanFiles);
    connect(chkShowHidden, &QCheckBox::stateChanged, this, &MainWindow::scanFiles);
    midLayout->addWidget(txtSearch);

    fileList = new QTreeWidget(this);
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    fileList->setHeaderHidden(true); 
    fileList->setColumnCount(1);
    
    connect(fileList, &QTreeWidget::itemClicked, this, &MainWindow::onFileSelected);
    connect(fileList, &QTreeWidget::itemDoubleClicked, this, &MainWindow::openFile); 
    connect(fileList, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showContextMenu); 
    // Connect Expansion Signal for Lazy Loading
    connect(fileList, &QTreeWidget::itemExpanded, this, &MainWindow::onItemExpanded);

    midLayout->addWidget(fileList);

    mainSplitter->addWidget(middlePanel);

    // --- Right Panel (Details & Preview) ---
    rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->addWidget(new QLabel("檔案預覽 (Preview)"));

    // Preview Area
    scrollArea = new QScrollArea(this);
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidgetResizable(false); 
    scrollArea->setAlignment(Qt::AlignCenter); 
    
    lblPreviewImage = new QLabel("請選擇檔案以預覽 (Select file to preview)", this);
    lblPreviewImage->setAlignment(Qt::AlignCenter);
    lblPreviewImage->setScaledContents(true); 
    
    scrollArea->setWidget(lblPreviewImage);
    rightLayout->addWidget(scrollArea);
    
    // Zoom Controls
    QHBoxLayout *zoomLayout = new QHBoxLayout();
    QPushButton *btnZoomIn = new QPushButton("放大", this);
    QPushButton *btnZoomOut = new QPushButton("縮小", this);
    QPushButton *btnFit = new QPushButton("適應視窗", this);
    
    connect(btnZoomIn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(btnFit, &QPushButton::clicked, this, &MainWindow::fitToWindow);
    
    zoomLayout->addWidget(btnZoomIn);
    zoomLayout->addWidget(btnZoomOut);
    zoomLayout->addWidget(btnFit);
    rightLayout->addLayout(zoomLayout);
    
    txtPreviewText = new QTextEdit(this);
    txtPreviewText->setReadOnly(true);
    txtPreviewText->setVisible(false); 
    rightLayout->addWidget(txtPreviewText);

    // Tags Section
    lblTags = new QLabel("標籤: --", this);
    lblTags->setWordWrap(true);
    lblTags->setStyleSheet("font-weight: bold; margin-top: 10px;");
    rightLayout->addWidget(lblTags);

    // Actions
    QHBoxLayout *actionLayout = new QHBoxLayout();
    btnAnalyzeFile = new QPushButton("分析檔案 (Analyze)", this);
    connect(btnAnalyzeFile, &QPushButton::clicked, this, &MainWindow::analyzeFile);
    actionLayout->addWidget(btnAnalyzeFile);

    btnSaveTags = new QPushButton("儲存標籤 (Save)", this);
    connect(btnSaveTags, &QPushButton::clicked, this, &MainWindow::saveTags);
    btnSaveTags->setEnabled(false);
    actionLayout->addWidget(btnSaveTags);

    // Manual Tag Management
    btnAddTag = new QPushButton("新增標籤 (Add)", this);
    connect(btnAddTag, &QPushButton::clicked, this, &MainWindow::addTag);
    actionLayout->addWidget(btnAddTag);

    btnRemoveTag = new QPushButton("移除標籤 (Remove)", this);
    connect(btnRemoveTag, &QPushButton::clicked, this, &MainWindow::removeTag);
    actionLayout->addWidget(btnRemoveTag);

    rightLayout->addLayout(actionLayout);

    lblStatus = new QLabel("準備就緒", this);
    lblStatus->setWordWrap(true);
    rightLayout->addWidget(lblStatus);

    rightLayout->addStretch();
    mainSplitter->addWidget(rightPanel);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setStretchFactor(2, 1);

    tabWidget->addTab(explorerTab, "檔案瀏覽器 (Explorer)");

    // === Tab 2: Graph View ===
    graphWidget = new GraphWidget(&tagManager, this);
    tabWidget->addTab(graphWidget, "關聯視圖 (Graph)");
}

void MainWindow::onTabChanged(int index) {
    if (index == 1) { 
        graphWidget->buildGraph();
    }
}

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory",
                                                    QString(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        currentPath = dir;
        dirWatcher.addPath(currentPath); 
        tagManager.loadTags(currentPath.toStdString());
        scanFiles();
    }
}

void MainWindow::scanFiles()
{
    fileList->clear();
    if (currentPath.isEmpty()) return;

    FileScanner scanner;
    // CRITICAL: Force non-recursive for UI to allow Lazy Loading.
    // Recursive scanning whole drive freezes UI.
    // We only scan top-level here.
    bool recur = chkRecursive->isChecked();
    bool showHidden = chkShowHidden->isChecked();
    std::vector<std::string> entries = scanner.scanDirectory(currentPath.toStdString(), recur, showHidden);

    for (const auto& entry : entries) {
        QString fullPath = QString::fromStdString(entry);
        QFileInfo fi(fullPath);
        QString filename = fi.fileName();

        QTreeWidgetItem* item = new QTreeWidgetItem(fileList);
        item->setText(0, filename);
        
        if (fi.isDir()) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            item->setData(0, Qt::UserRole, fullPath); 
            // Add Dummy Child for expansion
            new QTreeWidgetItem(item); 
        } else {
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            item->setData(0, Qt::UserRole, fullPath); 
        }
    }
    
    fileList->sortItems(0, Qt::AscendingOrder);
    updateTagList();
    lblStatus->setText(QString("已載入 %1 (項目數: %2)").arg(currentPath).arg(entries.size()));
}

void MainWindow::onItemExpanded(QTreeWidgetItem *item)
{
    // Lazy Load Children
    if (item->childCount() == 1 && item->child(0)->text(0).isEmpty()) {
        // It has a dummy child. Remove it.
        delete item->takeChild(0);
        
        QString path = item->data(0, Qt::UserRole).toString();
        FileScanner scanner;
        // Scan sub-folder (Non-recursive)
        bool showHidden = chkShowHidden->isChecked();
        std::vector<std::string> entries = scanner.scanDirectory(path.toStdString(), false, showHidden);
        
        for (const auto& entry : entries) {
            QString fullPath = QString::fromStdString(entry);
            QFileInfo fi(fullPath);
            QString filename = fi.fileName();
            
            QTreeWidgetItem* child = new QTreeWidgetItem(item);
            child->setText(0, filename);
            
            if (fi.isDir()) {
                child->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                child->setData(0, Qt::UserRole, fullPath);
                new QTreeWidgetItem(child); // Dummy
            } else {
                child->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
                child->setData(0, Qt::UserRole, fullPath);
            }
        }
        
       item->sortChildren(0, Qt::AscendingOrder);
    }
}

void MainWindow::updateTagList()
{
    tagListWidget->clear();
    std::vector<std::string> tags = tagManager.getAllTags();
    
    QListWidgetItem* allItem = new QListWidgetItem("All Files");
    allItem->setData(Qt::UserRole, "ALL");
    tagListWidget->addItem(allItem);

    for (const auto& tag : tags) {
        tagListWidget->addItem(QString::fromStdString(tag));
    }
}

void MainWindow::onTagSelected(QListWidgetItem *item)
{
    // Simplified filtering logic for now
    QString tag = item->text();
    QString data = item->data(Qt::UserRole).toString();

    // Note: filtering in Lazy Loaded tree is complex because items might not be loaded.
    // For now, this only filters *loaded* items.
    
    QTreeWidgetItemIterator it(fileList);
    while (*it) {
        if (data == "ALL") {
            (*it)->setHidden(false);
        } else {
            QString path = (*it)->data(0, Qt::UserRole).toString();
            if (path.isEmpty()) {
                 // Folder logic
            } else {
                std::filesystem::path p(path.toStdString());
                std::string filename = p.filename().string();
                
                std::vector<std::string> filesWithTag = tagManager.getFilesByTag(tag.toStdString());
                std::set<std::string> fileSet(filesWithTag.begin(), filesWithTag.end());
                
                bool match = (fileSet.find(filename) != fileSet.end());
                (*it)->setHidden(!match);
                
                if (match) {
                    QTreeWidgetItem* parent = (*it)->parent();
                    while(parent) {
                        parent->setHidden(false);
                        parent->setExpanded(true);
                        parent = parent->parent();
                    }
                }
            }
        }
        ++it;
    }
}

void MainWindow::onFileSelected(QTreeWidgetItem *item, int column)
{
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) return; 
    
    QFileInfo fi(filePath);
    if (fi.isDir()) return; // Don't preview folders

    // Debugging logs removed for production or simplified
    // updateFilePreview handles logging now
    
    updateFilePreview(filePath);
    updateTagDisplay(fi.fileName());
}

void MainWindow::openFile(QTreeWidgetItem* item, int column)
{
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        if (!fi.isDir()) {
             QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    }
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = fileList->itemAt(pos);
    if (!item) return;
    
    // Allow context menu on files
    QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
     
    // Check if file
    // Check if file
    if (QFileInfo(path).isDir()) return;

    QMenu contextMenu(tr("Context Menu"), this);

    QAction *actRename = new QAction(tr("更名 (Rename)"), this);
    connect(actRename, &QAction::triggered, this, &MainWindow::renameFile);
    contextMenu.addAction(actRename);

    QAction *actDelete = new QAction(tr("刪除 (Delete)"), this);
    connect(actDelete, &QAction::triggered, this, &MainWindow::deleteFile);
    contextMenu.addAction(actDelete);
    
    contextMenu.addSeparator();
    
    QAction *actAnaly = new QAction("AI 分析 (Analyze)", this);
    connect(actAnaly, &QAction::triggered, this, &MainWindow::analyzeFile);
    contextMenu.addAction(actAnaly);

    QAction *actEmbed = new QAction("建立向量 (Generate Embedding)", this);
    connect(actEmbed, &QAction::triggered, this, &MainWindow::onGenerateEmbedding);
    contextMenu.addAction(actEmbed);

    QAction *actSim = new QAction("尋找相似 (Find Similar)", this);
    connect(actSim, &QAction::triggered, this, &MainWindow::onFindSimilar);
    contextMenu.addAction(actSim);

    contextMenu.exec(fileList->mapToGlobal(pos));
}

void MainWindow::analyzeFile()
{
    if (fileList->selectedItems().isEmpty()) return;
    QString filePath = fileList->selectedItems().first()->data(0, Qt::UserRole).toString();
    
    if (QFileInfo(filePath).isDir()) return;

    lblStatus->setText("Reading file...");
    QApplication::processEvents();
    
    std::string content = DocumentParser::extractText(filePath.toStdString());
    if (content.empty()) {
        txtPreviewText->setText("(No text content found)");
        return;
    }

    if (!llamaEngine.isModelLoaded()) {
        QMessageBox::warning(this, "Error", "Model not loaded. Please Open Folder containing GGUF model first, or use 'Load Model'."); // Simplified check
        return;
    }

    lblStatus->setText("Analyzing with AI...");
    
    std::string filenameStr = QFileInfo(filePath).fileName().toStdString();
    
    QFuture<std::string> future = QtConcurrent::run(QThreadPool::globalInstance(), [this, filenameStr, content]() -> std::string {
        return llamaEngine.suggestTags(filenameStr, content);
    });
    watcher->setFuture(future);
}

void MainWindow::onAnalysisFinished()
{
    std::string result = watcher->result();
    lblStatus->setText("Analysis Complete");
    QMessageBox::information(this, "AI Analysis", QString::fromStdString(result));
}

void MainWindow::saveTags()
{
    tagManager.saveTags();
    btnSaveTags->setEnabled(false);
    lblStatus->setText("Tags Saved");
}

void MainWindow::addTag()
{
    if (fileList->selectedItems().isEmpty()) return;
    QString filePath = fileList->selectedItems().first()->data(0, Qt::UserRole).toString();
    std::filesystem::path p(filePath.toStdString());
    std::string filename = p.filename().string();
    
    bool ok;
    QString text = QInputDialog::getText(this, tr("Add Tag"),
                                         tr("Tag Name:"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty()) {
        tagManager.addTag(filename, text.toStdString());
        updateTagDisplay(QString::fromStdString(filename));
        updateTagList();
        btnSaveTags->setEnabled(true);
    }
}

void MainWindow::removeTag()
{
    if (fileList->selectedItems().isEmpty()) return;
    QString filePath = fileList->selectedItems().first()->data(0, Qt::UserRole).toString();
    std::filesystem::path p(filePath.toStdString());
    std::string filename = p.filename().string();
    
    std::vector<std::string> tags = tagManager.getTags(filename);
    if (tags.empty()) return;
    
    QStringList items;
    for(const auto& t : tags) items << QString::fromStdString(t);
    
    bool ok;
    QString item = QInputDialog::getItem(this, tr("Remove Tag"), tr("Select Tag:"), items, 0, false, &ok);
    
    if (ok && !item.isEmpty()) {
        tagManager.removeTag(filename, item.toStdString());
        updateTagDisplay(QString::fromStdString(filename));
        updateTagList();
        btnSaveTags->setEnabled(true);
    }
}

void MainWindow::removeGlobalTag()
{
     if (tagListWidget->selectedItems().isEmpty()) return;
     QString tag = tagListWidget->selectedItems().first()->text();
     
     if (tag == "All Files") return;

     QMessageBox::StandardButton reply;
     reply = QMessageBox::question(this, "Delete Tag", 
                                   QString("Are you sure you want to delete tag '%1' from ALL files?").arg(tag),
                                   QMessageBox::Yes|QMessageBox::No);
     if (reply == QMessageBox::Yes) {
         tagManager.deleteTag(tag.toStdString());
         updateTagList();
         if (!fileList->selectedItems().isEmpty()) {
             QString filePath = fileList->selectedItems().first()->data(0, Qt::UserRole).toString();
              QString localName = QFileInfo(filePath).fileName();
              updateTagDisplay(localName);
         }
     }
}

void MainWindow::filterFiles(const QString &text)
{
    QTreeWidgetItemIterator it(fileList);
    while (*it) {
        QString itemText = (*it)->text(0);
        bool match = itemText.contains(text, Qt::CaseInsensitive);
        (*it)->setHidden(!match);
        
        if (match) {
             QTreeWidgetItem* parent = (*it)->parent();
             while(parent) {
                 parent->setHidden(false);
                 parent->setExpanded(true);
                 parent = parent->parent();
             }
        }
        ++it;
    }
}

void MainWindow::updateFilePreview(const QString& filePath)
{
    std::ofstream log("smartfile_debug.log", std::ios::app);
    if (!log.is_open()) return; // Paranoia

    log << "Entring updateFilePreview: " << filePath.toStdString() << std::endl;

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    log << "Extension calculated: " << ext.toStdString() << std::endl;
    
    if (!lblPreviewImage) {
        log << "FATAL: lblPreviewImage is NULL!" << std::endl;
        return;
    }
    log << "lblPreviewImage check OK" << std::endl;

    lblPreviewImage->clear();
    log << "lblPreviewImage->clear() done" << std::endl;

    currentPreviewPixmap = QPixmap();
    log << "currentPreviewPixmap reset done" << std::endl;
    
    if (!txtPreviewText) {
         log << "FATAL: txtPreviewText is NULL!" << std::endl;
         return;
    }
    txtPreviewText->setVisible(false);
    log << "txtPreviewText hidden" << std::endl;
    
    if (!scrollArea) {
        log << "FATAL: scrollArea is NULL!" << std::endl;
        return;
    }
    scrollArea->setVisible(true); 
    log << "scrollArea showed" << std::endl;
    
    log << "UI reset done" << std::endl;

    if (QStringList{"png", "jpg", "jpeg", "bmp", "gif"}.contains(ext)) {
        log << "Trying to load image..." << std::endl;
        QPixmap pix(filePath);
        log << "QPixmap constructor returned. IsNull: " << pix.isNull() << std::endl;
        
        if (!pix.isNull()) {
            currentPreviewPixmap = pix;
            scaleFactor = 1.0;
            log << "Updating Image Display..." << std::endl;
            updateImageDisplay();
            log << "Image Display Updated." << std::endl;
            txtPreviewText->setVisible(false);
        } else {
            lblPreviewImage->setText("無法預覽圖片 (Invalid Image)");
            log << "Image is null." << std::endl;
        }
    } else {
        lblPreviewImage->setText("載入中..."); 
        txtPreviewText->setVisible(true);
        log << "Extracting text from: " << filePath.toStdString() << std::endl;
        
        std::string content = DocumentParser::extractText(filePath.toStdString());
        log << "Text extracted (size: " << content.size() << ")" << std::endl;
        
        txtPreviewText->setText(QString::fromStdString(content));
        log << "Preview text set" << std::endl;
        
        lblPreviewImage->clear();
    }
    log << "updateFilePreview finished." << std::endl;
}

void MainWindow::updateTagDisplay(const QString& filename)
{
    std::vector<std::string> tags = tagManager.getTags(filename.toStdString());
    QString tagStr = "標籤: ";
    for (const auto& t : tags) {
        tagStr += QString::fromStdString(t) + ", ";
    }
    if (tags.empty()) tagStr += "--";
    else tagStr.chop(2); 
    
    lblTags->setText(tagStr);
    
    std::ofstream log("smartfile_debug.log", std::ios::app);
    log << "Tag display updated" << std::endl;
}

void MainWindow::renameFile()
{
    QList<QTreeWidgetItem*> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty()) return;
    
    if (selectedItems.first()->data(0, Qt::UserRole).toString().isEmpty()) return;

    QString oldName = selectedItems.first()->text(0);
    QString fullPath = selectedItems.first()->data(0, Qt::UserRole).toString();
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename File"),
                                            tr("New name:"), QLineEdit::Normal,
                                            oldName, &ok);
    if (ok && !newName.isEmpty() && newName != oldName) {
        QFileInfo oldInfo(fullPath);
        QString newPath = oldInfo.dir().absoluteFilePath(newName);
        
        if (QFile::rename(fullPath, newPath)) {
            // Note: tagManager.renameFile assumes simple filenames for keys, keep that consistent?
            tagManager.renameFile(oldName.toStdString(), newName.toStdString());
            scanFiles(); 
            lblStatus->setText(QString("已更名 %1 -> %2").arg(oldName).arg(newName));
        } else {
             QMessageBox::critical(this, "Error", QString("Rename failed. Check permissions or if file exists."));
        }
    }
}

void MainWindow::deleteFile()
{
    QList<QTreeWidgetItem*> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty()) return;
    
    if (selectedItems.first()->data(0, Qt::UserRole).toString().isEmpty()) return;

    QString filename = selectedItems.first()->text(0);
    QString fullPath = selectedItems.first()->data(0, Qt::UserRole).toString();
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete File", 
                                  QString("確定要刪除 '%1' 嗎?\n(此動作無法復原)").arg(filename),
                                  QMessageBox::Yes|QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (QFile::remove(fullPath)) {
            tagManager.removeFile(filename.toStdString());
            scanFiles();
            txtPreviewText->clear();
            lblPreviewImage->setText("已刪除 (Deleted)");
            currentPreviewPixmap = QPixmap();
            lblTags->setText("標籤: --");
            lblStatus->setText(QString("已刪除 %1").arg(filename));
        } else {
             QMessageBox::critical(this, "Error", "刪除失敗 (Delete failed). File may be in use.");
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateImageDisplay() {
    if (currentPreviewPixmap.isNull()) return;

    QSize newSize = currentPreviewPixmap.size() * scaleFactor;
    lblPreviewImage->resize(newSize);
    lblPreviewImage->setPixmap(currentPreviewPixmap);
}

void MainWindow::zoomIn() {
    scaleFactor *= 1.25;
    updateImageDisplay();
}

void MainWindow::zoomOut() {
    scaleFactor *= 0.8;
    updateImageDisplay();
}

void MainWindow::fitToWindow() {
    if (currentPreviewPixmap.isNull()) return;
    
    QWidget *view = lblPreviewImage->parentWidget();
    if (view) {
        QSize viewSize = view->size();
        double wRatio = (double)viewSize.width() / currentPreviewPixmap.width();
        double hRatio = (double)viewSize.height() / currentPreviewPixmap.height();
        scaleFactor = std::min(wRatio, hRatio) * 0.95; 
        updateImageDisplay();
    }
}

void MainWindow::onMoniFileAdded(const QString &path)
{
    lblStatus->setText("新檔案: " + path);
    scanFiles(); 
}

void MainWindow::onMoniFileDeleted(const QString &path)
{
    lblStatus->setText("檔案刪除: " + path);
    scanFiles(); 
}

void MainWindow::onGenerateEmbedding()
{
    QList<QTreeWidgetItem*> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty()) return;

    QString filePath = selectedItems.first()->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) return; 

    lblStatus->setText("Reading Content...");
    QApplication::processEvents();
    
    std::string content = DocumentParser::extractText(filePath.toStdString());
    if (content.empty()) {
        QMessageBox::warning(this, "Info", "No text content extracted.");
        lblStatus->setText("Ready");
        return;
    }

    if (!llamaEngine.isModelLoaded()) {
        QMessageBox::warning(this, "Error", "Please load model first (Open Folder).");
        return;
    }

    lblStatus->setText("Generating Embedding...");
    QApplication::processEvents();
    
    std::vector<float> vec = llamaEngine.getEmbeddings(content);
    if (vec.empty()) {
        QMessageBox::critical(this, "Error", "Failed to generate embedding.");
        lblStatus->setText("Error");
        return;
    }

    auto& db = DatabaseManager::instance();
    db.addFile(filePath);
    int fileId = db.getFileId(filePath);
    
    if (db.saveVector(fileId, vec)) {
        lblStatus->setText("Vector Saved!");
        QMessageBox::information(this, "Success", "Vector saved to database.");
    } else {
        lblStatus->setText("Save Failed");
        QMessageBox::critical(this, "Error", "Failed to save to database.");
    }
}

void MainWindow::onFindSimilar()
{
    QList<QTreeWidgetItem*> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty()) return;

    QString filePath = selectedItems.first()->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) return; 

    auto& db = DatabaseManager::instance();
    int fileId = db.getFileId(filePath);

    if (fileId == -1) {
        QMessageBox::warning(this, "Info", "File not indexed. Run Generate Embedding first?");
        return;
    }
    
    if (db.getVector(fileId).empty()) {
        QMessageBox::warning(this, "Info", "No vector found. Run Generate Embedding first.");
        return;
    }

    std::vector<int> similarIds = db.findSimilarFiles(fileId, 5); 
    
    if (similarIds.empty()) {
        QMessageBox::information(this, "Result", "No similar files found.");
        return;
    }

    QString resultMsg = "Top 5 Similar Files:\n\n";
    for (int id : similarIds) {
        QString path = db.getFilePath(id);
        resultMsg += QFileInfo(path).fileName() + "\n";
    }
    
    QMessageBox::information(this, "Similarity Result", resultMsg);
}

QTreeWidgetItem* MainWindow::findOrCreateParent(const QString& path, std::map<QString, QTreeWidgetItem*>& nodeMap) {
    return nullptr; 
}

void MainWindow::loadModel()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load Model (GGUF)",
                                                    QString(),
                                                    "GGUF Models (*.gguf);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        lblStatus->setText("Loading Model...");
        QApplication::processEvents();
        
        if (llamaEngine.loadModel(fileName.toStdString())) {
             lblStatus->setText("Model Loaded: " + fileName);
             QMessageBox::information(this, "Info", "Model Loaded Successfully");
        } else {
             lblStatus->setText("Load Failed");
             QMessageBox::critical(this, "Error", "Failed to load model");
        }
    }
}
