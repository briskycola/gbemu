#pragma once

#include <filesystem>
#include <string>
#include <vector>

// A modal Dear ImGui dialog for picking a ROM
// file from disk. Call open() to show it, then
// call draw() once per frame; draw() returns true
// on the frame the user confirms a file.
class FileBrowser
{
    private:
        struct Entry
        {
            std::filesystem::path path;
            std::string name;
            bool isDirectory;
        };

        std::filesystem::path currentDirectory;
        std::vector<Entry> entries;
        int selectedIndex = -1;
        bool showAllFiles = false;
        bool openRequested = false;
        bool visible = false;
        char pathInput[1024] = {};
        std::string errorMessage;

        void changeDirectory(const std::filesystem::path &directory);
        void refresh();
        bool isROMFile(const std::filesystem::path &path) const;

    public:
        FileBrowser();
        void open();
        bool isOpen() const;
        void setDirectory(const std::filesystem::path &directory);
        bool draw(std::string &chosenPath);
};
