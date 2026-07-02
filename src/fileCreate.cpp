#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <memory>
#include <array>

using namespace std;
namespace fs = std::filesystem;

class FileCreator {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Text titleText;
    sf::Text inputLabel;
    sf::Text pathLabel;
    sf::Text statusText;
    sf::RectangleShape inputBox;
    sf::RectangleShape pathBox;
    sf::RectangleShape createButton;
    sf::RectangleShape browseButton;
    sf::Text createButtonText;
    sf::Text browseButtonText;
    string currentInput;
    string currentPath;
    bool isInputActive;
    bool isPathActive;
    sf::Clock clock;
    sf::RectangleShape cursor;
    bool showCursor;
    float cursorBlinkTime;
    vector<string> folderHistory;
    int currentFolderIndex;

    void setupWindow() {
        window.create(sf::VideoMode(800, 600), "OS File Creator", sf::Style::Resize | sf::Style::Close);
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
        titleText.setString("File Creator");
        titleText.setCharacterSize(30);
        titleText.setFillColor(sf::Color::White);
        titleText.setPosition(20, 20);

        // Path Label
        pathLabel.setFont(font);
        pathLabel.setString("Current Folder:");
        pathLabel.setCharacterSize(20);
        pathLabel.setFillColor(sf::Color::White);
        pathLabel.setPosition(20, 80);

        // Path Box
        pathBox.setSize(sf::Vector2f(400, 40));
        pathBox.setFillColor(sf::Color(50, 50, 50));
        pathBox.setOutlineThickness(2);
        pathBox.setOutlineColor(sf::Color::White);
        pathBox.setPosition(20, 120);

        // Browse Button
        browseButton.setSize(sf::Vector2f(120, 40));
        browseButton.setFillColor(sf::Color(0, 100, 200));
        browseButton.setPosition(440, 120);

        browseButtonText.setFont(font);
        browseButtonText.setString("Browse");
        browseButtonText.setCharacterSize(20);
        browseButtonText.setFillColor(sf::Color::White);
        browseButtonText.setPosition(460, 125);

        // Input Label
        inputLabel.setFont(font);
        inputLabel.setString("Enter filename:");
        inputLabel.setCharacterSize(20);
        inputLabel.setFillColor(sf::Color::White);
        inputLabel.setPosition(20, 180);

        // Input Box
        inputBox.setSize(sf::Vector2f(400, 40));
        inputBox.setFillColor(sf::Color(50, 50, 50));
        inputBox.setOutlineThickness(2);
        inputBox.setOutlineColor(sf::Color::White);
        inputBox.setPosition(20, 220);

        // Create Button
        createButton.setSize(sf::Vector2f(120, 40));
        createButton.setFillColor(sf::Color(0, 120, 0));
        createButton.setPosition(440, 220);

        createButtonText.setFont(font);
        createButtonText.setString("Create");
        createButtonText.setCharacterSize(20);
        createButtonText.setFillColor(sf::Color::White);
        createButtonText.setPosition(460, 225);

        // Status Text
        statusText.setFont(font);
        statusText.setCharacterSize(20);
        statusText.setFillColor(sf::Color::White);
        statusText.setPosition(20, 280);

        // Cursor
        cursor.setSize(sf::Vector2f(2, 30));
        cursor.setFillColor(sf::Color::White);
        showCursor = true;
        cursorBlinkTime = 0.5f;

        // Initialize current path
        currentPath = fs::current_path().string();
        folderHistory.push_back(currentPath);
        currentFolderIndex = 0;
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
                
                if (inputBox.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    isInputActive = true;
                    isPathActive = false;
                } else if (pathBox.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    isPathActive = true;
                    isInputActive = false;
                } else {
                    isInputActive = false;
                    isPathActive = false;
                }

                if (createButton.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    createFile();
                }
                else if (browseButton.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                    browseFolder();
                }
            }

            if (event.type == sf::Event::TextEntered) {
                if (isInputActive) {
                    if (event.text.unicode == '\b' && !currentInput.empty()) {
                        currentInput.pop_back();
                    }
                    else if (event.text.unicode < 128 && event.text.unicode != '\r' && event.text.unicode != '\n') {
                        currentInput += static_cast<char>(event.text.unicode);
                    }
                }
                else if (isPathActive) {
                    if (event.text.unicode == '\b' && !currentPath.empty()) {
                        currentPath.pop_back();
                    }
                    else if (event.text.unicode < 128 && event.text.unicode != '\r' && event.text.unicode != '\n') {
                        currentPath += static_cast<char>(event.text.unicode);
                    }
                }
            }
        }
    }

    string executeCommand(const string& command) {
        array<char, 128> buffer;
        string result;
        
        // Use FILE* directly instead of unique_ptr to avoid template warning
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
        
        // Remove trailing newline if present
        if (!result.empty() && result[result.length()-1] == '\n') {
            result.erase(result.length()-1);
        }
        return result;
    }

    void browseFolder() {
        try {
            // Try different file dialog commands in order of preference
            vector<string> commands = {
                "zenity --file-selection --directory --title=\"Select Folder\" 2>/dev/null",
                "kdialog --getexistingdirectory 2>/dev/null",
                "yad --file --directory --title=\"Select Folder\" 2>/dev/null",
                "qarma --file-selection --directory --title=\"Select Folder\" 2>/dev/null"
            };

            string selectedPath;
            for (const auto& cmd : commands) {
                selectedPath = executeCommand(cmd);
                if (!selectedPath.empty()) {
                    break;
                }
            }
            
            if (!selectedPath.empty()) {
                // Verify the path exists and is a directory
                if (fs::exists(selectedPath) && fs::is_directory(selectedPath)) {
                    currentPath = selectedPath;
                    // Add to history
                    folderHistory.push_back(currentPath);
                    currentFolderIndex = folderHistory.size() - 1;
                    statusText.setString("Folder selected: " + currentPath);
                    statusText.setFillColor(sf::Color::Green);
                } else {
                    statusText.setString("Selected path is not a valid directory");
                    statusText.setFillColor(sf::Color::Red);
                }
            } else {
                statusText.setString("No folder selected");
                statusText.setFillColor(sf::Color::Yellow);
            }
        } catch (const exception& e) {
            statusText.setString("Error opening file dialog: " + string(e.what()));
            statusText.setFillColor(sf::Color::Red);
        }
    }

    void createFile() {
        if (currentInput.empty()) {
            statusText.setString("Error: Please enter a filename");
            statusText.setFillColor(sf::Color::Red);
            return;
        }

        try {
            fs::path fullPath = fs::path(currentPath) / currentInput;
            ofstream file(fullPath);
            if (!file) {
                statusText.setString("Error creating file: " + currentInput);
                statusText.setFillColor(sf::Color::Red);
                return;
            }

            file << "This file was created by OS File Creator.\n";
            file.close();
            statusText.setString("File created successfully: " + fullPath.string());
            statusText.setFillColor(sf::Color::Green);
            currentInput.clear();
        } catch (const fs::filesystem_error& e) {
            statusText.setString("Error: " + string(e.what()));
            statusText.setFillColor(sf::Color::Red);
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
        window.draw(inputLabel);
        window.draw(inputBox);
        window.draw(createButton);
        window.draw(createButtonText);
        window.draw(statusText);

        // Draw path text
        sf::Text pathText;
        pathText.setFont(font);
        pathText.setString(currentPath);
        pathText.setCharacterSize(20);
        pathText.setFillColor(sf::Color::White);
        pathText.setPosition(30, 125);
        window.draw(pathText);

        // Draw input text
        sf::Text inputText;
        inputText.setFont(font);
        inputText.setString(currentInput);
        inputText.setCharacterSize(20);
        inputText.setFillColor(sf::Color::White);
        inputText.setPosition(30, 225);
        window.draw(inputText);

        // Draw cursor if input is active
        if ((isInputActive || isPathActive) && showCursor) {
            sf::Text tempText;
            tempText.setFont(font);
            tempText.setString(isInputActive ? currentInput : currentPath);
            tempText.setCharacterSize(20);
            float textWidth = tempText.getLocalBounds().width;
            cursor.setPosition(30 + textWidth, isInputActive ? 225 : 125);
            window.draw(cursor);
        }

        window.display();
    }

public:
    FileCreator() : isInputActive(false), isPathActive(false) {
        setupWindow();
        setupFont();
        setupUI();
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
    FileCreator creator;
    creator.run();
    return 0;
}
