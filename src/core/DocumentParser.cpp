#include "DocumentParser.h"
#include "miniz.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::string DocumentParser::extractText(const std::string& filePath)
{
    try {
        QString qPath = QString::fromStdString(filePath);
        QFileInfo fi(qPath);
        std::string ext = "." + fi.suffix().toLower().toStdString();
        // Handle empty extension case
        if (ext == ".") ext = "";

    if (ext == ".docx") {
        return parsDocx(filePath);
    } else if (ext == ".xlsx") {
        return parseXlsx(filePath);
    } else if (ext == ".pptx") {
        return parsePptx(filePath);
    } else if (ext == ".odt" || ext == ".odf") {
        return parseOdt(filePath);
    } else if (ext == ".html" || ext == ".htm" || ext == ".shtml" || ext == ".xhtml") {
        return parseHtml(filePath);
    }

    
    // Explicitly supported text extensions
    static const std::set<std::string> textExts = {
        ".txt", ".md", ".csv", ".xml", ".json", ".log",
        ".cpp", ".h", ".c", ".hpp", ".py", ".js", ".ts", ".css", ".svg",
        ".php", ".asp", ".aspx", ".java", ".cs", ".go", ".rs", ".sql", 
        ".cmake", ".gradle", ".properties", ".ini", ".conf", ".cfg"
    };

    if (textExts.count(ext) || ext.empty()) {
        // Fallback: Try to read as plain text
        QFile file(QString::fromStdString(filePath));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
             // Limit to 32KB to avoid reading massive binary files by accident
             // (Though FileScanner should have filtered known binaries)
             QByteArray data = file.read(32768); 
             return data.toStdString();
        }
    }

    } catch (const std::exception& e) {
        return std::string("Error reading file: ") + e.what();
    } catch (...) {
        return "Unknown error reading file.";
    }
    return "";
}



// Helper to unzip specific file from archive to string
static std::string extractZipEntry(const std::string& zipPath, const std::string& entryName) {
    // Use QFile to handle Unicode paths on Windows correctly
    QFile file(QString::fromStdString(zipPath));
    if (!file.open(QIODevice::ReadOnly)) {
        return "DEBUG: Failed to open file via Qt (Unicode check): " + zipPath;
    }
    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.isEmpty()) {
        return "DEBUG: File is empty: " + zipPath;
    }

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Initialize zip reader from memory buffer instead of file path
    if (!mz_zip_reader_init_mem(&zip_archive, fileData.constData(), fileData.size(), 0)) {
        return "DEBUG: Failed to parse zip structure from memory: " + zipPath;
    }

    int file_index = mz_zip_reader_locate_file(&zip_archive, entryName.c_str(), NULL, 0);
    if (file_index < 0) {
        std::string msg = "DEBUG: Entry '" + entryName + "' not found. Files:\n";
        int num_files = mz_zip_reader_get_num_files(&zip_archive);
        for (int i = 0; i < num_files; i++) {
           mz_zip_archive_file_stat file_stat;
           if (mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
               msg += " - " + std::string(file_stat.m_filename) + "\n";
           }
        }
        mz_zip_reader_end(&zip_archive);
        return msg;
    }

    size_t file_size;
    void* p = mz_zip_reader_extract_file_to_heap(&zip_archive, entryName.c_str(), &file_size, 0);
    if (!p) {
        mz_zip_reader_end(&zip_archive);
        return "DEBUG: Failed to extract entry to heap.";
    }

    std::string content((char*)p, file_size);
    mz_free(p);
    mz_zip_reader_end(&zip_archive);
    return content;
}

std::string DocumentParser::parsDocx(const std::string& filePath)
{
    std::string xmlContent = extractZipEntry(filePath, "word/document.xml");
    if (xmlContent.rfind("DEBUG:", 0) == 0) return xmlContent; // Pass error
    if (xmlContent.empty()) return "DEBUG: Empty document.xml extracted";

    QString text;
    QXmlStreamReader xml(QString::fromStdString(xmlContent));

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == "t") { // Text node in DOCX
                 text += xml.readElementText() + " ";
            }
            if (xml.name().toString() == "p") { // Paragraph
                text += "\n";
            }
        }
    }
    return text.trimmed().toStdString();
}

std::string DocumentParser::parseXlsx(const std::string& filePath)
{
    std::string sharedStringsXml = extractZipEntry(filePath, "xl/sharedStrings.xml");
    if (sharedStringsXml.rfind("DEBUG:", 0) == 0) {
        // Shared strings might just be missing, not an error.
        // But if it says "Failed to open zip", that is an error.
        if (sharedStringsXml.find("Failed to open zip") != std::string::npos) return sharedStringsXml;
        // else fallback to reading sheet1 directly if sharedStrings missing
        sharedStringsXml = ""; 
    }
    
    std::vector<QString> stringTable; // Index -> String

    if (!sharedStringsXml.empty()) {
        QXmlStreamReader xml(QString::fromStdString(sharedStringsXml));
        while (!xml.atEnd()) {
            if (xml.readNextStartElement()) {
                if (xml.name().toString() == "t") {
                     stringTable.push_back(xml.readElementText());
                }
            }
        }
    }

    // Read Sheet 1 (simplify to just sheet1 for now)
    std::string sheetXml = extractZipEntry(filePath, "xl/worksheets/sheet1.xml");
    if (sheetXml.rfind("DEBUG:", 0) == 0) return sheetXml; // Return failure if zip read failed
    if (sheetXml.empty()) return ""; 

    QString result;
    QXmlStreamReader xml(QString::fromStdString(sheetXml));
    while (!xml.atEnd()) {
         if (xml.readNextStartElement()) {
             if (xml.name().toString() == "c") { // Cell
                 QString type = xml.attributes().value("t").toString(); // 's' for shared string
                 
                 // Find value 'v' inside 'c'
                 // But 'c' might just contain 'v' node
                 // We need to read until end of 'c' or find 'v'
                 // Simplified:
             }
             if (xml.name().toString() == "v") { // Value
                 QString val = xml.readElementText();
                 // If previous cell type was 's', lookup in table
                 // Complex to track state here. 
                 // For simple search/AI, dumping sharedStrings is often enough!
             }
         }
    }
    
    // Combining shared strings is usually enough to capture the "text content" for AI analysis
    QString fullText;
    for(const auto& s : stringTable) {
        fullText += s + "\n";
    }
    
    // Fallback: if no shared strings, maybe inline strings?
    if (fullText.isEmpty()) {
        return "(XLSX Read: No shared strings found, raw numeric data skipped)";
    }
    
    return fullText.toStdString();
}

std::string DocumentParser::parsePptx(const std::string& filePath)
{
    std::string fullText;
    
    // Iterate through slides (assuming slide1.xml, slide2.xml... up to 50 for performance)
    for (int i = 1; i <= 50; ++i) {
        std::string entryName = "ppt/slides/slide" + std::to_string(i) + ".xml";
        std::string slideXml = extractZipEntry(filePath, entryName);
        
        // Stop if file not found (returns DEBUG error message)
        if (slideXml.rfind("DEBUG:", 0) == 0) {
            break; 
        }
        
        QXmlStreamReader xml(QString::fromStdString(slideXml));
        while (!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                // In PPTX, text is usually in <a:t> (namespace a, tag t)
                // QXmlStreamReader::name() returns "t".
                if (xml.name().toString() == "t") { 
                    fullText += xml.readElementText().toStdString() + "\n";
                }
            }
        }
    }
    
    if (fullText.empty()) {
        return "(PPTX Read: No text found or encrypted)";
    }
    
    return fullText;
}



std::string DocumentParser::parseOdt(const std::string& filePath)
{
    std::string xmlContent = extractZipEntry(filePath, "content.xml");
    if (xmlContent.rfind("DEBUG:", 0) == 0) return xmlContent; 
    
    std::string text;
    QXmlStreamReader xml(QString::fromStdString(xmlContent));

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            // ODT text is in <text:p> or <text:h>
            QString name = xml.name().toString();
            if (name == "p" || name == "h") { 
                text += xml.readElementText().toStdString() + "\n";
            }
        }
    }
    return text;
}

std::string DocumentParser::parseHtml(const std::string& filePath)
{
    // Simple read
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return "";
    
    // Very naive strip tags or just return raw. Llama handles HTML reasonably well.
    // Let's rely on Llama to ignore tags.
    QByteArray data = file.readAll();
    return data.toStdString();
}


