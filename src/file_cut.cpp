#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstdlib> // For getenv

// It's good practice to put std::filesystem in a namespace alias
namespace fs = std::filesystem;

// Forward declaration if needed, or ensure FileBrowser is defined before FileManager uses it.
// In this single file structure, FileBrowser is defined first.

class FileBrowser {
private:
    sf::RenderWindow& parentWindow;
    sf::Font& font; // Assuming font is successfully loaded by FileManager and passed here
    sf::RectangleShape background;
    sf::Text titleText;
    sf::Text currentPathText;
    sf::Text statusText;
    
    std::string currentPath;
    std::vector<std::string> entriesList; // Stores display names of files/folders
    int selectedEntryIndex = -1; // Index in 'entriesList'
    int scrollStartIndex = 0;    // For scrolling, index in 'entriesList'
    bool isSelectingFolderMode;
    
    std::vector<sf::RectangleShape> entryDisplayShapes;
    std::vector<sf::Text> entryDisplayTexts;
    
    sf::RectangleShape upDirectoryButton;
    sf::Text upDirectoryButtonText;
    sf::RectangleShape selectItemButton;
    sf::Text selectItemButtonText;
    sf::RectangleShape cancelButtonDialog;
    sf::Text cancelButtonDialogText;
    
    const int MAX_VISIBLE_ENTRIES = 10;
    const float ENTRY_ITEM_HEIGHT = 30.0f;
    const float LIST_START_Y_OFFSET = 120.0f;
    const float DIALOG_WIDTH = 700.0f;
    const float DIALOG_HEIGHT = 500.0f;
    const float DIALOG_MARGIN = 20.0f;

    void centerTextInShape(sf::Text& textObj, const sf::RectangleShape& shape) {
        sf::FloatRect textBounds = textObj.getLocalBounds();
        textObj.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
        textObj.setPosition(shape.getPosition().x + shape.getSize().x / 2.0f, shape.getPosition().y + shape.getSize().y / 2.0f);
    }
    
    void setupUserInterface() {
        sf::Vector2u parentSize = parentWindow.getSize();
        float bgX = std::max(0.0f, (parentSize.x - DIALOG_WIDTH) / 2.0f);
        float bgY = std::max(0.0f, (parentSize.y - DIALOG_HEIGHT) / 2.0f);

        background.setSize(sf::Vector2f(DIALOG_WIDTH, DIALOG_HEIGHT));
        background.setPosition(bgX, bgY);
        background.setFillColor(sf::Color(40, 40, 40));
        background.setOutlineThickness(2);
        background.setOutlineColor(sf::Color(100, 100, 100));
        
        titleText.setFont(font);
        titleText.setString(isSelectingFolderMode ? "Select Folder" : "Select File");
        titleText.setCharacterSize(20);
        titleText.setFillColor(sf::Color::White);
        titleText.setPosition(background.getPosition().x + DIALOG_MARGIN, background.getPosition().y + DIALOG_MARGIN);
        
        currentPathText.setFont(font);
        currentPathText.setString("Path: " + currentPath);
        currentPathText.setCharacterSize(16);
        currentPathText.setFillColor(sf::Color::Yellow);
        currentPathText.setPosition(background.getPosition().x + DIALOG_MARGIN, background.getPosition().y + DIALOG_MARGIN + 30);
        
        statusText.setFont(font);
        statusText.setString("");
        statusText.setCharacterSize(16);
        statusText.setFillColor(sf::Color(255, 150, 150)); // Error/status color
        statusText.setPosition(background.getPosition().x + DIALOG_MARGIN, background.getPosition().y + DIALOG_MARGIN + 60);
        
        float buttonsYPos = background.getPosition().y + DIALOG_HEIGHT - DIALOG_MARGIN - 30.0f;
        float buttonWidth = 100.0f;
        float buttonHeight = 30.0f;

        upDirectoryButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        upDirectoryButton.setPosition(background.getPosition().x + DIALOG_MARGIN, buttonsYPos);
        upDirectoryButton.setFillColor(sf::Color(70, 70, 70));
        upDirectoryButtonText.setFont(font);
        upDirectoryButtonText.setString("Up");
        upDirectoryButtonText.setCharacterSize(16);
        centerTextInShape(upDirectoryButtonText, upDirectoryButton);
        
        cancelButtonDialog.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        cancelButtonDialog.setPosition(background.getPosition().x + DIALOG_WIDTH - DIALOG_MARGIN - buttonWidth, buttonsYPos);
        cancelButtonDialog.setFillColor(sf::Color(120, 70, 70));
        cancelButtonDialogText.setFont(font);
        cancelButtonDialogText.setString("Cancel");
        cancelButtonDialogText.setCharacterSize(16);
        centerTextInShape(cancelButtonDialogText, cancelButtonDialog);

        selectItemButton.setSize(sf::Vector2f(buttonWidth, buttonHeight));
        selectItemButton.setPosition(cancelButtonDialog.getPosition().x - buttonWidth - DIALOG_MARGIN / 2.0f , buttonsYPos);
        selectItemButton.setFillColor(sf::Color(70, 120, 70));
        selectItemButtonText.setFont(font);
        selectItemButtonText.setString("Select");
        selectItemButtonText.setCharacterSize(16);
        centerTextInShape(selectItemButtonText, selectItemButton);
    }
    
    void populateEntryList() {
        entriesList.clear();
        entryDisplayShapes.clear();
        entryDisplayTexts.clear();
        
        try {
            // Add ".." for parent directory, if not at root.
            fs::path currentFsPath(currentPath);
            if (currentFsPath.has_parent_path() && currentFsPath.parent_path() != currentFsPath) {
                 entriesList.push_back("..");
            }
            
            std::vector<fs::path> directories;
            std::vector<fs::path> files;

            for (const auto& entry : fs::directory_iterator(currentFsPath, fs::directory_options::skip_permission_denied)) {
                try {
                    if (entry.is_directory()) {
                        directories.push_back(entry.path());
                    } else if (entry.is_regular_file() && !isSelectingFolderMode) {
                        files.push_back(entry.path());
                    }
                } catch (const fs::filesystem_error& e) {
                    // Skip problematic entries (e.g. broken symlinks, permission issues not caught by skip_permission_denied)
                    std::cerr << "Skipping entry due to filesystem error: " << e.what() << std::endl;
                }
            }
            
            std::sort(directories.begin(), directories.end());
            std::sort(files.begin(), files.end());

            for(const auto& dirPath : directories) entriesList.push_back(dirPath.filename().string());
            for(const auto& filePath : files) entriesList.push_back(filePath.filename().string());
            
            // Create UI elements for visible entries
            float listVisualStartY = background.getPosition().y + LIST_START_Y_OFFSET;
            float listAreaItemWidth = DIALOG_WIDTH - 2 * DIALOG_MARGIN;

            int endIdx = std::min(scrollStartIndex + MAX_VISIBLE_ENTRIES, static_cast<int>(entriesList.size()));
            
            for (int i = scrollStartIndex; i < endIdx; ++i) {
                sf::RectangleShape shape;
                shape.setSize(sf::Vector2f(listAreaItemWidth, ENTRY_ITEM_HEIGHT));
                shape.setPosition(background.getPosition().x + DIALOG_MARGIN, listVisualStartY + (i - scrollStartIndex) * ENTRY_ITEM_HEIGHT);
                
                shape.setFillColor((i == selectedEntryIndex) ? sf::Color(80, 100, 140) : sf::Color(60, 60, 60));
                
                sf::Text text;
                text.setFont(font);
                
                std::string displayName = entriesList[i];
                if (entriesList[i] == "..") {
                    displayName = "[ .. Go Up ]";
                    text.setFillColor(sf::Color(180, 180, 220)); // Light blue for ".."
                } else {
                    fs::path entryFullPath = currentFsPath / entriesList[i];
                     bool isDir = false;
                    try { isDir = fs::is_directory(entryFullPath); } catch (...) {} // Graceful check

                    if (isDir) {
                        displayName = "[DIR] " + displayName;
                        text.setFillColor(sf::Color(120, 180, 255)); // Blue for directories
                    } else {
                         text.setFillColor(sf::Color::White); // White for files
                    }
                }
                
                // Truncate display name if too long
                unsigned int maxChars = static_cast<unsigned int>(listAreaItemWidth / (text.getCharacterSize() * 0.6f)); // Estimate
                if (displayName.length() > maxChars && maxChars > 3) {
                    displayName = displayName.substr(0, maxChars - 3) + "...";
                }

                text.setString(displayName);
                text.setCharacterSize(16);
                // Vertically center text in the entry shape
                sf::FloatRect textLocalBounds = text.getLocalBounds();
                text.setOrigin(textLocalBounds.left, textLocalBounds.top + textLocalBounds.height / 2.0f);
                text.setPosition(shape.getPosition().x + 10, shape.getPosition().y + ENTRY_ITEM_HEIGHT / 2.0f);
                
                entryDisplayShapes.push_back(shape);
                entryDisplayTexts.push_back(text);
            }
            statusText.setString(""); // Clear status on successful refresh
        } catch (const std::exception& e) {
            statusText.setString("Error listing dir: " + std::string(e.what()));
            entriesList.clear(); 
        }
    }
    
    void changeDirectory(const std::string& newDirPathStr) {
        try {
            fs::path newPath = fs::weakly_canonical(fs::path(newDirPathStr));
            if (fs::exists(newPath) && fs::is_directory(newPath)) {
                currentPath = newPath.string();
                currentPathText.setString("Path: " + currentPath); // Update displayed path
                selectedEntryIndex = -1; 
                scrollStartIndex = 0;
                populateEntryList();
            } else {
                statusText.setString("Invalid directory: " + newPath.string());
            }
        } catch (const std::exception& e) {
            statusText.setString("Navigate Error: " + std::string(e.what()));
        }
    }
    
    void applyButtonHoverEffect(sf::RectangleShape& button, const sf::Vector2f& mappedMousePos, sf::Color normalColor, sf::Color hoverColor) {
        button.setFillColor(button.getGlobalBounds().contains(mappedMousePos) ? hoverColor : normalColor);
    }

public:
    FileBrowser(sf::RenderWindow& targetWindow, sf::Font& appFont, bool selectFolder = false) 
        : parentWindow(targetWindow), font(appFont), isSelectingFolderMode(selectFolder) {
        try {
            // Determine a sensible starting path (e.g., user's home directory)
            #if defined(_WIN32) || defined(_WIN64)
            const char* homeDrive = getenv("HOMEDRIVE");
            const char* homePathEnv = getenv("HOMEPATH");
            if (homeDrive && homePathEnv) {
                currentPath = std::string(homeDrive) + std::string(homePathEnv);
            } else {
                 currentPath = fs::current_path().string(); // Fallback to current working directory
            }
            #else // For Linux, macOS, etc.
            const char* homeDir = getenv("HOME");
            if (homeDir) {
                currentPath = homeDir;
            } else {
                currentPath = fs::current_path().string(); // Fallback
            }
            #endif
            // Validate starting path
            if (!fs::exists(currentPath) || !fs::is_directory(currentPath)) {
                 currentPath = fs::current_path().string(); 
            }
        } catch (...) { // Catch-all for filesystem or environment variable issues
            currentPath = "."; // Absolute fallback
        }
        
        setupUserInterface();
        populateEntryList();
    }
    
    // Main loop for the file browser dialog
    std::string runModal() {
        std::string selectedPathResult = "";
        bool dialogIsRunning = true;
        
        sf::View originalView = parentWindow.getView(); // Preserve main window's view

        // The FileBrowser draws directly, so its coordinate system is pixel-based relative to the window
        // unless a specific view is set for the dialog itself. Here, we assume default view logic.

        while (dialogIsRunning && parentWindow.isOpen()) {
            sf::Event event;
            sf::Vector2f worldMousePos = parentWindow.mapPixelToCoords(sf::Mouse::getPosition(parentWindow)); // Get current mouse pos

            while (parentWindow.pollEvent(event)) {
                // Update mouse position based on event data for event-specific actions
                if (event.type == sf::Event::MouseMoved) {
                    worldMousePos = parentWindow.mapPixelToCoords({event.mouseMove.x, event.mouseMove.y});
                } else if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased) {
                    worldMousePos = parentWindow.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                }

                if (event.type == sf::Event::Closed) {
                    parentWindow.close(); // Closing dialog closes the main window
                    dialogIsRunning = false;
                    selectedPathResult = ""; 
                }
                
                if (event.type == sf::Event::Resized) { // If main window resizes, re-center dialog
                    setupUserInterface(); 
                    populateEntryList();  
                }

                if (event.type == sf::Event::MouseMoved) {
                    applyButtonHoverEffect(upDirectoryButton, worldMousePos, sf::Color(70, 70, 70), sf::Color(90, 90, 90));
                    applyButtonHoverEffect(selectItemButton, worldMousePos, sf::Color(70, 120, 70), sf::Color(90, 140, 90));
                    applyButtonHoverEffect(cancelButtonDialog, worldMousePos, sf::Color(120, 70, 70), sf::Color(140, 90, 90));
                    
                    for (size_t i = 0; i < entryDisplayShapes.size(); ++i) {
                        if (static_cast<int>(scrollStartIndex + i) == selectedEntryIndex) continue; // Skip selected
                        entryDisplayShapes[i].setFillColor(entryDisplayShapes[i].getGlobalBounds().contains(worldMousePos) ? 
                                                            sf::Color(70, 85, 120) : sf::Color(60, 60, 60));
                    }
                }
                
                if (event.type == sf::Event::MouseWheelScrolled) {
                    if (event.mouseWheelScroll.delta > 0 && scrollStartIndex > 0) { // Scroll Up
                        scrollStartIndex = std::max(0, scrollStartIndex - 1);
                    } else if (event.mouseWheelScroll.delta < 0 && scrollStartIndex + MAX_VISIBLE_ENTRIES < static_cast<int>(entriesList.size())) { // Scroll Down
                        scrollStartIndex++;
                    }
                    populateEntryList(); // Refresh displayed items
                }
                
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    bool actionTaken = false;
                    // Check entry clicks
                    for (size_t i = 0; i < entryDisplayShapes.size(); ++i) {
                        if (entryDisplayShapes[i].getGlobalBounds().contains(worldMousePos)) {
                            int newlyClickedIndex = scrollStartIndex + i;
                            actionTaken = true;

                            static sf::Clock doubleClickTimer; 
                            static int lastClickedForDbl = -1;

                            if (selectedEntryIndex == newlyClickedIndex && 
                                lastClickedForDbl == newlyClickedIndex && // Ensure same item for double click
                                doubleClickTimer.getElapsedTime().asMilliseconds() < 500) { // Double-click
                                if (selectedEntryIndex >= 0 && selectedEntryIndex < static_cast<int>(entriesList.size())) {
                                    std::string entryName = entriesList[selectedEntryIndex];
                                    if (entryName == "..") {
                                        changeDirectory(fs::path(currentPath).parent_path().string());
                                    } else {
                                        fs::path fullEntryPath = fs::path(currentPath) / entryName;
                                        if (fs::is_directory(fullEntryPath)) {
                                            changeDirectory(fullEntryPath.string());
                                        } else if (!isSelectingFolderMode) { // Is a file and we are not in folder selection mode
                                            selectedPathResult = fullEntryPath.string();
                                            dialogIsRunning = false;
                                        }
                                    }
                                }
                            } else { // Single-click
                                selectedEntryIndex = newlyClickedIndex;
                                populateEntryList(); // Redraw to show selection
                            }
                            lastClickedForDbl = selectedEntryIndex;
                            doubleClickTimer.restart();
                            break; 
                        }
                    }
                    
                    if (actionTaken) continue; // If an entry was clicked, skip button checks for this event

                    // Check button clicks
                    if (upDirectoryButton.getGlobalBounds().contains(worldMousePos)) {
                        changeDirectory(fs::path(currentPath).parent_path().string());
                    } else if (selectItemButton.getGlobalBounds().contains(worldMousePos)) {
                        if (selectedEntryIndex >= 0 && selectedEntryIndex < static_cast<int>(entriesList.size())) {
                            std::string entryName = entriesList[selectedEntryIndex];
                            fs::path fullEntryPath = fs::path(currentPath) / entryName;

                            if (entryName == "..") { // Selecting ".."
                                if (isSelectingFolderMode) { // Select current directory
                                     selectedPathResult = currentPath;
                                     dialogIsRunning = false;
                                } else { statusText.setString("Cannot select '..' as a file."); }
                            } else if (isSelectingFolderMode) { // Folder selection mode
                                if (fs::is_directory(fullEntryPath)) {
                                    selectedPathResult = fullEntryPath.string();
                                    dialogIsRunning = false;
                                } else { statusText.setString("Selection must be a folder."); }
                            } else { // File selection mode
                                if (!fs::is_directory(fullEntryPath)) { // It's a file
                                    selectedPathResult = fullEntryPath.string();
                                    dialogIsRunning = false;
                                } else { // It's a directory, navigate into it on "Select"
                                    changeDirectory(fullEntryPath.string());
                                }
                            }
                        } else if (isSelectingFolderMode) { // No specific entry selected, but in folder mode: select current path
                            selectedPathResult = currentPath;
                            dialogIsRunning = false;
                        } else { statusText.setString("No item selected."); }
                    } else if (cancelButtonDialog.getGlobalBounds().contains(worldMousePos)) {
                        selectedPathResult = "";
                        dialogIsRunning = false;
                    }
                }
                
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        selectedPathResult = ""; dialogIsRunning = false;
                    } else if (event.key.code == sf::Keyboard::Return) {
                         if (selectedEntryIndex >= 0 && selectedEntryIndex < static_cast<int>(entriesList.size())) {
                            std::string entryName = entriesList[selectedEntryIndex];
                             fs::path fullEntryPath = fs::path(currentPath) / entryName;
                             if (entryName == "..") changeDirectory(fs::path(currentPath).parent_path().string());
                             else if (fs::is_directory(fullEntryPath)) {
                                 if(isSelectingFolderMode) { selectedPathResult = fullEntryPath.string(); dialogIsRunning = false; }
                                 else changeDirectory(fullEntryPath.string());
                             } else if (!isSelectingFolderMode) { selectedPathResult = fullEntryPath.string(); dialogIsRunning = false; }
                         }
                    } else if (event.key.code == sf::Keyboard::Up) {
                        if (selectedEntryIndex > 0) selectedEntryIndex--;
                        else if (!entriesList.empty()) selectedEntryIndex = 0;
                        if (selectedEntryIndex < scrollStartIndex) scrollStartIndex = selectedEntryIndex;
                        populateEntryList();
                    } else if (event.key.code == sf::Keyboard::Down) {
                        if (!entriesList.empty() && selectedEntryIndex < static_cast<int>(entriesList.size()) - 1) selectedEntryIndex++;
                        if (selectedEntryIndex >= scrollStartIndex + MAX_VISIBLE_ENTRIES) scrollStartIndex = selectedEntryIndex - MAX_VISIBLE_ENTRIES + 1;
                        populateEntryList();
                    }
                }
            }
            
            // Drawing (Dialog takes over rendering)
            parentWindow.clear(sf::Color(20, 20, 25)); // Clear with a slightly different bg for main window
            
            parentWindow.draw(background);
            parentWindow.draw(titleText);
            parentWindow.draw(currentPathText);
            parentWindow.draw(statusText);
            
            for (size_t i = 0; i < entryDisplayShapes.size(); ++i) {
                parentWindow.draw(entryDisplayShapes[i]);
                parentWindow.draw(entryDisplayTexts[i]);
            }
            
            parentWindow.draw(upDirectoryButton); parentWindow.draw(upDirectoryButtonText);
            parentWindow.draw(selectItemButton); parentWindow.draw(selectItemButtonText);
            parentWindow.draw(cancelButtonDialog); parentWindow.draw(cancelButtonDialogText);
            
            parentWindow.display();
        }
        
        parentWindow.setView(originalView); // Restore original view of the main window
        return selectedPathResult;
    }
};

class FileManager {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Text titleText;
    sf::Text sourceTextLabel; // Changed name for clarity
    sf::Text destTextLabel;   // Changed name for clarity
    sf::Text statusTextDisplay; // Changed name for clarity
    
    sf::RectangleShape sourceBrowseButton;
    sf::RectangleShape destBrowseButton;
    sf::RectangleShape executeCutButton;
    
    sf::Text sourceBrowseButtonText;
    sf::Text destBrowseButtonText;
    sf::Text executeCutButtonText;
    
    std::string currentSourcePath;
    std::string currentDestPath;

    struct LayoutConfiguration {
        float titleYRatio = 0.05f;
        float contentStartYRatio = 0.15f;
        float itemGroupSpacingYRatio = 0.12f;
        float sideMarginRatio = 0.08f;
        float labelMaxWidthRatio = 0.45f; // Max width for path labels
        float browseButtonWidthRatio = 0.30f;
        float browseButtonHeightRatio = 0.06f;
        float mainActionButtonYRatio = 0.75f;
        float mainActionButtonWidthRatio = 0.4f;
        float mainActionButtonHeightRatio = 0.08f;
        float statusTextYRatio = 0.9f;
    } layoutConfig;
    
    bool isFullyInitialized = false;

    void centerTextInShape(sf::Text& textObj, const sf::RectangleShape& shape) {
        sf::FloatRect textBounds = textObj.getLocalBounds();
        textObj.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
        textObj.setPosition(shape.getPosition().x + shape.getSize().x / 2.0f, shape.getPosition().y + shape.getSize().y / 2.0f);
    }

    void setupStyledButton(sf::RectangleShape& button, sf::Text& text, const std::string& label,
                           float x, float y, float width, float height, unsigned int charSize) {
        button.setPosition(x, y);
        button.setSize(sf::Vector2f(width, height));
        button.setFillColor(sf::Color(50, 50, 50));
        button.setOutlineThickness(1); 
        button.setOutlineColor(sf::Color(100, 100, 100));

        text.setFont(font);
        text.setString(label);
        text.setCharacterSize(charSize);
        text.setFillColor(sf::Color::White);
        centerTextInShape(text, button);
    }
    
    void performInitialSetup() {
        isFullyInitialized = false; // Assume failure until success
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf") && 
            !font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf") &&
            !font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) { // Ensure an arial.ttf is in the execution dir or system path
            std::cerr << "FATAL ERROR: Could not load any fallback font. Application cannot start." << std::endl;
            return; 
        }
        
        window.create(sf::VideoMode(800, 600), "File Cut Manager - Dynamic UI");
        window.setFramerateLimit(60);
        
        titleText.setString("File Cut Manager");
        sourceTextLabel.setString("Source: (None)");
        destTextLabel.setString("Destination: (None)");
        statusTextDisplay.setString("Status: Ready.");

        sourceBrowseButtonText.setString("Browse Source");
        destBrowseButtonText.setString("Browse Dest. Dir");
        executeCutButtonText.setString("Cut File");
        
        isFullyInitialized = true; // All critical setup done
        applyCurrentLayout(); // Apply layout after successful initialization
    }
    
    void applyCurrentLayout() {
        if (!isFullyInitialized) return;

        sf::Vector2u windowSize = window.getSize();
        float winWidth = static_cast<float>(windowSize.x);
        float winHeight = static_cast<float>(windowSize.y);

        unsigned int titleCharSz = std::max(16u, std::min(36u, static_cast<unsigned int>(winHeight * 0.05f)));
        unsigned int labelCharSz = std::max(12u, std::min(24u, static_cast<unsigned int>(winHeight * 0.03f)));
        unsigned int btnCharSz = std::max(10u, std::min(20u, static_cast<unsigned int>(winHeight * 0.025f)));
        unsigned int statusCharSz = std::max(12u, std::min(22u, static_cast<unsigned int>(winHeight * 0.028f)));

        titleText.setFont(font);
        titleText.setCharacterSize(titleCharSz);
        sf::FloatRect titleBounds = titleText.getLocalBounds();
        titleText.setOrigin(titleBounds.left + titleBounds.width / 2.0f, titleBounds.top + titleBounds.height / 2.0f);
        titleText.setPosition(winWidth / 2.0f, winHeight * layoutConfig.titleYRatio);

        float currentY = winHeight * layoutConfig.contentStartYRatio;
        float sideMargin = winWidth * layoutConfig.sideMarginRatio;
        float usableWidth = winWidth - 2 * sideMargin;
        
        float browseBtnWidth = usableWidth * layoutConfig.browseButtonWidthRatio;
        float browseBtnHeight = winHeight * layoutConfig.browseButtonHeightRatio;
        float labelXPos = sideMargin;
        float labelMaxWidth = usableWidth * layoutConfig.labelMaxWidthRatio; 
        float browseBtnXPos = winWidth - sideMargin - browseBtnWidth;

        // Source Path Label and Browse Button
        sourceTextLabel.setFont(font);
        sourceTextLabel.setCharacterSize(labelCharSz);
        // Truncate text for sourceTextLabel if needed
        std::string srcDisplay = currentSourcePath.empty() ? "Source: (None)" : "Source: " + fs::path(currentSourcePath).filename().string();
        // Basic truncation (a better method would measure text width)
        if (srcDisplay.length() * (labelCharSz * 0.6f) > labelMaxWidth) { // Rough estimate
            int allowedChars = static_cast<int>(labelMaxWidth / (labelCharSz * 0.6f)) - 10; // -10 for "Source: ..."
            if (allowedChars < 5) allowedChars = 5;
            srcDisplay = "Source: ..." + fs::path(currentSourcePath).filename().string().substr(std::max(0, (int)fs::path(currentSourcePath).filename().string().length() - allowedChars));
        }
        sourceTextLabel.setString(srcDisplay);
        sourceTextLabel.setPosition(labelXPos, currentY + (browseBtnHeight - labelCharSz) / 2.0f);
        setupStyledButton(sourceBrowseButton, sourceBrowseButtonText, "Browse Source",
                          browseBtnXPos, currentY, browseBtnWidth, browseBtnHeight, btnCharSz);
        currentY += browseBtnHeight + (winHeight * layoutConfig.itemGroupSpacingYRatio * 0.5f);

        // Destination Path Label and Browse Button
        destTextLabel.setFont(font);
        destTextLabel.setCharacterSize(labelCharSz);
        std::string destDisplay = currentDestPath.empty() ? "Destination: (None)" : "Destination: " + fs::path(currentDestPath).filename().string();
         if (destDisplay.length() * (labelCharSz * 0.6f) > labelMaxWidth) {
            int allowedChars = static_cast<int>(labelMaxWidth / (labelCharSz * 0.6f)) - 16; // -16 for "Destination: ..."
            if (allowedChars < 5) allowedChars = 5;
            std::string destFilename = fs::path(currentDestPath).filename().string();
            if (destFilename.empty()) destFilename = fs::path(currentDestPath).root_name().string(); // For drives C:/
            destDisplay = "Destination: ..." + destFilename.substr(std::max(0, (int)destFilename.length() - allowedChars));
        }
        destTextLabel.setString(destDisplay);
        destTextLabel.setPosition(labelXPos, currentY + (browseBtnHeight - labelCharSz) / 2.0f);
        setupStyledButton(destBrowseButton, destBrowseButtonText, "Browse Dest. Dir",
                          browseBtnXPos, currentY, browseBtnWidth, browseBtnHeight, btnCharSz);
        
        // Main Action Button (Cut File)
        float mainBtnWidth = winWidth * layoutConfig.mainActionButtonWidthRatio;
        float mainBtnHeight = winHeight * layoutConfig.mainActionButtonHeightRatio;
        setupStyledButton(executeCutButton, executeCutButtonText, "Cut File",
                          (winWidth - mainBtnWidth) / 2.0f, winHeight * layoutConfig.mainActionButtonYRatio,
                          mainBtnWidth, mainBtnHeight, btnCharSz);

        // Status Text
        statusTextDisplay.setFont(font);
        statusTextDisplay.setCharacterSize(statusCharSz);
        statusTextDisplay.setPosition(sideMargin, winHeight * layoutConfig.statusTextYRatio);
    }
    
    bool executeFileCut(const std::string& sourceFile, std::string destinationFolder) {
        if (!isFullyInitialized) { statusTextDisplay.setString("Error: System not initialized."); applyCurrentLayout(); return false; }
        try {
            fs::path srcPath = fs::path(sourceFile);
            fs::path destDir = fs::path(destinationFolder);

            if (!fs::exists(srcPath)) {
                statusTextDisplay.setString("Error: Source file does not exist."); applyCurrentLayout(); return false;
            }
            if (fs::is_directory(srcPath)) {
                statusTextDisplay.setString("Error: Source cannot be a directory."); applyCurrentLayout(); return false;
            }
            if (!fs::exists(destDir) || !fs::is_directory(destDir)) {
                 statusTextDisplay.setString("Error: Destination must be an existing directory."); applyCurrentLayout(); return false;
            }
            
            fs::path finalDestPath = destDir / srcPath.filename();
            
            statusTextDisplay.setString("Moving: " + srcPath.filename().string() + "..."); applyCurrentLayout();
            
            try {
                fs::rename(srcPath, finalDestPath); // Atomic move if possible
                statusTextDisplay.setString("File cut successfully!");
                currentSourcePath.clear(); // Clear source after successful operation
                sourceTextLabel.setString("Source: (None)");
            }
            catch (const fs::filesystem_error& e) { // Often std::errc::cross_device_link
                if (e.code() == std::errc::cross_device_link || 
                    e.code().value() == EXDEV) { // EXDEV for cross-device link on POSIX
                    statusTextDisplay.setString("Cross-device: Copying then deleting..."); applyCurrentLayout();
                    fs::copy_file(srcPath, finalDestPath, fs::copy_options::overwrite_existing);
                    fs::remove(srcPath); // Delete original after successful copy
                    statusTextDisplay.setString("File cut (copy+delete) successfully!");
                    currentSourcePath.clear();
                    sourceTextLabel.setString("Source: (None)");
                } else {
                    throw; // Re-throw other filesystem errors
                }
            }
        }
        catch (const std::exception& e) {
            statusTextDisplay.setString("Error cutting file: " + std::string(e.what()));
            applyCurrentLayout(); return false;
        }
        applyCurrentLayout(); // Refresh UI
        return true;
    }
    
    void applyButtonHoverEffect(sf::RectangleShape& button, const sf::Vector2f& mappedMousePos) {
        if (!isFullyInitialized) return;
        button.setFillColor(button.getGlobalBounds().contains(mappedMousePos) ? sf::Color(80, 80, 80) : sf::Color(50, 50, 50));
    }
    
    void processEvents() {
        if (!isFullyInitialized || !window.isOpen()) return;
        
        sf::Event event;
        while (window.pollEvent(event)) {
            sf::Vector2f worldMousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window)); // General position for hover
            if (event.type == sf::Event::MouseMoved) {
                worldMousePos = window.mapPixelToCoords({event.mouseMove.x, event.mouseMove.y});
            } else if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased) {
                worldMousePos = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
            }

            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0.f, 0.f, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
                applyCurrentLayout(); 
            }
                
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    if (sourceBrowseButton.getGlobalBounds().contains(worldMousePos)) {
                        FileBrowser browser(window, font, false); 
                        std::string result = browser.runModal();
                        if (!result.empty()) {
                            if (fs::is_regular_file(result)) {
                                currentSourcePath = result;
                            } else {
                                currentSourcePath.clear();
                                statusTextDisplay.setString("Selected source is not a valid file.");
                            }
                        }
                        applyCurrentLayout(); // Update displayed path
                    }
                    else if (destBrowseButton.getGlobalBounds().contains(worldMousePos)) {
                        FileBrowser browser(window, font, true); 
                        std::string result = browser.runModal();
                        if (!result.empty()) {
                             if (fs::is_directory(result)) {
                                currentDestPath = result;
                             } else {
                                currentDestPath.clear();
                                statusTextDisplay.setString("Selected destination is not a valid folder.");
                             }
                        }
                        applyCurrentLayout(); // Update displayed path
                    }
                    else if (executeCutButton.getGlobalBounds().contains(worldMousePos)) {
                        if (!currentSourcePath.empty() && !currentDestPath.empty()) {
                            executeFileCut(currentSourcePath, currentDestPath);
                        } else {
                            statusTextDisplay.setString("Select source file and destination folder first.");
                            applyCurrentLayout();
                        }
                    }
                }
            }
            
            // Update hover for all relevant buttons continuously or on MouseMoved
            applyButtonHoverEffect(sourceBrowseButton, worldMousePos);
            applyButtonHoverEffect(destBrowseButton, worldMousePos);
            applyButtonHoverEffect(executeCutButton, worldMousePos);
        }
    }
    
    void renderFrame() {
        if (!isFullyInitialized || !window.isOpen()) return;
        window.clear(sf::Color(30, 30, 40)); 
        
        window.draw(titleText);
        window.draw(sourceTextLabel);
        window.draw(destTextLabel);
        
        window.draw(sourceBrowseButton); window.draw(sourceBrowseButtonText);
        window.draw(destBrowseButton); window.draw(destBrowseButtonText);
        window.draw(executeCutButton); window.draw(executeCutButtonText);

        window.draw(statusTextDisplay);
        
        window.display();
    }

public:
    FileManager() : isFullyInitialized(false) { // Initialize flag in constructor initializer list
        performInitialSetup();
    }
    
    void run() {
        if (!isFullyInitialized) {
             std::cerr << "FileManager cannot run due to initialization failure (likely font loading)." << std::endl;
             if(window.isOpen()) window.close(); // Ensure window is closed if created then init failed
             return;
        }
        while (window.isOpen()) {
            processEvents();
            renderFrame();
        }
    }
};

int main() {
    FileManager manager;
    manager.run();
    return 0;
}