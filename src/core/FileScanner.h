#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <filesystem>
#include <vector>
#include <string>

class FileScanner
{
public:
    FileScanner();
    std::vector<std::string> scanDirectory(const std::string& path, bool recursive = false);

private:
    bool isIgnored(const std::filesystem::path& path);
};

#endif // FILESCANNER_H
