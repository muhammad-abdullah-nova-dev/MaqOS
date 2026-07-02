#include <SFML/Graphics.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cmath>

using namespace std;

// Premium MaqOS Design System
namespace Theme {
    const sf::Color Background = sf::Color(15, 15, 20);
    const sf::Color GlassPanel = sf::Color(30, 30, 40, 200);
    const sf::Color Accent = sf::Color(0, 150, 255);
    const sf::Color TextMain = sf::Color::White;
    const sf::Color TextSecondary = sf::Color(150, 150, 170);
    const sf::Color BtnNormal = sf::Color(45, 45, 60);
    const sf::Color BtnHover = sf::Color(60, 60, 80);
    const sf::Color BtnAccent = sf::Color(0, 120, 215);
    const sf::Color BtnSpecial = sf::Color(200, 50, 50);
}

struct Button {
    string label;
    sf::RectangleShape shape;
    sf::Text text;
    sf::Color baseColor;
};

int main() {
    sf::RenderWindow window(sf::VideoMode(400, 650), "MaqOS Calculator", sf::Style::Default);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        cerr << "Failed to load font!\n";
        return 1;
    }

    string expression = "";
    string resultStr = "0";
    bool clearOnNext = false;

    // UI Layout
    float padding = 15;
    float btnSize = (400 - padding * 5) / 4.f;

    vector<string> labels = {
        "C", "(", ")", "/",
        "7", "8", "9", "*",
        "4", "5", "6", "-",
        "1", "2", "3", "+",
        "0", ".", "DEL", "="
    };

    vector<Button> buttons;
    for (int i = 0; i < labels.size(); i++) {
        int row = i / 4;
        int col = i % 4;
        
        Button b;
        b.label = labels[i];
        b.shape.setSize({btnSize, btnSize});
        b.shape.setPosition(padding + col * (btnSize + padding), 220 + row * (btnSize + padding));
        
        if (labels[i] == "=") b.baseColor = Theme::BtnAccent;
        else if (labels[i] == "C") b.baseColor = Theme::BtnSpecial;
        else if (isdigit(labels[i][0]) || labels[i] == ".") b.baseColor = Theme::BtnNormal;
        else b.baseColor = sf::Color(55, 55, 75);

        b.shape.setFillColor(b.baseColor);
        
        b.text.setFont(font);
        b.text.setString(labels[i]);
        b.text.setCharacterSize(24);
        b.text.setFillColor(Theme::TextMain);
        
        sf::FloatRect tr = b.text.getLocalBounds();
        b.text.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
        b.text.setPosition(b.shape.getPosition().x + btnSize/2.f, b.shape.getPosition().y + btnSize/2.f);
        
        buttons.push_back(b);
    }

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                
                for (auto& b : buttons) {
                    if (b.shape.getGlobalBounds().contains(mPos)) {
                        if (b.label == "C") {
                            expression = "";
                            resultStr = "0";
                        } else if (b.label == "DEL") {
                            if (!expression.empty()) expression.pop_back();
                        } else if (b.label == "=") {
                            // Simple parser (placeholder for complex logic)
                            try {
                                // For now, let's keep it simple as a proof of concept UI
                                resultStr = "Result shown here";
                                clearOnNext = true;
                            } catch (...) {
                                resultStr = "Error";
                            }
                        } else {
                            if (clearOnNext) {
                                expression = "";
                                clearOnNext = false;
                            }
                            expression += b.label;
                        }
                    }
                }
            }
        }

        // Hover effects
        sf::Vector2f mPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        for (auto& b : buttons) {
            if (b.shape.getGlobalBounds().contains(mPos)) {
                b.shape.setFillColor(Theme::BtnHover);
                b.shape.setScale(1.05f, 1.05f);
            } else {
                b.shape.setFillColor(b.baseColor);
                b.shape.setScale(1.f, 1.f);
            }
        }

        window.clear(Theme::Background);

        // Display Panel
        sf::RectangleShape display(sf::Vector2f(400 - padding*2, 180));
        display.setPosition(padding, padding);
        display.setFillColor(Theme::GlassPanel);
        display.setOutlineThickness(1);
        display.setOutlineColor(sf::Color(255, 255, 255, 30));
        window.draw(display);

        sf::Text exprText(expression, font, 20);
        exprText.setFillColor(Theme::TextSecondary);
        exprText.setPosition(padding * 2, 40);
        window.draw(exprText);

        sf::Text resText(resultStr, font, 48);
        resText.setFillColor(Theme::TextMain);
        resText.setStyle(sf::Text::Bold);
        resText.setPosition(400 - padding*2 - resText.getGlobalBounds().width - 10, 100);
        window.draw(resText);

        for (auto& b : buttons) {
            window.draw(b.shape);
            window.draw(b.text);
        }

        window.display();
    }

    return 0;
}

