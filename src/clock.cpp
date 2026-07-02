#include <SFML/Graphics.hpp>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace std;

namespace Theme {
    const sf::Color Background = sf::Color(10, 10, 15);
    const sf::Color NeonBlue = sf::Color(0, 150, 255);
    const sf::Color NeonPurple = sf::Color(150, 0, 255);
    const sf::Color TextMain = sf::Color(230, 230, 250);
}

int main() {
    sf::RenderWindow window(sf::VideoMode(500, 700), "MaqOS Precision Clock", sf::Style::Default);
    window.setFramerateLimit(60);

    sf::Font font;
    font.loadFromFile("assets/fonts/SegoeUIVF.ttf");

    sf::Clock animClock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        float t = animClock.getElapsedTime().asSeconds();
        window.clear(Theme::Background);

        // Animated Background Glow
        sf::CircleShape glow(300);
        glow.setOrigin(300, 300);
        glow.setPosition(250, 350);
        glow.setFillColor(sf::Color(Theme::NeonBlue.r, Theme::NeonBlue.g, Theme::NeonBlue.b, 15 + 10 * sin(t)));
        window.draw(glow);

        // Get Time
        auto now = chrono::system_clock::now();
        time_t now_c = chrono::system_clock::to_time_t(now);
        tm* l = localtime(&now_c);

        // Analog Clock Face (Minimalist)
        float cx = 250, cy = 250, radius = 180;
        sf::CircleShape face(radius, 100);
        face.setOrigin(radius, radius);
        face.setPosition(cx, cy);
        face.setFillColor(sf::Color(20, 20, 30, 150));
        face.setOutlineThickness(2);
        face.setOutlineColor(sf::Color(100, 100, 150, 50));
        window.draw(face);

        // Hands
        auto drawHand = [&](float angle, float length, float thickness, sf::Color color) {
            sf::RectangleShape hand(sf::Vector2f(thickness, length));
            hand.setOrigin(thickness / 2.f, length);
            hand.setPosition(cx, cy);
            hand.setRotation(angle);
            hand.setFillColor(color);
            window.draw(hand);
        };

        float sAngle = l->tm_sec * 6.f;
        float mAngle = l->tm_min * 6.f + l->tm_sec * 0.1f;
        float hAngle = l->tm_hour * 30.f + l->tm_min * 0.5f;

        drawHand(hAngle, radius * 0.5f, 6, Theme::TextMain);
        drawHand(mAngle, radius * 0.8f, 4, Theme::NeonBlue);
        drawHand(sAngle, radius * 0.9f, 2, Theme::NeonPurple);

        // Digital Time (Glass Display)
        sf::RectangleShape glass(sf::Vector2f(400, 150));
        glass.setOrigin(200, 75);
        glass.setPosition(250, 550);
        glass.setFillColor(sf::Color(255, 255, 255, 10));
        glass.setOutlineThickness(1);
        glass.setOutlineColor(sf::Color(255, 255, 255, 30));
        window.draw(glass);

        stringstream ss;
        ss << setfill('0') << setw(2) << l->tm_hour << ":" << setw(2) << l->tm_min << ":" << setw(2) << l->tm_sec;
        sf::Text digi(ss.str(), font, 64);
        digi.setFillColor(Theme::TextMain);
        digi.setStyle(sf::Text::Bold);
        sf::FloatRect dr = digi.getLocalBounds();
        digi.setOrigin(dr.left + dr.width/2.f, dr.top + dr.height/2.f);
        digi.setPosition(250, 535);
        window.draw(digi);

        // Date
        char dateBuf[64];
        strftime(dateBuf, sizeof(dateBuf), "%A, %d %B", l);
        sf::Text dateText(dateBuf, font, 20);
        dateText.setFillColor(Theme::NeonBlue);
        sf::FloatRect dtr = dateText.getLocalBounds();
        dateText.setOrigin(dtr.left + dtr.width/2.f, dtr.top + dtr.height/2.f);
        dateText.setPosition(250, 590);
        window.draw(dateText);

        window.display();
    }
    return 0;
}

