#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

namespace Theme {
    const sf::Color Background = sf::Color(5, 5, 10);
    const sf::Color Accent = sf::Color(0, 150, 255);
    const sf::Color TextMain = sf::Color(200, 200, 220);
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1600, 1000), "MaqOS — Shutting Down", sf::Style::None);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) return -1;

    sf::Clock clock;
    float duration = 4.0f;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        float t = clock.getElapsedTime().asSeconds();
        if (t > duration) window.close();

        window.clear(Theme::Background);

        // Rotating Spinner
        float radius = 40.f;
        sf::CircleShape dot(4.f);
        dot.setFillColor(Theme::Accent);
        for (int i = 0; i < 8; i++) {
            float angle = i * (3.14159f * 2.f / 8.f) + t * 4.f;
            float dx = cos(angle) * radius;
            float dy = sin(angle) * radius;
            
            float alpha = (float)(i + 1) / 8.f * 255.f;
            dot.setFillColor(sf::Color(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, (sf::Uint8)alpha));
            dot.setPosition(800 + dx, 500 + dy);
            window.draw(dot);
        }

        sf::Text status("Shutting down MaqOS...", font, 24);
        status.setFillColor(Theme::TextMain);
        status.setPosition(800 - status.getLocalBounds().width/2.f, 580);
        window.draw(status);

        sf::Text footer("All processes terminated securely.", font, 14);
        footer.setFillColor(sf::Color(100, 100, 120));
        footer.setPosition(800 - footer.getLocalBounds().width/2.f, 620);
        window.draw(footer);

        // Global Fade-out overlay
        if (t > duration - 1.0f) {
            float fade = (t - (duration - 1.0f)) * 255;
            sf::RectangleShape overlay(sf::Vector2f(1600, 1000));
            overlay.setFillColor(sf::Color(0, 0, 0, (sf::Uint8)min(255.f, fade)));
            window.draw(overlay);
        }

        window.display();
    }

    return 0;
}

