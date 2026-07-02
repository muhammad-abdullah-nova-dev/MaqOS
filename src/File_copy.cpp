#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

class FileBrowser {
private:
    sf::RenderWindow& window;
    sf::Font& font;
    
    sf::RectangleShape background;
    sf::Text titleText;
    sf::Text currentPathText;
    sf::Text statusText;
    
    sf::RectangleShape upButton;
    sf::Text upButtonText;
    
    sf::RectangleShape selectButton;
    sf::Text selectButtonText;
    
    sf::RectangleShape cancelButton;
    sf::Text cancelButtonText;
    
    std::string currentPath;
    std::vector<fs::directory_entry> entries;
    std::vector<sf::Text> entryTexts; // This will store only the visible texts to draw
    
    int selectedIndex = -1;
    int scrollOffset = 0;
    int maxVisibleEntries = 10;
    
    // For scrollbar
    sf::RectangleShape scrollbar;
    sf::RectangleShape scrollThumb;
    bool isDraggingScroll = false;
    
    // For navigation
    sf::Clock doubleclickClock;
    int lastClickedIndex = -1;
    
    // UI layout properties
    float margin = 20.0f;
    float entryHeight = 30.0f;
    float buttonHeight = 40.0f;
    float buttonWidth = 120.0f;
    
public:
    FileBrowser(sf::RenderWindow& win, sf::Font& fnt) 
        : window(win), font(fnt) {
        
        // Start at the current working directory
        currentPath = fs::current_path().string();
        
        // Setup UI elements
        sf::Vector2u windowSize = window.getSize();
        float width = static_cast<float>(windowSize.x);
        float height = static_cast<float>(windowSize.y);
        
        // Set up background
        background.setSize(sf::Vector2f(width * 0.9f, height * 0.9f));
        background.setPosition(width * 0.05f, height * 0.05f);
        background.setFillColor(sf::Color(40, 40, 40));
        background.setOutlineThickness(2);
        background.setOutlineColor(sf::Color(100, 100, 100));
        
        // Setup title
        titleText.setFont(font);
        titleText.setString("File Browser");
        titleText.setCharacterSize(24);
        titleText.setFillColor(sf::Color::White);
        
        // Setup current path text
        currentPathText.setFont(font);
        currentPathText.setCharacterSize(16);
        currentPathText.setFillColor(sf::Color(200, 200, 200));
        
        // Setup status text
        statusText.setFont(font);
        statusText.setCharacterSize(16);
        statusText.setFillColor(sf::Color(180, 180, 180));
        statusText.setString("Select a file or directory");
        
        // Setup buttons
        setupButton(upButton, upButtonText, "Up", buttonWidth, buttonHeight);
        setupButton(selectButton, selectButtonText, "Select", buttonWidth, buttonHeight);
        setupButton(cancelButton, cancelButtonText, "Cancel", buttonWidth, buttonHeight);
        
        // Setup scrollbar
        scrollbar.setFillColor(sf::Color(60, 60, 60));
        scrollThumb.setFillColor(sf::Color(120, 120, 120));
        
        // Load directory contents
        refreshDirectoryContents(); // This also calls updateLayout indirectly via updateEntryPositions
        updateLayout(); // Call explicitly to ensure all elements are positioned correctly initially
    }
    
    void setupButton(sf::RectangleShape& button, sf::Text& text, const std::string& label, 
                     float width, float height) {
        button.setSize(sf::Vector2f(width, height));
        button.setFillColor(sf::Color(50, 50, 50));
        button.setOutlineThickness(1);
        button.setOutlineColor(sf::Color(100, 100, 100));
        
        text.setFont(font);
        text.setString(label);
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
    }
    
    void updateLayout() {
        sf::Vector2u windowSize = window.getSize();
        // Use the actual window size for the browser's own layout, not a percentage of it if it's meant to be a sub-window
        // However, the current constructor uses 90% of the main window for its background.
        // For this example, we'll assume it's laying out within its own allocated space (the 'background' shape).
        
        float browserWidth = background.getSize().x;
        float browserHeight = background.getSize().y;
        float browserLeft = background.getPosition().x;
        float browserTop = background.getPosition().y;
                
        // Position title
        titleText.setPosition(browserLeft + margin, browserTop + margin);
        
        // Position current path text
        currentPathText.setString("Path: " + currentPath); // Make sure this is updated if currentPath changes
        currentPathText.setPosition(browserLeft + margin, browserTop + margin + 40);
        
        // Position buttons
        float buttonsY = browserTop + browserHeight - buttonHeight - margin;
        
        upButton.setPosition(browserLeft + margin, buttonsY);
        centerTextInButton(upButtonText, upButton);
        
        selectButton.setPosition(browserLeft + browserWidth - buttonWidth * 2 - margin * 2, buttonsY);
        centerTextInButton(selectButtonText, selectButton);
        
        cancelButton.setPosition(browserLeft + browserWidth - buttonWidth - margin, buttonsY);
        centerTextInButton(cancelButtonText, cancelButton);
        
        // Position status text
        statusText.setPosition(browserLeft + margin, buttonsY - margin - statusText.getCharacterSize());
        
        // Calculate file list area
        float listStartY = browserTop + margin + 80; // Adjusted for title and path text
        float listHeight = buttonsY - margin - listStartY - (statusText.getCharacterSize() + margin); // Deduct status text space
        maxVisibleEntries = std::max(1, static_cast<int>(listHeight / entryHeight)); // Ensure at least 1
        
        // Setup scrollbar
        float scrollbarWidth = 10.0f;
        scrollbar.setSize(sf::Vector2f(scrollbarWidth, listHeight));
        scrollbar.setPosition(browserLeft + browserWidth - margin - scrollbarWidth, listStartY);
        
        updateScrollThumb();
        
        // Position file entries
        updateEntryPositions();
    }
    
    void centerTextInButton(sf::Text& text, const sf::RectangleShape& button) {
        sf::FloatRect textBounds = text.getLocalBounds();
        sf::FloatRect buttonBounds = button.getGlobalBounds(); // Use global bounds for position
        
        text.setPosition(
            buttonBounds.left + (buttonBounds.width - textBounds.width) / 2.0f - textBounds.left,
            buttonBounds.top + (buttonBounds.height - textBounds.height) / 2.0f - textBounds.top
        );
    }
    
    void updateScrollThumb() {
        if (entries.empty() || maxVisibleEntries <= 0) { // Added maxVisibleEntries check
            scrollThumb.setSize(sf::Vector2f(scrollbar.getSize().x, scrollbar.getSize().y));
            scrollThumb.setPosition(scrollbar.getPosition());
            return;
        }
        
        float totalEntries = static_cast<float>(entries.size());
        float visibleEntries = static_cast<float>(maxVisibleEntries);
        
        if (totalEntries <= visibleEntries) {
            scrollThumb.setSize(sf::Vector2f(scrollbar.getSize().x, scrollbar.getSize().y));
            scrollThumb.setPosition(scrollbar.getPosition());
        } else {
            float thumbHeightRatio = visibleEntries / totalEntries;
            float thumbHeight = thumbHeightRatio * scrollbar.getSize().y;
            thumbHeight = std::max(20.0f, thumbHeight); // Minimum thumb size
            
            scrollThumb.setSize(sf::Vector2f(scrollbar.getSize().x, thumbHeight));
            
            float scrollableHeight = scrollbar.getSize().y - thumbHeight;
            float scrollRatio = 0.0f;
            if (entries.size() - maxVisibleEntries > 0) { // Avoid division by zero
                 scrollRatio = static_cast<float>(scrollOffset) / (entries.size() - maxVisibleEntries);
            }
            float thumbY = scrollbar.getPosition().y + (scrollRatio * scrollableHeight);
            
            scrollThumb.setPosition(scrollbar.getPosition().x, thumbY);
        }
    }
    
    void updateEntryPositions() {
        entryTexts.clear();
        
        float listStartX = background.getGlobalBounds().left + margin;
        float listStartY = background.getGlobalBounds().top + margin + 80;
        float listWidth = background.getGlobalBounds().width - margin * 2 - scrollbar.getSize().x - margin;
        
        for (size_t i = 0; i < entries.size(); ++i) {
            // Only create text for visible entries
            if (static_cast<int>(i) >= scrollOffset && 
                static_cast<int>(i) < scrollOffset + maxVisibleEntries) {
                
                sf::Text entryText;
                entryText.setFont(font);
                entryText.setCharacterSize(16);
                
                std::string displayName;
                bool isParentDirLink = (i == 0 && entries[i].path().filename().empty() && entries[i].path().string() != currentPath); // Heuristic for ".."

                if (fs::is_directory(entries[i].path())) {
                    if (isParentDirLink || entries[i].path().filename().string() == "..") {
                         displayName = "[.. UP]";
                    } else {
                        displayName = "[DIR] " + entries[i].path().filename().string();
                    }
                    entryText.setFillColor(sf::Color(120, 180, 255)); 
                } else {
                    displayName = entries[i].path().filename().string();
                    entryText.setFillColor(sf::Color::White); 
                }
                
                // Truncate if necessary
                // A more robust truncation would consider character width
                unsigned int maxDisplayChars = static_cast<unsigned int>(listWidth / (entryText.getCharacterSize() * 0.5f)); // Rough estimate
                if (displayName.length() > maxDisplayChars && maxDisplayChars > 3) {
                    displayName = displayName.substr(0, maxDisplayChars - 3) + "...";
                }
                
                entryText.setString(displayName);
                
                float yPos = listStartY + (static_cast<int>(i) - scrollOffset) * entryHeight;
                entryText.setPosition(listStartX, yPos);
                
                entryTexts.push_back(entryText); // Add to visible texts
            }
        }
    }
    
    void refreshDirectoryContents() {
        entries.clear();
        // selectedIndex = -1; // Keep selectedIndex if possible, or reset it
        int oldSelectedIndex = selectedIndex;
        std::string oldSelectedPathStr;
        if(oldSelectedIndex != -1 && oldSelectedIndex < static_cast<int>(entries.size())) {
            // This condition is problematic because entries is cleared.
            // We should aim to re-select if the item still exists or select first item.
        }


        try {
            fs::path currentDir(currentPath);
            std::vector<fs::directory_entry> tempEntries;

            // Add parent directory ".." entry if not at root
            if (currentDir.has_parent_path() && currentDir.parent_path() != currentDir) {
                 // Construct a special entry for parent. fs::directory_entry needs a valid path.
                 // We can make it point to the parent path and handle its display specially.
                tempEntries.push_back(fs::directory_entry(currentDir.parent_path()));
            }
            
            for (const auto& entry : fs::directory_iterator(currentPath)) {
                tempEntries.push_back(entry);
            }
            
            std::sort(tempEntries.begin(), tempEntries.end(), 
                [&](const fs::directory_entry& a, const fs::directory_entry& b) {
                    // Special handling for ".." (parent directory link) to appear first
                    bool aIsParentLink = (a.path() == currentDir.parent_path() && currentDir.has_parent_path() && currentDir.parent_path() != currentDir);
                    bool bIsParentLink = (b.path() == currentDir.parent_path() && currentDir.has_parent_path() && currentDir.parent_path() != currentDir);

                    if (aIsParentLink && !bIsParentLink) return true;
                    if (!aIsParentLink && bIsParentLink) return false;

                    bool aIsDir = fs::is_directory(a.path());
                    bool bIsDir = fs::is_directory(b.path());
                    
                    if (aIsDir && !bIsDir) return true;
                    if (!aIsDir && bIsDir) return false;
                    
                    return a.path().filename().string() < b.path().filename().string();
                });
            
            entries = tempEntries; // Assign sorted entries

            // Attempt to re-select or select the first item
            selectedIndex = -1; 
            if (!entries.empty()) {
                selectedIndex = 0; // Default to first item
            }
                
            statusText.setString("Select a file or directory (" + std::to_string(entries.size()) + " items)");
            
        } catch (const std::exception& e) {
            statusText.setString("Error: " + std::string(e.what()));
            fs::path p(currentPath);
            if (p.has_parent_path() && p.parent_path() != p) {
                currentPath = p.parent_path().string();
                refreshDirectoryContents(); // Recursive call, be careful
            } else {
                // Cannot go further up, might be root or inaccessible
                entries.clear(); 
            }
        }
        
        scrollOffset = 0;
        updateEntryPositions();
        updateScrollThumb();
        // updateLayout(); // Not typically needed here unless path text length changes drastically
    }
    
    // In class FileBrowser
void handleEvents() {
    sf::Event event;
    
    // Removed initial mousePosView and mousePos declarations from outside the loop.
    // We will get mouse positions from event data or fresh when needed.

    while (window.pollEvent(event)) {
        sf::Vector2f worldMousePos; // Will hold the mouse position in world/view coordinates

        // It's best to get mouse coordinates from the event structure itself when available
        // as it represents the mouse position at the exact moment the event occurred.

        if (event.type == sf::Event::Closed) {
            isRunning = false;
            isCancelled = true;
        }
        
        if (event.type == sf::Event::Resized) {
             background.setSize(sf::Vector2f(static_cast<float>(event.size.width) * 0.9f, static_cast<float>(event.size.height) * 0.9f));
             background.setPosition(static_cast<float>(event.size.width) * 0.05f, static_cast<float>(event.size.height) * 0.05f);
             updateLayout(); // This will update positions of all child elements
        }

        if (event.type == sf::Event::MouseMoved) {
            sf::Vector2i pixelPos(event.mouseMove.x, event.mouseMove.y);
            worldMousePos = window.mapPixelToCoords(pixelPos);
            handleMouseMove(worldMousePos);
        }
        
        if (event.type == sf::Event::MouseButtonPressed) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            worldMousePos = window.mapPixelToCoords(pixelPos);
            handleMousePress(worldMousePos, event);
        }
        
        if (event.type == sf::Event::MouseButtonReleased) {
            sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
            worldMousePos = window.mapPixelToCoords(pixelPos);
            handleMouseRelease(worldMousePos, event);
        }
        
        if (event.type == sf::Event::MouseWheelScrolled) {
            // This event doesn't strictly need mouse position for the current scroll logic,
            // but if it did, you'd get it like this:
            // sf::Vector2i pixelPos(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
            // worldMousePos = window.mapPixelToCoords(pixelPos);
            handleMouseScroll(event);
        }

        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Up) {
                if (selectedIndex > 0) {
                    selectedIndex--;
                    if (selectedIndex < scrollOffset) {
                        scrollOffset = selectedIndex;
                    }
                    updateEntryPositions();
                    updateScrollThumb();
                }
            } else if (event.key.code == sf::Keyboard::Down) {
                if (selectedIndex < static_cast<int>(entries.size()) - 1) {
                    selectedIndex++;
                    if (selectedIndex >= scrollOffset + maxVisibleEntries) {
                        scrollOffset = selectedIndex - maxVisibleEntries + 1;
                    }
                    updateEntryPositions();
                    updateScrollThumb();
                }
            } else if (event.key.code == sf::Keyboard::Enter) {
                if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) { // Check bounds
                    if (fs::is_directory(entries[selectedIndex].path())) {
                        navigateToSelected();
                    } else {
                        handleSelect(); // Select file on Enter
                    }
                }
            } else if (event.key.code == sf::Keyboard::Backspace) {
                navigateUp();
            } else if (event.key.code == sf::Keyboard::Escape) {
                handleCancel();
            }
        }
    }
}
    
    void handleMouseMove(const sf::Vector2f& mousePos) {
        updateButtonHover(upButton, mousePos);
        updateButtonHover(selectButton, mousePos);
        updateButtonHover(cancelButton, mousePos);
        
        if (isDraggingScroll) {
            sf::FloatRect scrollbarBounds = scrollbar.getGlobalBounds();
            float thumbHeight = scrollThumb.getSize().y;
            
            float newThumbY = mousePos.y - (thumbHeight / 2.0f); // Center thumb on mouse
            newThumbY = std::max(scrollbarBounds.top, newThumbY);
            newThumbY = std::min(scrollbarBounds.top + scrollbarBounds.height - thumbHeight, newThumbY);
            scrollThumb.setPosition(scrollThumb.getPosition().x, newThumbY);
            
            float scrollableTrackHeight = scrollbarBounds.height - thumbHeight;
            if (scrollableTrackHeight > 0) { // Avoid division by zero
                float scrollRatio = (newThumbY - scrollbarBounds.top) / scrollableTrackHeight;
                int maxScrollOffset = std::max(0, static_cast<int>(entries.size()) - maxVisibleEntries);
                scrollOffset = static_cast<int>(scrollRatio * maxScrollOffset);
            } else {
                scrollOffset = 0;
            }
            
            updateEntryPositions();
        }
    }
    
    void handleMousePress(const sf::Vector2f& mousePos, const sf::Event& event) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            if (scrollThumb.getGlobalBounds().contains(mousePos)) {
                isDraggingScroll = true;
                return;
            }
             // Check scrollbar track click for jump scrolling
            if (scrollbar.getGlobalBounds().contains(mousePos) && !scrollThumb.getGlobalBounds().contains(mousePos)) {
                float thumbHeight = scrollThumb.getSize().y;
                float scrollbarTop = scrollbar.getGlobalBounds().top;
                float relativeClickY = mousePos.y - scrollbarTop;
                int direction = (relativeClickY < (scrollThumb.getPosition().y - scrollbarTop)) ? -1 : 1;
                
                int pageScrollAmount = maxVisibleEntries;
                int maxOffset = std::max(0, static_cast<int>(entries.size()) - maxVisibleEntries);
                scrollOffset = std::max(0, std::min(maxOffset, scrollOffset + direction * pageScrollAmount));

                updateEntryPositions();
                updateScrollThumb();
                return;
            }
            
            if (upButton.getGlobalBounds().contains(mousePos)) {
                navigateUp();
                return;
            }
            
            if (selectButton.getGlobalBounds().contains(mousePos)) {
                handleSelect();
                return;
            }
            
            if (cancelButton.getGlobalBounds().contains(mousePos)) {
                handleCancel();
                return;
            }
            
            sf::FloatRect bgBounds = background.getGlobalBounds();
            float listStartX = bgBounds.left + margin;
            float listStartY = bgBounds.top + margin + 80;
            float listWidth = bgBounds.width - margin * 2 - scrollbar.getSize().x - margin;
            float listClientHeight = maxVisibleEntries * entryHeight; 
            
            if (mousePos.x >= listStartX && mousePos.x <= listStartX + listWidth &&
                mousePos.y >= listStartY && mousePos.y <= listStartY + listClientHeight) { // Check within visible list area
                
                int visualIndex = static_cast<int>((mousePos.y - listStartY) / entryHeight);
                int clickedIndexInEntries = scrollOffset + visualIndex;
                
                if (clickedIndexInEntries >= 0 && clickedIndexInEntries < static_cast<int>(entries.size())) {
                    if (selectedIndex == clickedIndexInEntries && 
                        doubleclickClock.getElapsedTime().asMilliseconds() < 500) { // Double click
                        navigateToSelected(); // This will handle if it's the ".." entry too
                    } else { // Single click
                        selectedIndex = clickedIndexInEntries;
                        statusText.setString(entries[selectedIndex].path().filename().string());
                        updateEntryPositions(); // To update highlight if not drawn separately
                    }
                    
                    lastClickedIndex = selectedIndex; // For double click tracking
                    doubleclickClock.restart();
                }
            }
        }
    }
    
    void handleMouseRelease(const sf::Vector2f& mousePos, const sf::Event& event) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            isDraggingScroll = false;
        }
    }
    
    void handleMouseScroll(const sf::Event& event) {
        if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
            int delta = (event.mouseWheelScroll.delta > 0) ? -3 : 3; // Scroll 3 items at a time
            int maxOffset = std::max(0, static_cast<int>(entries.size()) - maxVisibleEntries);
            scrollOffset = std::max(0, std::min(maxOffset, scrollOffset + delta));
            
            updateEntryPositions();
            updateScrollThumb();
        }
    }
    
    void updateButtonHover(sf::RectangleShape& button, const sf::Vector2f& mousePos) {
        if (button.getGlobalBounds().contains(mousePos)) {
            button.setFillColor(sf::Color(70, 70, 70));
        } else {
            button.setFillColor(sf::Color(50, 50, 50));
        }
    }
    
    void navigateUp() {
        fs::path current(currentPath);
        if (current.has_parent_path() && current.parent_path() != current) {
            currentPath = current.parent_path().string();
            refreshDirectoryContents();
            // updateLayout(); // Not typically needed unless path text length changes drastically
        }
    }
    
    void navigateToSelected() {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            const auto& selectedEntry = entries[selectedIndex];
            
            // Check if it's the ".." (parent directory) link
            fs::path currentP(currentPath);
            if (selectedEntry.path() == currentP.parent_path() && currentP.has_parent_path() && currentP.parent_path() != currentP) {
                 navigateUp();
            } else if (fs::is_directory(selectedEntry.path())) {
                currentPath = selectedEntry.path().string();
                refreshDirectoryContents();
               // updateLayout(); 
            }
            // If it's a file, double-clicking might mean selecting it (handled by handleSelect on Enter/Button)
            // or could open it, but this browser is for selection.
        }
    }
    
    void handleSelect() {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            if (fs::is_directory(entries[selectedIndex].path())) {
                // If user clicks "Select" on a directory, navigate into it instead of closing.
                // Or, allow selecting a directory if that's the goal.
                // For now, let's assume "Select" means "this is my choice".
                 statusText.setString("Selected: " + entries[selectedIndex].path().string());
                 isRunning = false; // Signal completion
                 isCancelled = false;
            } else { // It's a file
                statusText.setString("Selected: " + entries[selectedIndex].path().string());
                isRunning = false; // Signal completion
                isCancelled = false;
            }
        } else {
            statusText.setString("Please select a file or directory first");
        }
    }
    
    void handleCancel() {
        selectedIndex = -1;
        isCancelled = true;
        isRunning = false; // Signal completion
        statusText.setString("Cancelled by user.");
    }
    
    void render() {
        // Note: FileBrowser typically draws on the window passed to it.
        // The caller (FileManager) should manage clearing and displaying the main window
        // if FileBrowser is a modal dialog.
        // If FileBrowser takes over the whole window temporarily, then it should clear and display.
        // For this structure, assuming it draws itself fully.
        // window.clear(sf::Color(30, 30, 30)); // If it's the only thing drawing

        window.draw(background);
        window.draw(titleText);
        window.draw(currentPathText);
        window.draw(statusText);
        
        // Draw highlight for selected item
        if (selectedIndex >= scrollOffset && selectedIndex < scrollOffset + maxVisibleEntries && selectedIndex < static_cast<int>(entries.size())) {
            int visualIndex = selectedIndex - scrollOffset; // Index in the visible list
            
            sf::FloatRect bgBounds = background.getGlobalBounds();
            float listStartX = bgBounds.left + margin;
            float listRenderStartY = bgBounds.top + margin + 80; // Where list items start rendering
            float listWidth = bgBounds.width - margin * 2 - scrollbar.getSize().x - margin;
            
            sf::RectangleShape highlight;
            highlight.setSize(sf::Vector2f(listWidth, entryHeight));
            highlight.setPosition(listStartX, listRenderStartY + visualIndex * entryHeight);
            highlight.setFillColor(sf::Color(70, 70, 120, 150));
            window.draw(highlight);
        }
        
        for (const auto& text : entryTexts) {
            window.draw(text);
        }
        
        window.draw(scrollbar);
        window.draw(scrollThumb);
        
        window.draw(upButton);
        window.draw(upButtonText);
        window.draw(selectButton);
        window.draw(selectButtonText);
        window.draw(cancelButton);
        window.draw(cancelButtonText);
        
        // window.display(); // Only if it's managing the whole window loop
    }
    
public: // Ensure these are accessible
    bool isRunning = true;
    bool isCancelled = false;
    
    std::string getSelectedPath() {
        if (!isCancelled && selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
             // Ensure that we don't return the path of ".." if it was the selected entry
             // and the user intended to select a directory or file.
             // The handleSelect logic should ideally prevent this, or this function should filter.
            fs::path currentP(currentPath);
            if (entries[selectedIndex].path() == currentP.parent_path() && currentP.has_parent_path() && currentP.parent_path() != currentP) {
                // If ".." was selected, it implies user wants the current directory `currentPath` to be "selected" as the outcome,
                // or the action was navigation which is handled elsewhere.
                // This depends on the desired behavior of "selecting" a "..".
                // For now, let it return the parent path.
            }
            return entries[selectedIndex].path().string();
        }
        return "";
    }
    
    void run() {
        isRunning = true;
        isCancelled = false;
        // updateLayout(); // Ensure layout is fresh before starting loop if window size changed
        
        // Store original view if this browser is a "dialog"
        sf::View originalView = window.getView();

        // Set a view for the browser that matches its background area, if it's a "sub-window"
        // For simplicity, this example assumes FileBrowser might take over drawing or uses coordinates mapped from the main window.
        // If it's a "modal" dialog, it might have its own event loop as implemented.

        while (isRunning && window.isOpen()) {
            handleEvents(); // Process its own events

            // Rendering: The FileBrowser should draw its components.
            // Clearing and displaying the window should be coordinated if other things are on screen.
            // If it's a modal dialog that takes over, it clears and displays.
            window.clear(sf::Color(20,20,20)); // Clear the portion it's responsible for or whole window
            render(); // Draw its UI
            window.display(); // Display the changes
        }
        
        // Restore original view if changed
        // window.setView(originalView);
    }
};

class FileManager {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Text titleText;
    sf::Text sourceText;
    sf::Text destText;
    sf::Text statusText;
    
    sf::RectangleShape sourceButton;
    sf::RectangleShape destButton;
    sf::RectangleShape copyButton;
    
    sf::Text sourceButtonText;
    sf::Text destButtonText;
    sf::Text copyButtonText;
    
    std::string sourcePath;
    std::string destPath;
    
    unsigned int defaultWidth = 800;
    unsigned int defaultHeight = 600;
    
    struct {
        float titleY = 0.05f;          
        float elementStartY = 0.15f;   // Adjusted for more space
        float elementSpacingY = 0.1f; 
        float sideMargin = 0.06f;      
        float buttonWidthRatio = 0.3f; // Renamed for clarity
        float buttonHeightRatio = 0.08f; // Renamed for clarity
        float textElementSpacing = 0.06f; // Spacing for text elements
    } layout;
    
    void initializeUI() {
        // Attempt to load a common system font, with fallbacks
        // For Linux:
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf") &&
            !font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf") &&
        // For Windows (common location):
            !font.loadFromFile("C:/Windows/Fonts/arial.ttf") &&
        // For macOS (common location):
            !font.loadFromFile("/System/Library/Fonts/Helvetica.ttc") && 
            !font.loadFromFile("/Library/Fonts/Arial.ttf") &&
        // Local fallback:
            !font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            std::cerr << "Error: Could not load any fallback font. Ensure arial.ttf is in the execution directory or install a system font." << std::endl;
            // Optionally, throw an exception or exit
             window.close(); // Close if font fails
             return;
        }
        
        window.create(sf::VideoMode(defaultWidth, defaultHeight), "File Copy Manager", sf::Style::Default);
        window.setFramerateLimit(60);
        
        titleText.setFont(font);
        titleText.setString("File Copy Manager");
        
        sourceText.setFont(font);
        sourceText.setString("Source: No file selected");
        
        destText.setFont(font);
        destText.setString("Destination: No directory selected"); // Changed for clarity
        
        statusText.setFont(font);
        statusText.setString("Status: Ready");
        
        updateLayout(); // Arrange UI elements
    }
    
    void updateLayout() {
        sf::Vector2u windowSize = window.getSize();
        float width = static_cast<float>(windowSize.x);
        float height = static_cast<float>(windowSize.y);
        
        unsigned int titleFontSize = static_cast<unsigned int>(height * 0.05f);
        titleFontSize = std::max(16u, std::min(36u, titleFontSize)); 
        
        unsigned int textFontSize = static_cast<unsigned int>(height * 0.03f);
        textFontSize = std::max(12u, std::min(24u, textFontSize)); 
        
        unsigned int buttonFontSize = static_cast<unsigned int>(height * 0.025f); // Slightly smaller for button text
        buttonFontSize = std::max(10u, std::min(22u, buttonFontSize));
        
        titleText.setCharacterSize(titleFontSize);
        sf::FloatRect titleBounds = titleText.getLocalBounds();
        titleText.setOrigin(titleBounds.left + titleBounds.width / 2.0f, titleBounds.top + titleBounds.height / 2.0f);
        titleText.setPosition(width / 2.0f, height * layout.titleY);
        
        float currentY = height * layout.elementStartY;
        float leftMargin = width * layout.sideMargin;
        float rightColumnX = width * (1.0f - layout.sideMargin - layout.buttonWidthRatio);


        sourceText.setCharacterSize(textFontSize);
        sourceText.setPosition(leftMargin, currentY);
        setupButton(sourceButton, sourceButtonText, "Browse Source", 
                   rightColumnX, currentY - (height*layout.buttonHeightRatio - textFontSize)/2 , width * layout.buttonWidthRatio, height * layout.buttonHeightRatio, buttonFontSize);
        currentY += height * layout.elementSpacingY;
        
        destText.setCharacterSize(textFontSize);
        destText.setPosition(leftMargin, currentY);
        setupButton(destButton, destButtonText, "Browse Dest. Dir", 
                   rightColumnX, currentY - (height*layout.buttonHeightRatio - textFontSize)/2, width * layout.buttonWidthRatio, height * layout.buttonHeightRatio, buttonFontSize);
        currentY += height * layout.elementSpacingY;
        
        statusText.setCharacterSize(textFontSize);
        statusText.setPosition(leftMargin, currentY);
        currentY += height * layout.textElementSpacing; // Space before the main action button
        
        // Copy button centered or full width
        float copyButtonWidth = width * layout.buttonWidthRatio * 1.5f; // Make it a bit wider
        setupButton(copyButton, copyButtonText, "Copy File", 
                   (width - copyButtonWidth) / 2.0f, currentY, copyButtonWidth, height * layout.buttonHeightRatio, buttonFontSize);
    }
    
    void setupButton(sf::RectangleShape& button, sf::Text& text, const std::string& label, 
                     float x, float y, float w, float h, unsigned int fontSize) {
        button.setSize(sf::Vector2f(w, h));
        button.setPosition(x, y);
        button.setFillColor(sf::Color(50, 50, 50));
        button.setOutlineThickness(1); // Thinner outline
        button.setOutlineColor(sf::Color(100, 100, 100));
        
        text.setFont(font);
        text.setString(label);
        text.setCharacterSize(fontSize);
        text.setFillColor(sf::Color::White);
        
        sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
        text.setPosition(x + w / 2.0f, y + h / 2.0f);
    }

    void updateButtonHover(sf::RectangleShape& button, const sf::Vector2f& mousePos) {
        if (button.getGlobalBounds().contains(mousePos)) {
            button.setFillColor(sf::Color(80, 80, 80)); // Brighter hover
        } else {
            button.setFillColor(sf::Color(50, 50, 50)); // Default color
        }
    }
    
    bool copyFile(const std::string& source, const std::string& destinationDir) {
        try {
            fs::path srcPath(source);
            fs::path dstDirPath(destinationDir);

            if (!fs::exists(srcPath)) {
                statusText.setString("Error: Source file does not exist.");
                return false;
            }
            if (fs::is_directory(srcPath)) {
                statusText.setString("Error: Source cannot be a directory for file copy.");
                return false;
            }
            
            if (!fs::exists(dstDirPath)) {
                if (!fs::create_directories(dstDirPath)) {
                     statusText.setString("Error: Could not create destination directory.");
                     return false;
                }
                statusText.setString("Info: Destination directory created.");
            } else if (!fs::is_directory(dstDirPath)) {
                statusText.setString("Error: Destination path is not a directory.");
                return false;
            }
            
            fs::path finalDestPath = dstDirPath / srcPath.filename();
            
            fs::copy_file(srcPath, finalDestPath, fs::copy_options::overwrite_existing);
            statusText.setString("Success: Copied to " + finalDestPath.string());
            return true;
        }
        catch (const fs::filesystem_error& e) { // More specific exception
            statusText.setString("Filesystem Error: " + std::string(e.what()));
            return false;
        }
        catch (const std::exception& e) {
            statusText.setString("Error: " + std::string(e.what()));
            return false;
        }
    }
    
    // Modified to specify if Browse for a file or directory
    std::string browsePath(const std::string& dialogTitle, bool selectDirectory = false) {
        statusText.setString("Status: Browse..."); // Update status before opening browser
        render(); // Render the status update immediately
        
        FileBrowser browser(window, font); // FileBrowser will now use the main window
        // browser.setTitle(dialogTitle); // If FileBrowser had a method to set its title
        // browser.setDirectorySelectionMode(selectDirectory); // If FileBrowser could be configured

        browser.run(); // This will block and run its own loop
        
        // After browser closes, update UI based on its result
        if (!browser.isCancelled && !browser.getSelectedPath().empty()) {
            return browser.getSelectedPath();
        }
        statusText.setString("Status: Browse cancelled or no selection.");
        return "";
    }

public:
    FileManager() {
        initializeUI();
    }

    // In class FileManager
void handleEvents() {
    sf::Event event;

    while (window.pollEvent(event)) {
        sf::Vector2f worldMousePos; // To store mapped mouse coordinates

        if (event.type == sf::Event::Closed)
            window.close();
        
        if (event.type == sf::Event::Resized) {
            sf::FloatRect visibleArea(0, 0, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
            window.setView(sf::View(visibleArea));
            updateLayout(); 
        }

        if (event.type == sf::Event::MouseMoved) {
            sf::Vector2i pixelPos(event.mouseMove.x, event.mouseMove.y);
            worldMousePos = window.mapPixelToCoords(pixelPos);
            updateButtonHover(sourceButton, worldMousePos);
            updateButtonHover(destButton, worldMousePos);
            updateButtonHover(copyButton, worldMousePos);
        }
            
        if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2i pixelPos(event.mouseButton.x, event.mouseButton.y);
                worldMousePos = window.mapPixelToCoords(pixelPos);
                
                if (sourceButton.getGlobalBounds().contains(worldMousePos)) {
                    std::string selectedPath = browsePath("Select Source File");
                    if (!selectedPath.empty()) {
                        if (fs::is_regular_file(selectedPath)) {
                            sourcePath = selectedPath;
                            // Truncate for display if too long, or ensure sourceText can handle it
                            std::string displayName = fs::path(sourcePath).filename().string();
                            if (displayName.length() > 30) displayName = displayName.substr(0, 27) + "...";
                            sourceText.setString("Source: " + displayName);
                        } else {
                            statusText.setString("Status: Please select a valid file as source.");
                        }
                    }
                    // updateLayout(); // Could be called if text changes significantly alter bounds used for layout
                }
                else if (destButton.getGlobalBounds().contains(worldMousePos)) {
                    std::string selectedPath = browsePath("Select Destination Directory", true);
                    if (!selectedPath.empty()) {
                         if (fs::is_directory(selectedPath)) {
                            destPath = selectedPath;
                            std::string displayName = fs::path(destPath).filename().string();
                            if (displayName.empty() && !fs::path(destPath).root_name().empty()){ // e.g. C:/
                                displayName = fs::path(destPath).string(); 
                            }
                            if (displayName.length() > 30) displayName = displayName.substr(0, 27) + "...";
                            destText.setString("Destination: " + displayName);
                         } else {
                            fs::path p = selectedPath;
                            if (fs::is_regular_file(p) && p.has_parent_path()){
                                destPath = p.parent_path().string();
                                std::string displayName = p.parent_path().filename().string();
                                if (displayName.length() > 30) displayName = displayName.substr(0, 27) + "...";
                                destText.setString("Destination: " + displayName);
                                statusText.setString("Info: Selected file's directory as destination.");
                            } else {
                               statusText.setString("Status: Please select a valid directory as destination.");
                            }
                         }
                    }
                    // updateLayout();
                }
                else if (copyButton.getGlobalBounds().contains(worldMousePos)) {
                    if (!sourcePath.empty() && !destPath.empty()) {
                        copyFile(sourcePath, destPath);
                    }
                    else {
                        statusText.setString("Status: Select source file and destination directory first.");
                    }
                    // updateLayout(); // Status text changed
                }
            }
        }
        // Other event types like MouseButtonReleased, MouseWheelScrolled, KeyPressed would be here if needed
    }
}

    void render() {
        window.clear(sf::Color(30, 30, 40)); // Dark blueish background

        window.draw(titleText);
        window.draw(sourceText);
        window.draw(destText);
        window.draw(statusText);

        window.draw(sourceButton);
        window.draw(sourceButtonText);
        window.draw(destButton);
        window.draw(destButtonText);
        window.draw(copyButton);
        window.draw(copyButtonText);

        window.display();
    }

    void run() {
        if (!font.getInfo().family.empty()) { // Check if font loaded successfully
            while (window.isOpen()) {
                handleEvents();
                // No separate update logic needed here for this simple manager, events drive changes.
                render();
            }
        } else {
             std::cerr << "Cannot run FileManager: Font not loaded." << std::endl;
        }
    }
};


int main() {
    FileManager manager;
    manager.run();
    return 0;
}