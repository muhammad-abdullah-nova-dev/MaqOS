#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

#ifdef _WIN32
#include <windows.h>
#endif

// Open URL securely and instantly
void open_url(string url) {
    if (url.find("http://") == string::npos && url.find("https://") == string::npos) {
        url = "https://" + url;
    }
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, 1); // SW_SHOWNORMAL
#else
    system(("xdg-open \"" + url + "\" &").c_str());
#endif
}

struct Shortcut {
    string name;
    string url;
    string iconPath;
    sf::Color themeColor;
};

int main(int argc, char* argv[]) {
    string process_name = "Browser";
    if (argc > 1) process_name = argv[1];

    // sf::Style::Default enables Minimize, Maximize/Fullscreen, and Close buttons
    sf::RenderWindow window(sf::VideoMode(960, 680), "MaqOS Nexus Browser", sf::Style::Default);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        cerr << "Failed to load font!\n";
        return 1;
    }

    string input_text = "";
    bool is_focused = false;

    // Shortcuts with high-quality icons
    vector<Shortcut> shortcuts = {
        {"Google",        "google.com",          "assets/icons/google_logo.png",        sf::Color(66, 133, 244)},
        {"YouTube",       "youtube.com",         "assets/icons/youtube_logo.png",       sf::Color(255, 0, 0)},
        {"GitHub",        "github.com",          "assets/icons/github_logo.png",        sf::Color(36, 41, 46)},
        {"StackOverflow", "stackoverflow.com",   "assets/icons/stackoverflow_logo.png", sf::Color(244, 128, 36)},
        {"Reddit",        "reddit.com",          "assets/icons/reddit_logo.png",        sf::Color(255, 69, 0)},
        {"ChatGPT",       "chat.openai.com",     "assets/icons/chatgpt_logo.png",       sf::Color(16, 163, 127)},
        {"Wikipedia",     "wikipedia.org",       "assets/icons/wikipedia_logo.png",     sf::Color(100, 100, 100)},
        {"LinkedIn",      "linkedin.com",        "assets/icons/linkedin_logo.png",      sf::Color(0, 119, 181)}
    };

    // Load textures
    vector<sf::Texture> shortcutTextures(shortcuts.size());
    vector<sf::Sprite> shortcutSprites(shortcuts.size());
    for (size_t i = 0; i < shortcuts.size(); i++) {
        if (shortcutTextures[i].loadFromFile(shortcuts[i].iconPath)) {
            shortcutSprites[i].setTexture(shortcutTextures[i]);
            float s = 48.0f / shortcutTextures[i].getSize().x;
            shortcutSprites[i].setScale(s, s);
        }
    }

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::Resized) {
                // Update view to maintain aspect ratio and proper coordinates
                sf::FloatRect visibleArea(0, 0, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
            }

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

                float winW = window.getSize().x;

                // Check address bar click
                float barX = winW * 0.1f;
                float barW = winW * 0.7f;
                sf::FloatRect searchBox(barX, 90, barW, 45);
                is_focused = searchBox.contains(mousePos);

                // Check Go button click
                float goX = barX + barW + 10;
                sf::FloatRect goBox(goX, 90, 80, 45);
                if (goBox.contains(mousePos) && !input_text.empty()) {
                    open_url(input_text);
                    input_text = "";
                }

                // Check shortcuts
                int cols = 4;
                float cardW = 160, cardH = 160, spacingX = 30, spacingY = 30;
                float gridW = cols * cardW + (cols - 1) * spacingX;
                float gridStartX = (winW - gridW) / 2.f;
                float gridStartY = 200;

                for (size_t i = 0; i < shortcuts.size(); i++) {
                    int row = i / cols;
                    int col = i % cols;
                    float cx = gridStartX + col * (cardW + spacingX);
                    float cy = gridStartY + row * (cardH + spacingY);
                    sf::FloatRect cardRect(cx, cy, cardW, cardH);
                    if (cardRect.contains(mousePos)) {
                        open_url(shortcuts[i].url);
                    }
                }
            }

            if (event.type == sf::Event::TextEntered && is_focused) {
                if (event.text.unicode == '\b') {
                    if (!input_text.empty()) input_text.pop_back();
                } else if (event.text.unicode == '\r') {
                    if (!input_text.empty()) {
                        open_url(input_text);
                        input_text = "";
                    }
                } else if (event.text.unicode < 128 && event.text.unicode >= 32) {
                    input_text += static_cast<char>(event.text.unicode);
                }
            }
        }

        float winW = window.getSize().x;
        float winH = window.getSize().y;

        window.clear(sf::Color(18, 18, 22));

        // Header bar
        sf::RectangleShape header(sf::Vector2f(winW, 70));
        header.setFillColor(sf::Color(25, 28, 35));
        window.draw(header);

        // Title
        sf::Text title("MaqOS Nexus Browser", font, 28);
        title.setFillColor(sf::Color(200, 210, 230));
        title.setPosition((winW - title.getLocalBounds().width) / 2.f, 18);
        window.draw(title);

        // Address Bar
        float barX = winW * 0.1f;
        float barW = winW * 0.7f;
        sf::RectangleShape searchBar(sf::Vector2f(barW, 45));
        searchBar.setPosition(barX, 90);
        searchBar.setFillColor(sf::Color(35, 38, 45));
        searchBar.setOutlineThickness(2);
        searchBar.setOutlineColor(is_focused ? sf::Color(0, 150, 255) : sf::Color(60, 65, 75));
        window.draw(searchBar);

        sf::Text typedText(input_text.empty() && !is_focused ? "Search or enter web address..." : input_text, font, 18);
        typedText.setFillColor(input_text.empty() && !is_focused ? sf::Color(120, 130, 150) : sf::Color::White);
        typedText.setPosition(barX + 15, 100);
        window.draw(typedText);

        if (is_focused) {
            float curX = barX + 15 + typedText.getLocalBounds().width + 2;
            sf::RectangleShape cursor(sf::Vector2f(2, 22));
            cursor.setPosition(curX, 101);
            cursor.setFillColor(sf::Color(0, 150, 255));
            window.draw(cursor);
        }

        // Go Button
        float goX = barX + barW + 10;
        sf::RectangleShape goBtn(sf::Vector2f(80, 45));
        goBtn.setPosition(goX, 90);
        goBtn.setFillColor(sf::Color(0, 120, 215));
        window.draw(goBtn);
        sf::Text goText("GO", font, 18);
        goText.setFillColor(sf::Color::White);
        goText.setStyle(sf::Text::Bold);
        goText.setPosition(goX + 25, 100);
        window.draw(goText);

        // Shortcuts Grid
        int cols = 4;
        float cardW = 160, cardH = 160, spacingX = 30, spacingY = 30;
        float gridW = cols * cardW + (cols - 1) * spacingX;
        float gridStartX = (winW - gridW) / 2.f;
        float gridStartY = 200;

        sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        for (size_t i = 0; i < shortcuts.size(); i++) {
            int row = i / cols;
            int col = i % cols;
            float cx = gridStartX + col * (cardW + spacingX);
            float cy = gridStartY + row * (cardH + spacingY);

            bool hover = sf::FloatRect(cx, cy, cardW, cardH).contains(mPos);

            sf::RectangleShape card(sf::Vector2f(cardW, cardH));
            card.setPosition(cx, cy);
            card.setFillColor(hover ? sf::Color(45, 50, 60) : sf::Color(30, 33, 40));
            card.setOutlineThickness(hover ? 3.0f : 1.0f);
            card.setOutlineColor(hover ? shortcuts[i].themeColor : sf::Color(55, 60, 70));
            window.draw(card);

            // Icon
            if (shortcutTextures[i].getSize().x > 0) {
                shortcutSprites[i].setPosition(cx + (cardW - 48)/2.f, cy + 30);
                window.draw(shortcutSprites[i]);
            }

            sf::Text nameText(shortcuts[i].name, font, 15);
            nameText.setFillColor(sf::Color::White);
            nameText.setPosition(cx + (cardW - nameText.getLocalBounds().width)/2.f, cy + 95);
            window.draw(nameText);

            sf::Text urlSub(shortcuts[i].url, font, 12);
            urlSub.setFillColor(sf::Color(130, 140, 160));
            urlSub.setPosition(cx + (cardW - urlSub.getLocalBounds().width)/2.f, cy + 120);
            window.draw(urlSub);
        }

        // Footer
        sf::RectangleShape footer(sf::Vector2f(winW, 30));
        footer.setPosition(0, winH - 30);
        footer.setFillColor(sf::Color(25, 28, 35));
        window.draw(footer);

        sf::Text fText("MaqOS Nexus | Powered by MaqOS Kernel", font, 12);
        fText.setFillColor(sf::Color(100, 110, 130));
        fText.setPosition(15, winH - 25);
        window.draw(fText);

        window.display();
    }

    return 0;
}

