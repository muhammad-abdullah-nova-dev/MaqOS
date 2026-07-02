#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <memory>
#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

class FileInfoViewer {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Text titleText;
    sf::Text pathLabel;
    sf::Text infoText;
    sf::RectangleShape pathBox;
    sf::RectangleShape browseButton;
    sf::Text browseButtonText;
    string currentPath;
    bool isPathActive;
    sf::Clock clock;
    sf::RectangleShape cursor;
    bool showCursor;
    float cursorBlinkTime;
    vector<string> folderHistory;
    int currentFolderIndex;
    sf::RectangleShape scrollBar;
    float scrollOffset;
    bool isDraggingScroll;
    float contentHeight;
    static const int LINE_HEIGHT = 25;
    static const int MAX_LINES = 15;

    void setupWindow() {
        window.create(sf::VideoMode(800, 600), "OS File Info Viewer", sf::Style::Resize | sf::Style::Close);
        window.setFramerateLimit(60);
    }

    void setupFont() {
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            cout << "Error loading font\n";
        }
    }

    void setupUI() {
        // Title
        titleText.setFont(font);
        titleText.setString("File Information Viewer");
        titleText.setCharacterSize(30);
        titleText.setFillColor(sf::Color::White);
        titleText.setPosition(20, 20);

        // Path Label
        pathLabel.setFont(font);
        pathLabel.setString("Enter path or select file:");
        pathLabel.setCharacterSize(20);
        pathLabel.setFillColor(sf::Color::White);
        pathLabel.setPosition(20, 80);

        // Path Box
        pathBox.setSize(sf::Vector2f(600, 40));
        pathBox.setFillColor(sf::Color(50, 50, 50));
        pathBox.setOutlineThickness(2);
        pathBox.setOutlineColor(sf::Color::White);
        pathBox.setPosition(20, 120);

        // Browse Button
        browseButton.setSize(sf::Vector2f(120, 40));
        browseButton.setFillColor(sf::Color(0, 100, 200));
        browseButton.setPosition(640, 120);

        browseButtonText.setFont(font);
        browseButtonText.setString("Browse");
        browseButtonText.setCharacterSize(20);
        browseButtonText.setFillColor(sf::Color::White);
        browseButtonText.setPosition(660, 125);

        // Scroll Bar
        scrollBar.setSize(sf::Vector2f(10, 400));
        scrollBar.setFillColor(sf::Color(100, 100, 100));
        scrollBar.setPosition(770, 180);

        // Cursor
        cursor.setSize(sf::Vector2f(2, 30));
        cursor.setFillColor(sf::Color::White);
        showCursor = true;
        cursorBlinkTime = 0.5f;

        // Initialize current path
        currentPath = fs::current_path().string();
        folderHistory.push_back(currentPath);
        currentFolderIndex = 0;
        scrollOffset = 0;
        isDraggingScroll = false;
        contentHeight = 0;
    }

    string executeCommand(const string& command) {
        array<char, 128> buffer;
        string result;
        
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw runtime_error("popen() failed!");
        }
        
        try {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
        } catch (...) {
            pclose(pipe);
            throw;
        }
        
        pclose(pipe);
        
        if (!result.empty() && result[result.length()-1] == '\n') {
            result.erase(result.length()-1);
        }
        return result;
    }

    string formatFileSize(uintmax_t size) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double fileSize = static_cast<double>(size);
        
        while (fileSize >= 1024 && unit < 4) {
            fileSize /= 1024;
            unit++;
        }
        
        stringstream ss;
        ss << fixed << setprecision(2) << fileSize << " " << units[unit];
        return ss.str();
    }

    string formatTime(const fs::file_time_type& time) {
        auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(
            time - fs::file_time_type::clock::now() + chrono::system_clock::now());
        auto tt = chrono::system_clock::to_time_t(sctp);
        stringstream ss;
        ss << put_time(localtime(&tt), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void updateFileInfo() {
        try {
            if (!fs::exists(currentPath)) {
                infoText.setString("Error: Path does not exist");
                infoText.setFillColor(sf::Color::Red);
                return;
            }

            stringstream info;
            fs::path path(currentPath);
            
            // Basic Information
            info << "Name: " << path.filename().string() << "\n";
            info << "Type: " << (fs::is_directory(path) ? "Directory" : "File") << "\n";
            info << "Path: " << fs::absolute(path).string() << "\n\n";

            // File/Directory specific information
            if (fs::is_directory(path)) {
                // Directory information
                size_t fileCount = 0;
                size_t dirCount = 0;
                uintmax_t totalSize = 0;

                for (const auto& entry : fs::directory_iterator(path)) {
                    if (fs::is_directory(entry)) {
                        dirCount++;
                    } else {
                        fileCount++;
                        totalSize += fs::file_size(entry);
                    }
                }

                info << "Contents:\n";
                info << "  Files: " << fileCount << "\n";
                info << "  Directories: " << dirCount << "\n";
                info << "  Total Size: " << formatFileSize(totalSize) << "\n\n";
            } else {
                // File information
                info << "Size: " << formatFileSize(fs::file_size(path)) << "\n";
                info << "Created: " << formatTime(fs::last_write_time(path)) << "\n\n";
            }

            // Permissions
            auto perms = fs::status(path).permissions();
            info << "Permissions:\n";
            info << "  Owner: " << ((perms & fs::perms::owner_read) != fs::perms::none ? "r" : "-")
                 << ((perms & fs::perms::owner_write) != fs::perms::none ? "w" : "-")
                 << ((perms & fs::perms::owner_exec) != fs::perms::none ? "x" : "-") << "\n";
            info << "  Group: " << ((perms & fs::perms::group_read) != fs::perms::none ? "r" : "-")
                 << ((perms & fs::perms::group_write) != fs::perms::none ? "w" : "-")
                 << ((perms & fs::perms::group_exec) != fs::perms::none ? "x" : "-") << "\n";
            info << "  Others: " << ((perms & fs::perms::others_read) != fs::perms::none ? "r" : "-")
                 << ((perms & fs::perms::others_write) != fs::perms::none ? "w" : "-")
                 << ((perms & fs::perms::others_exec) != fs::perms::none ? "x" : "-") << "\n";

            infoText.setString(info.str());
            infoText.setFillColor(sf::Color::White);
            
            // Calculate content height for scrolling
            contentHeight = infoText.getLocalBounds().height;
        } catch (const fs::filesystem_error& e) {
            infoText.setString("Error: " + string(e.what()));
            infoText.setFillColor(sf::Color::Red);
        }
    }

    void browseFile() {
        try {
            vector<string> commands = {
                "zenity --file-selection --title=\"Select File or Directory\" 2>/dev/null",
                "kdialog --getopenfilename 2>/dev/null",
                "yad --file --title=\"Select File or Directory\" 2>/dev/null",
                "qarma --file-selection --title=\"Select File or Directory\" 2>/dev/null"
            };

            string selectedPath;
            for (const auto& cmd : commands) {
                selectedPath = executeCommand(cmd);
                if (!selectedPath.empty()) {
                    break;
                }
            }
            
            if (!selectedPath.empty()) {
                currentPath = selectedPath;
                folderHistory.push_back(currentPath);
                currentFolderIndex = folderHistory.size() - 1;
                updateFileInfo();
            }
        } catch (const exception& e) {
            infoText.setString("Error opening file dialog: " + string(e.what()));
            infoText.setFillColor(sf::Color::Red);
        }
    }

    void handleInput() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                
                if (pathBox.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    isPathActive = true;
                } else {
                    isPathActive = false;
                }

                if (browseButton.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    browseFile();
                }

                if (scrollBar.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    isDraggingScroll = true;
                }
            }

            if (event.type == sf::Event::MouseButtonReleased) {
                isDraggingScroll = false;
            }

            if (event.type == sf::Event::MouseMoved && isDraggingScroll) {
                float mouseY = sf::Mouse::getPosition(window).y;
                float scrollBarTop = 180;
                float scrollBarBottom = 580;
                float scrollBarHeight = scrollBarBottom - scrollBarTop;
                
                scrollOffset = (mouseY - scrollBarTop) / scrollBarHeight;
                scrollOffset = max(0.0f, min(1.0f, scrollOffset));
            }

            if (event.type == sf::Event::MouseWheelScrolled) {
                scrollOffset -= event.mouseWheelScroll.delta * 0.1f;
                scrollOffset = max(0.0f, min(1.0f, scrollOffset));
            }

            if (event.type == sf::Event::TextEntered && isPathActive) {
                if (event.text.unicode == '\b' && !currentPath.empty()) {
                    currentPath.pop_back();
                }
                else if (event.text.unicode < 128 && event.text.unicode != '\r' && event.text.unicode != '\n') {
                    currentPath += static_cast<char>(event.text.unicode);
                }
                else if (event.text.unicode == '\r' || event.text.unicode == '\n') {
                    updateFileInfo();
                }
            }
        }
    }

    void updateCursor() {
        float deltaTime = clock.restart().asSeconds();
        cursorBlinkTime -= deltaTime;
        if (cursorBlinkTime <= 0) {
            showCursor = !showCursor;
            cursorBlinkTime = 0.5f;
        }
    }

    void draw() {
        window.clear(sf::Color(30, 30, 40));

        // Draw UI elements
        window.draw(titleText);
        window.draw(pathLabel);
        window.draw(pathBox);
        window.draw(browseButton);
        window.draw(browseButtonText);

        // Draw path text
        sf::Text pathText;
        pathText.setFont(font);
        pathText.setString(currentPath);
        pathText.setCharacterSize(20);
        pathText.setFillColor(sf::Color::White);
        pathText.setPosition(30, 125);
        window.draw(pathText);

        // Draw cursor if path is active
        if (isPathActive && showCursor) {
            sf::Text tempText;
            tempText.setFont(font);
            tempText.setString(currentPath);
            tempText.setCharacterSize(20);
            float textWidth = tempText.getLocalBounds().width;
            cursor.setPosition(30 + textWidth, 125);
            window.draw(cursor);
        }

        // Draw info text with scrolling
        infoText.setFont(font);
        infoText.setCharacterSize(20);
        infoText.setPosition(30, 180 - scrollOffset * (contentHeight - 400));
        window.draw(infoText);

        // Draw scroll bar
        float scrollBarHeight = 400;
        float scrollBarY = 180 + scrollOffset * (600 - 180 - scrollBarHeight);
        scrollBar.setPosition(770, scrollBarY);
        window.draw(scrollBar);

        window.display();
    }

public:
    FileInfoViewer() : isPathActive(false), scrollOffset(0), isDraggingScroll(false), contentHeight(0) {
        setupWindow();
        setupFont();
        setupUI();
        updateFileInfo();
    }

    void run() {
        while (window.isOpen()) {
            handleInput();
            updateCursor();
            draw();
        }
    }
};

int main() {
    FileInfoViewer viewer;
    viewer.run();
    return 0;
}
