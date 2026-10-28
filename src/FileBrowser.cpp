#include "FileBrowser.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>

namespace fs = std::filesystem;

static constexpr const char *POPUP_NAME = "Open ROM";

// The dialog opens centered, covering 80% of the window
static constexpr float DIALOG_SIZE_FRACTION = 0.8f;
static const ImVec2 CENTER_PIVOT(0.5f, 0.5f);

// The footer below the file list has two rows: the
// "Show all files" checkbox and the selection/buttons row
static constexpr float FOOTER_ROWS = 2.0f;
static constexpr float BUTTON_WIDTH = 80.0f;
static constexpr int FOOTER_BUTTON_COUNT = 2;

static const ImVec4 ERROR_TEXT_COLOR(1.0f, 0.4f, 0.4f, 1.0f);

// Passing -1 as a widget width makes it fill the rest of the row
static constexpr float FILL_REMAINING_WIDTH = -1.0f;

static std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

FileBrowser::FileBrowser()
{
    std::error_code error;
    fs::path startDirectory = fs::current_path(error);
    changeDirectory(error ? fs::path(".") : startDirectory);
}

void FileBrowser::open()
{
    openRequested = true;
    visible = true;
    refresh();
}

bool FileBrowser::isOpen() const
{
    return visible;
}

void FileBrowser::setDirectory(const fs::path &directory)
{
    changeDirectory(directory);
}

bool FileBrowser::isROMFile(const fs::path &path) const
{
    std::string extension = toLower(path.extension().string());
    return extension == ".gb" || extension == ".gbc" || extension == ".sgb";
}

void FileBrowser::changeDirectory(const fs::path &directory)
{
    std::error_code error;
    fs::path resolved = fs::canonical(directory, error);
    if (error || !fs::is_directory(resolved, error))
    {
        errorMessage = "Not a directory: " + directory.string();
        return;
    }

    currentDirectory = resolved;
    refresh();
}

void FileBrowser::refresh()
{
    entries.clear();
    selectedIndex = -1;
    errorMessage.clear();

    std::string pathText = currentDirectory.string();
    std::strncpy(pathInput, pathText.c_str(), sizeof(pathInput) - 1);
    pathInput[sizeof(pathInput) - 1] = '\0';

    std::error_code error;
    fs::directory_iterator it(currentDirectory, fs::directory_options::skip_permission_denied, error);
    if (error)
    {
        errorMessage = "Could not read directory: " + error.message();
        return;
    }

    for (; it != fs::directory_iterator(); it.increment(error))
    {
        if (error) break;

        const fs::path &path = it->path();
        std::string name = path.filename().string();

        // Skip hidden files and folders
        if (name.empty() || name[0] == '.') continue;

        std::error_code typeError;
        bool isDirectory = it->is_directory(typeError);
        if (typeError) continue;
        if (!isDirectory && !showAllFiles && !isROMFile(path)) continue;

        entries.push_back({ path, name, isDirectory });
    }

    // Directories first, then files, both sorted
    // alphabetically without regard to case
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b)
    {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return toLower(a.name) < toLower(b.name);
    });
}

bool FileBrowser::draw(std::string &chosenPath)
{
    if (openRequested)
    {
        ImGui::OpenPopup(POPUP_NAME);
        openRequested = false;
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, CENTER_PIVOT);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * DIALOG_SIZE_FRACTION, viewport->Size.y * DIALOG_SIZE_FRACTION),
                             ImGuiCond_Appearing);

    bool keepOpen = true;
    if (!ImGui::BeginPopupModal(POPUP_NAME, &keepOpen))
    {
        visible = false;
        return false;
    }

    bool fileChosen = false;
    fs::path pendingDirectory;

    // Navigation bar: go up a level, or type a path
    if (ImGui::Button("Up") && currentDirectory.has_parent_path())
    {
        pendingDirectory = currentDirectory.parent_path();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(FILL_REMAINING_WIDTH);
    if (ImGui::InputText("##path", pathInput, sizeof(pathInput), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        pendingDirectory = fs::path(pathInput);
    }

    if (!errorMessage.empty())
    {
        ImGui::TextColored(ERROR_TEXT_COLOR, "%s", errorMessage.c_str());
    }

    // Leave room for the footer below the list
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() * FOOTER_ROWS;
    if (ImGui::BeginChild("##entries", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_Borders))
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const Entry &entry = entries[i];
                std::string label = entry.isDirectory ? "[DIR]  " + entry.name + "/" : "       " + entry.name;

                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str(), selectedIndex == i, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    selectedIndex = i;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (entry.isDirectory)
                        {
                            pendingDirectory = entry.path;
                        }
                        else
                        {
                            chosenPath = entry.path.string();
                            fileChosen = true;
                        }
                    }
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    if (ImGui::Checkbox("Show all files", &showAllFiles))
    {
        refresh();
    }

    const bool hasSelection = selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size());
    const char *selectedName = hasSelection ? entries[selectedIndex].name.c_str() : "";
    ImGui::Text("Selected: %s", selectedName);

    const ImGuiStyle &style = ImGui::GetStyle();
    const float buttonsWidth = BUTTON_WIDTH * FOOTER_BUTTON_COUNT + style.ItemSpacing.x * (FOOTER_BUTTON_COUNT - 1);

    // Right-align the Open and Cancel buttons
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonsWidth));

    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("Open", ImVec2(BUTTON_WIDTH, 0.0f)) && hasSelection)
    {
        const Entry &entry = entries[selectedIndex];
        if (entry.isDirectory)
        {
            pendingDirectory = entry.path;
        }
        else
        {
            chosenPath = entry.path.string();
            fileChosen = true;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(BUTTON_WIDTH, 0.0f)))
    {
        keepOpen = false;
    }

    if (!pendingDirectory.empty())
    {
        changeDirectory(pendingDirectory);
    }

    if (fileChosen || !keepOpen)
    {
        ImGui::CloseCurrentPopup();
        visible = false;
    }

    ImGui::EndPopup();
    return fileChosen;
}
