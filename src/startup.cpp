#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <ctime>

using namespace std;

// Premium MaqOS Design System
namespace Theme {
    const sf::Color Background = sf::Color(10, 10, 15);
    const sf::Color GlassPanel = sf::Color(30, 30, 45, 200);
    const sf::Color AccentBlue = sf::Color(0, 150, 255);
    const sf::Color AccentRed  = sf::Color(255, 50, 70);
    const sf::Color TextMain   = sf::Color(230, 230, 240);
    const sf::Color TextDim    = sf::Color(150, 150, 170);
}

int main(int argc, char* argv[]) {
    string ram   = (argc >= 2) ? argv[1] : "4";
    string hdd   = (argc >= 3) ? argv[2] : "256";
    string cores = (argc >= 4) ? argv[3] : "8";

    // Changed to 1280x720 to match standard screens and the main OS window
    sf::RenderWindow window(sf::VideoMode(1280, 720), "MaqOS — System Startup", sf::Style::None);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) return -1;

    // ============================================================
    // PHASE 1: Boot Splash (Animated)
    // ============================================================
    sf::Texture logoTex;
    logoTex.loadFromFile("assets/images/MaqOS.png");
    sf::Sprite logo(logoTex);
    logo.setScale(1280.f / logoTex.getSize().x, 720.f / logoTex.getSize().y);
    logo.setPosition(0, 0);

    sf::Clock splashClock;
    while (splashClock.getElapsedTime().asSeconds() < 4.5f && window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) if (event.type == sf::Event::Closed) return 0;

        float t = splashClock.getElapsedTime().asSeconds();
        float alpha = (t < 1.0f) ? t * 255 : (t > 3.5f) ? (4.5f - t) * 255 : 255;
        logo.setColor(sf::Color(255, 255, 255, alpha));

        window.clear(Theme::Background);
        window.draw(logo);

        // Progress Bar (Centered at the bottom)
        sf::RectangleShape barBg(sf::Vector2f(300, 4));
        barBg.setPosition(490, 650);
        barBg.setFillColor(sf::Color(40, 40, 50));
        window.draw(barBg);

        float progress = min(1.0f, t / 4.0f);
        sf::RectangleShape barFg(sf::Vector2f(300 * progress, 4));
        barFg.setPosition(490, 650);
        barFg.setFillColor(Theme::AccentBlue);
        window.draw(barFg);

        sf::Text loadText("Loading MaqOS Kernel...", font, 14);
        loadText.setFillColor(Theme::TextDim);
        loadText.setPosition(640 - loadText.getLocalBounds().width / 2.f, 670);
        window.draw(loadText);

        window.display();
    }

    // ============================================================
    // PHASE 2: Premium Login Screen
    // ============================================================
    sf::Texture bgTex;
    bgTex.loadFromFile("assets/images/lock.jpg");
    sf::Sprite bg(bgTex);
    bg.setScale(1280.f / bgTex.getSize().x, 720.f / bgTex.getSize().y);
    bg.setColor(sf::Color(100, 100, 100));

    string userStr = "Admin", passStr = "";
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) return 0;
            if (event.type == sf::Event::TextEntered) {
                if (event.text.unicode == '\r') goto loginDone;
                if (event.text.unicode == '\b') { if(!passStr.empty()) passStr.pop_back(); }
                else if (event.text.unicode < 128) passStr += static_cast<char>(event.text.unicode);
            }
        }

        window.clear();
        window.draw(bg);

        // Glass Panel (Centered in 1280x720)
        sf::RectangleShape panel(sf::Vector2f(400, 480));
        panel.setOrigin(200, 240);
        panel.setPosition(640, 360);
        panel.setFillColor(Theme::GlassPanel);
        panel.setOutlineThickness(1);
        panel.setOutlineColor(sf::Color(255, 255, 255, 40));
        window.draw(panel);

        sf::Texture avatarTex;
        bool hasAvatar = avatarTex.loadFromFile("assets/icons/user_avatar.png");
        if (hasAvatar) {
            sf::Sprite avatar(avatarTex);
            avatar.setOrigin(avatarTex.getSize().x / 2.f, avatarTex.getSize().y / 2.f);
            avatar.setPosition(640, 230);
            avatar.setScale(80.f / avatarTex.getSize().x, 80.f / avatarTex.getSize().y);
            window.draw(avatar);
        }

        sf::Text welcome("Welcome Back", font, 28);
        welcome.setStyle(sf::Text::Bold);
        welcome.setPosition(640 - welcome.getLocalBounds().width/2.f, 300);
        window.draw(welcome);

        sf::Text userDisp(userStr, font, 16);
        userDisp.setFillColor(Theme::TextDim);
        userDisp.setPosition(640 - userDisp.getLocalBounds().width/2.f, 340);
        window.draw(userDisp);

        sf::RectangleShape input(sf::Vector2f(320, 45));
        input.setOrigin(160, 22);
        input.setPosition(640, 420);
        input.setFillColor(sf::Color(20, 20, 30));
        input.setOutlineThickness(2);
        input.setOutlineColor(Theme::AccentBlue);
        window.draw(input);

        string hidden(passStr.length(), '*');
        sf::Text passText(hidden + "|", font, 20);
        passText.setPosition(500, 408);
        window.draw(passText);

        sf::Text hint("Press ENTER to Login", font, 12);
        hint.setFillColor(Theme::TextDim);
        hint.setPosition(640 - hint.getLocalBounds().width/2.f, 480);
        window.draw(hint);

        window.display();
    }

loginDone:
    // ============================================================
    // PHASE 3: Mode Selection
    // ============================================================
    int selection = -1;
    while (window.isOpen() && selection == -1) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) return 0;
            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f m = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                if (sf::FloatRect(220, 220, 400, 400).contains(m)) selection = 0;
                if (sf::FloatRect(660, 220, 400, 400).contains(m)) selection = 1;
            }
        }

        window.clear(Theme::Background);
        sf::Vector2f m = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        auto drawCard = [&](float x, float y, string title, string desc, sf::Color color, bool hover) {
            sf::RectangleShape c(sf::Vector2f(400, 400));
            c.setPosition(x, y);
            c.setFillColor(hover ? sf::Color(color.r, color.g, color.b, 40) : sf::Color(30, 30, 40));
            c.setOutlineThickness(hover ? 4 : 1);
            c.setOutlineColor(hover ? color : sf::Color(60, 60, 70));
            window.draw(c);

            sf::Text t(title, font, 32);
            t.setStyle(sf::Text::Bold);
            t.setPosition(x + 30, y + 30);
            t.setFillColor(hover ? color : sf::Color::White);
            window.draw(t);

            sf::Text d(desc, font, 16);
            d.setFillColor(Theme::TextDim);
            d.setPosition(x + 30, y + 90);
            window.draw(d);
            
            sf::RectangleShape btn(sf::Vector2f(340, 45));
            btn.setPosition(x + 30, y + 320);
            btn.setFillColor(hover ? color : sf::Color(50, 50, 60));
            window.draw(btn);
            
            sf::Text bt("LAUNCH MODE", font, 14);
            bt.setStyle(sf::Text::Bold);
            bt.setPosition(x + 30 + (340 - bt.getLocalBounds().width)/2.f, y + 332);
            window.draw(bt);
        };

        sf::Text q("System Mode Selection", font, 36);
        q.setPosition(640 - q.getLocalBounds().width/2.f, 80);
        window.draw(q);

        drawCard(220, 220, "User Mode", "Optimized for speed.\nRun all desktop apps.\nClean & Secure environment.", Theme::AccentBlue, sf::FloatRect(220, 220, 400, 400).contains(m));
        drawCard(660, 220, "Kernel Mode", "Full system access.\nDirect hardware control.\nAdvanced monitoring tools.", Theme::AccentRed, sf::FloatRect(660, 220, 400, 400).contains(m));

        window.display();
    }

    window.close();
    if (selection == 0) {
        system("./bin/gif_loader");
        system(("./bin/main " + ram + " " + hdd + " " + cores).c_str());
    } else if (selection == 1) {
        system("./bin/K");
    }

    return 0;
}

