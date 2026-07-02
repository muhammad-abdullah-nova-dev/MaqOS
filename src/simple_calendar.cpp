#include <SFML/Graphics.hpp>
#include <string>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <vector>

using namespace std;

namespace Theme {
    const sf::Color Background = sf::Color(15, 15, 20);
    const sf::Color Panel = sf::Color(30, 30, 45, 240);
    const sf::Color Accent = sf::Color(0, 150, 255);
    const sf::Color DayText = sf::Color(220, 220, 230);
    const sf::Color WeekendText = sf::Color(150, 150, 170);
    const sf::Color TodayCircle = sf::Color(0, 150, 255, 100);
}

class ModernCalendar {
private:
    int year, month, selectedDay;
    sf::Font font;

    int getDaysInMonth(int m, int y) {
        int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) return 29;
        return d[m - 1];
    }

    int getFirstDay(int m, int y) {
        tm t_in = {0, 0, 0, 1, m - 1, y - 1900};
        time_t t_temp = mktime(&t_in);
        return localtime(&t_temp)->tm_wday;
    }

public:
    ModernCalendar() {
        time_t now = time(0);
        tm* l = localtime(&now);
        year = 1900 + l->tm_year;
        month = 1 + l->tm_mon;
        selectedDay = l->tm_mday;
        font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
    }

    void run() {
        sf::RenderWindow window(sf::VideoMode(500, 600), "MaqOS Calendar", sf::Style::Default);
        window.setFramerateLimit(60);

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) window.close();
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Left) { month--; if(month<1){month=12;year--;} }
                    if (event.key.code == sf::Keyboard::Right) { month++; if(month>12){month=1;year++;} }
                }
            }

            window.clear(Theme::Background);

            // Header
            sf::Text title;
            title.setFont(font);
            string months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
            title.setString(months[month-1] + " " + to_string(year));
            title.setCharacterSize(28);
            title.setStyle(sf::Text::Bold);
            title.setPosition(40, 30);
            window.draw(title);

            // Grid
            float startX = 40, startY = 120;
            float cellSize = 60;
            string days[] = {"S", "M", "T", "W", "T", "F", "S"};
            
            for(int i=0; i<7; i++) {
                sf::Text d(days[i], font, 16);
                d.setFillColor(Theme::WeekendText);
                d.setPosition(startX + i * cellSize + 20, startY - 30);
                window.draw(d);
            }

            int first = getFirstDay(month, year);
            int total = getDaysInMonth(month, year);
            
            time_t now = time(0);
            tm* l = localtime(&now);
            int realDay = l->tm_mday;
            int realMonth = 1 + l->tm_mon;
            int realYear = 1900 + l->tm_year;

            for(int i=0; i<42; i++) {
                int r = i / 7;
                int c = i % 7;
                float px = startX + c * cellSize;
                float py = startY + r * cellSize;

                int dayNum = i - first + 1;
                if(dayNum > 0 && dayNum <= total) {
                    if(dayNum == realDay && month == realMonth && year == realYear) {
                        sf::CircleShape circ(22);
                        circ.setFillColor(Theme::TodayCircle);
                        circ.setPosition(px + 8, py + 8);
                        window.draw(circ);
                    }

                    sf::Text t(to_string(dayNum), font, 20);
                    t.setFillColor(Theme::DayText);
                    sf::FloatRect tr = t.getLocalBounds();
                    t.setOrigin(tr.left + tr.width/2.f, tr.top + tr.height/2.f);
                    t.setPosition(px + cellSize/2.f, py + cellSize/2.f);
                    window.draw(t);
                }
            }

            // Glass footer
            sf::RectangleShape footer(sf::Vector2f(500, 100));
            footer.setPosition(0, 500);
            footer.setFillColor(Theme::Panel);
            window.draw(footer);

            sf::Text hint("Use Arrow Keys to Navigate", font, 14);
            hint.setFillColor(Theme::WeekendText);
            hint.setPosition(150, 540);
            window.draw(hint);

            window.display();
        }
    }
};

int main() {
    ModernCalendar().run();
    return 0;
}
