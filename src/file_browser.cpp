#include <SFML/Graphics.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;

class FileBrowserUI {
    sf::RenderWindow& window;
    sf::Font& font;
    std::string currentPath;
    std::vector<std::string> entries;
    int selectedIndex = -1;
    int scrollStart = 0;
    const int maxVisible = 10;
    const float entryHeight = 34.f;
    const float cardWidth = 700.f;
    const float cardHeight = 500.f;
    const float margin = 24.f;
    sf::RectangleShape card;
    sf::Text title, pathText, statusText;
    sf::RectangleShape upBtn, selectBtn, cancelBtn;
    sf::Text upBtnText, selectBtnText, cancelBtnText;
    std::vector<sf::RectangleShape> entryRects;
    std::vector<sf::Text> entryTexts;

    void centerText(sf::Text& t, const sf::RectangleShape& r) {
        auto b = t.getLocalBounds();
        t.setOrigin(b.left + b.width/2, b.top + b.height/2);
        t.setPosition(r.getPosition().x + r.getSize().x/2, r.getPosition().y + r.getSize().y/2);
    }
    void refreshEntries() {
        entries.clear(); entryRects.clear(); entryTexts.clear();
        fs::path p(currentPath);
        if (p.has_parent_path() && p.parent_path() != p) entries.push_back(".. (Up)");
        std::vector<fs::path> dirs, files;
        for (auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied)) {
            try {
                if (e.is_directory()) dirs.push_back(e.path());
                else if (e.is_regular_file()) files.push_back(e.path());
            } catch (...) {}
        }
        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());
        for (auto& d : dirs) entries.push_back("[DIR] " + d.filename().string());
        for (auto& f : files) entries.push_back(f.filename().string());
        float y = card.getPosition().y + 110.f;
        float x = card.getPosition().x + margin;
        float w = cardWidth - 2*margin;
        int end = std::min(scrollStart+maxVisible, (int)entries.size());
        for (int i=scrollStart; i<end; ++i) {
            sf::RectangleShape r(sf::Vector2f(w, entryHeight));
            r.setPosition(x, y + (i-scrollStart)*entryHeight);
            r.setFillColor(i==selectedIndex ? sf::Color(80,100,160) : sf::Color(60,60,70));
            entryRects.push_back(r);
            sf::Text t(entries[i], font, 18);
            t.setFillColor(entries[i].find("[DIR]") == 0 ? sf::Color(120,180,255) : sf::Color::White);
            t.setPosition(r.getPosition().x+12, r.getPosition().y+4);
            entryTexts.push_back(t);
        }
    }
    void layout() {
        sf::Vector2u sz = window.getSize();
        float cx = (sz.x-cardWidth)/2, cy = (sz.y-cardHeight)/2;
        card.setSize(sf::Vector2f(cardWidth, cardHeight));
        card.setPosition(cx, cy);
        card.setFillColor(sf::Color(44,46,66,235));
        card.setOutlineThickness(3);
        card.setOutlineColor(sf::Color(99,102,241));
        title.setFont(font); title.setString("Browse Files"); title.setCharacterSize(22);
        title.setFillColor(sf::Color::White);
        title.setPosition(card.getPosition().x+margin, card.getPosition().y+margin);
        pathText.setFont(font); pathText.setString("Path: "+currentPath); pathText.setCharacterSize(16);
        pathText.setFillColor(sf::Color(235,235,180));
        pathText.setPosition(card.getPosition().x+margin, card.getPosition().y+margin+32);
        statusText.setFont(font); statusText.setString(""); statusText.setCharacterSize(15);
        statusText.setFillColor(sf::Color(255,150,150));
        statusText.setPosition(card.getPosition().x+margin, card.getPosition().y+margin+62);
        float by = card.getPosition().y+cardHeight-margin-38;
        float bw = 110, bh = 32;
        upBtn.setSize(sf::Vector2f(bw,bh)); upBtn.setPosition(card.getPosition().x+margin,by);
        upBtn.setFillColor(sf::Color(70,70,90));
        upBtnText.setFont(font); upBtnText.setString("Up"); upBtnText.setCharacterSize(16);
        centerText(upBtnText,upBtn);
        cancelBtn.setSize(sf::Vector2f(bw,bh)); cancelBtn.setPosition(card.getPosition().x+cardWidth-margin-bw,by);
        cancelBtn.setFillColor(sf::Color(120,70,70));
        cancelBtnText.setFont(font); cancelBtnText.setString("Cancel"); cancelBtnText.setCharacterSize(16);
        centerText(cancelBtnText,cancelBtn);
        selectBtn.setSize(sf::Vector2f(bw,bh)); selectBtn.setPosition(cancelBtn.getPosition().x-bw-margin/2,by);
        selectBtn.setFillColor(sf::Color(70,120,70));
        selectBtnText.setFont(font); selectBtnText.setString("Select"); selectBtnText.setCharacterSize(16);
        centerText(selectBtnText,selectBtn);
    }
public:
    FileBrowserUI(sf::RenderWindow& win, sf::Font& f) : window(win), font(f) {
        const char* home = getenv("HOME");
        currentPath = home ? home : ".";
        layout();
        refreshEntries();
    }
    void draw() {
        window.draw(card); window.draw(title); window.draw(pathText); window.draw(statusText);
        for (auto& r : entryRects) window.draw(r);
        for (auto& t : entryTexts) window.draw(t);
        window.draw(upBtn); window.draw(upBtnText);
        window.draw(selectBtn); window.draw(selectBtnText);
        window.draw(cancelBtn); window.draw(cancelBtnText);
    }
    void handleEvent(const sf::Event& e, bool& done, std::string& selected) {
        if (e.type == sf::Event::Resized) { layout(); refreshEntries(); }
        if (e.type == sf::Event::MouseWheelScrolled) {
            if (e.mouseWheelScroll.delta < 0 && scrollStart+maxVisible < (int)entries.size()) ++scrollStart;
            if (e.mouseWheelScroll.delta > 0 && scrollStart > 0) --scrollStart;
            refreshEntries();
        }
        if (e.type == sf::Event::MouseMoved) {
            auto mp = sf::Vector2f(e.mouseMove.x, e.mouseMove.y);
            selectedIndex = -1;
            for (int i=0; i<(int)entryRects.size(); ++i) {
                if (entryRects[i].getGlobalBounds().contains(mp)) selectedIndex = i+scrollStart;
            }
            refreshEntries();
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            auto mp = sf::Vector2f(e.mouseButton.x, e.mouseButton.y);
            if (upBtn.getGlobalBounds().contains(mp)) {
                fs::path p(currentPath);
                if (p.has_parent_path() && p.parent_path() != p) { currentPath = p.parent_path().string(); scrollStart=0; }
                layout(); refreshEntries();
            } else if (cancelBtn.getGlobalBounds().contains(mp)) {
                done = true; selected = "";
            } else if (selectBtn.getGlobalBounds().contains(mp)) {
                if (selectedIndex >= 0 && selectedIndex < (int)entries.size()) {
                    std::string sel = entries[selectedIndex];
                    if (sel == ".. (Up)") {
                        fs::path p(currentPath);
                        if (p.has_parent_path() && p.parent_path() != p) { currentPath = p.parent_path().string(); scrollStart=0; }
                        layout(); refreshEntries();
                    } else if (sel.find("[DIR] ") == 0) {
                        std::string folder = sel.substr(6);
                        currentPath = (fs::path(currentPath) / folder).string();
                        scrollStart = 0;
                        layout(); refreshEntries();
                    } else {
                        done = true; selected = (fs::path(currentPath) / sel).string();
                    }
                }
            } else {
                for (int i=0; i<(int)entryRects.size(); ++i) {
                    if (entryRects[i].getGlobalBounds().contains(mp)) {
                        selectedIndex = i+scrollStart;
                        std::string sel = entries[selectedIndex];
                        if (sel == ".. (Up)") {
                            fs::path p(currentPath);
                            if (p.has_parent_path() && p.parent_path() != p) { currentPath = p.parent_path().string(); scrollStart=0; }
                            layout(); refreshEntries();
                        } else if (sel.find("[DIR] ") == 0) {
                            std::string folder = sel.substr(6);
                            currentPath = (fs::path(currentPath) / folder).string();
                            scrollStart = 0;
                            layout(); refreshEntries();
                        } else {
                            done = true; selected = (fs::path(currentPath) / sel).string();
                        }
                        return;
                    }
                }
            }
        }
        if (e.type == sf::Event::KeyPressed) {
            if (e.key.code == sf::Keyboard::Up && selectedIndex > 0) { --selectedIndex; refreshEntries(); }
            if (e.key.code == sf::Keyboard::Down && selectedIndex+1 < (int)entries.size()) { ++selectedIndex; refreshEntries(); }
            if (e.key.code == sf::Keyboard::Enter && selectedIndex >= 0 && selectedIndex < (int)entries.size()) {
                std::string sel = entries[selectedIndex];
                if (sel == ".. (Up)") {
                    fs::path p(currentPath);
                    if (p.has_parent_path() && p.parent_path() != p) { currentPath = p.parent_path().string(); scrollStart=0; }
                    layout(); refreshEntries();
                } else if (sel.find("[DIR] ") == 0) {
                    std::string folder = sel.substr(6);
                    currentPath = (fs::path(currentPath) / folder).string();
                    scrollStart = 0;
                    layout(); refreshEntries();
                } else {
                    done = true; selected = (fs::path(currentPath) / sel).string();
                }
            }
            if (e.key.code == sf::Keyboard::Escape) { done = true; selected = ""; }
        }
    }
};

int main() {
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    int w = std::min(900, int(desktop.width*0.6f));
    int h = std::min(700, int(desktop.height*0.7f));
    sf::RenderWindow window(sf::VideoMode(w,h), "File Browser", sf::Style::Default);
    window.setFramerateLimit(60);
    sf::Font font; font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
    FileBrowserUI browser(window, font);
    bool done = false; std::string selected;
    while (window.isOpen() && !done) {
        sf::Event e;
        while (window.pollEvent(e)) browser.handleEvent(e, done, selected);
        window.clear(sf::Color(28,29,38));
        browser.draw();
        window.display();
    }
    if (!selected.empty()) {
        std::cout << "Selected: " << selected << std::endl;
    }
    return 0;
} 