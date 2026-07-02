#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace std;

class Stopwatch {
private:
    struct Colors {
        static const sf::Color Background;
        static const sf::Color Card;
        static const sf::Color CardShadow;
        static const sf::Color Text;
        static const sf::Color Button;
        static const sf::Color ButtonHover;
        static const sf::Color ButtonActive;
        static const sf::Color ButtonShadow;
        static const sf::Color LapTime;
        static const sf::Color LapTimeAlt;
        static const sf::Color Border;
    };

    struct Animation {
        float value;
        float target;
        float speed;
        bool active;
    };

    sf::RenderWindow window;
    sf::Font font;
    sf::Clock clock;
    sf::Time elapsedTime;
    sf::Time lastLapTime;
    bool isRunning;
    bool isPaused;
    vector<sf::Time> lapTimes;
    
    // UI Elements
    sf::Text timeText;
    sf::Text millisecondsText;
    sf::RectangleShape startButton;
    sf::RectangleShape resetButton;
    sf::RectangleShape lapButton;
    sf::Text startButtonText;
    sf::Text resetButtonText;
    sf::Text lapButtonText;
    vector<sf::Text> lapTimeTexts;
    sf::RectangleShape background;
    sf::RectangleShape timeDisplay;
    sf::RectangleShape timeDisplayGlow;
    
    // Animations
    Animation timeDisplayScale;
    Animation buttonScale;
    Animation lapTimeOpacity;
    vector<Animation> lapTimeScales;
    
    // Responsive dimensions
    float WINDOW_WIDTH;
    float WINDOW_HEIGHT;
    float BUTTON_WIDTH;
    float BUTTON_HEIGHT;
    float BUTTON_SPACING;
    float TIME_DISPLAY_HEIGHT;
    float LAP_TIME_HEIGHT;
    const int MAX_LAP_TIMES = 10;

    bool setupWindow() {
        try {
            // Get screen size
            sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
            WINDOW_WIDTH = std::min(desktop.width * 0.4f, 800.0f);  // Cap at 800px
            WINDOW_HEIGHT = std::min(desktop.height * 0.7f, 600.0f);  // Cap at 600px
            
            // Calculate responsive dimensions
            BUTTON_WIDTH = WINDOW_WIDTH * 0.25f;
            BUTTON_HEIGHT = WINDOW_HEIGHT * 0.08f;
            BUTTON_SPACING = WINDOW_WIDTH * 0.05f;
            TIME_DISPLAY_HEIGHT = WINDOW_HEIGHT * 0.25f;
            LAP_TIME_HEIGHT = WINDOW_HEIGHT * 0.05f;

            window.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Stopwatch", sf::Style::Close);
            window.setFramerateLimit(60);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creating window: " << e.what() << std::endl;
            return false;
        }
    }

    bool setupFont() {
        try {
            if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
                std::cerr << "Error loading font\n";
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading font: " << e.what() << std::endl;
            return false;
        }
    }

    void setupAnimations() {
        timeDisplayScale = {1.0f, 1.0f, 5.0f, false};
        buttonScale = {1.0f, 1.0f, 8.0f, false};
        lapTimeOpacity = {0.0f, 1.0f, 3.0f, false};
        lapTimeScales.resize(MAX_LAP_TIMES, {0.0f, 1.0f, 4.0f, false});
    }

    void setupUI() {
        try {
            // Background
            background.setSize(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
            background.setFillColor(Colors::Background);

            // Card-style time display with shadow
            timeDisplayGlow.setSize(sf::Vector2f(WINDOW_WIDTH - 40, TIME_DISPLAY_HEIGHT + 20));
            timeDisplayGlow.setPosition(14, 14);
            timeDisplayGlow.setFillColor(Colors::CardShadow);
            timeDisplayGlow.setOutlineThickness(0);

            timeDisplay.setSize(sf::Vector2f(WINDOW_WIDTH - 40, TIME_DISPLAY_HEIGHT));
            timeDisplay.setPosition(20, 20);
            timeDisplay.setFillColor(Colors::Card);
            timeDisplay.setOutlineThickness(2);
            timeDisplay.setOutlineColor(Colors::Border);

            // Time text
            timeText.setFont(font);
            timeText.setCharacterSize(WINDOW_HEIGHT * 0.12f);
            timeText.setFillColor(Colors::Text);
            timeText.setPosition(WINDOW_WIDTH * 0.1f, WINDOW_HEIGHT * 0.05f);

            // Milliseconds text
            millisecondsText.setFont(font);
            millisecondsText.setCharacterSize(WINDOW_HEIGHT * 0.06f);
            millisecondsText.setFillColor(Colors::Text);
            millisecondsText.setPosition(WINDOW_WIDTH * 0.1f, WINDOW_HEIGHT * 0.15f);

            // Buttons
            setupButton(startButton, startButtonText, "Start", 0);
            setupButton(resetButton, resetButtonText, "Reset", 1);
            setupButton(lapButton, lapButtonText, "Lap", 2);
        } catch (const std::exception& e) {
            std::cerr << "Error setting up UI: " << e.what() << std::endl;
        }
    }

    void setupButton(sf::RectangleShape& button, sf::Text& text, const string& label, int position) {
        float buttonY = TIME_DISPLAY_HEIGHT + WINDOW_HEIGHT * 0.10f;
        float buttonX = WINDOW_WIDTH * 0.1f + position * (BUTTON_WIDTH + BUTTON_SPACING);
        button.setSize(sf::Vector2f(BUTTON_WIDTH, BUTTON_HEIGHT));
        button.setPosition(buttonX, buttonY);
        button.setFillColor(Colors::Button);
        button.setOutlineThickness(0);
        // Shadow
        button.setOutlineColor(Colors::ButtonShadow);

        text.setFont(font);
        text.setString(label);
        text.setCharacterSize(WINDOW_HEIGHT * 0.045f);
        text.setFillColor(Colors::Text);
        sf::FloatRect textRect = text.getLocalBounds();
        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        text.setPosition(buttonX + BUTTON_WIDTH / 2.0f, buttonY + BUTTON_HEIGHT / 2.0f);
    }

    void updateAnimations(float deltaTime) {
        // Update time display scale
        if (timeDisplayScale.active) {
            timeDisplayScale.value += (timeDisplayScale.target - timeDisplayScale.value) * deltaTime * timeDisplayScale.speed;
            if (abs(timeDisplayScale.value - timeDisplayScale.target) < 0.01f) {
                timeDisplayScale.value = timeDisplayScale.target;
                timeDisplayScale.active = false;
            }
        }

        // Update button scale
        if (buttonScale.active) {
            buttonScale.value += (buttonScale.target - buttonScale.value) * deltaTime * buttonScale.speed;
            if (abs(buttonScale.value - buttonScale.target) < 0.01f) {
                buttonScale.value = buttonScale.target;
                buttonScale.active = false;
            }
        }

        // Update lap time opacity
        if (lapTimeOpacity.active) {
            lapTimeOpacity.value += (lapTimeOpacity.target - lapTimeOpacity.value) * deltaTime * lapTimeOpacity.speed;
            if (abs(lapTimeOpacity.value - lapTimeOpacity.target) < 0.01f) {
                lapTimeOpacity.value = lapTimeOpacity.target;
                lapTimeOpacity.active = false;
            }
        }

        // Update lap time scales
        for (auto& scale : lapTimeScales) {
            if (scale.active) {
                scale.value += (scale.target - scale.value) * deltaTime * scale.speed;
                if (abs(scale.value - scale.target) < 0.01f) {
                    scale.value = scale.target;
                    scale.active = false;
                }
            }
        }
    }

    void drawButton(sf::RectangleShape& button, sf::Text& text) {
        // Draw button at its set position, no scaling/origin
        window.draw(button);
        window.draw(text);
    }

    string formatTime(sf::Time time) {
        int totalSeconds = time.asSeconds();
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        int milliseconds = time.asMilliseconds() % 1000;

        stringstream ss;
        ss << setfill('0') << setw(2) << hours << ":"
           << setfill('0') << setw(2) << minutes << ":"
           << setfill('0') << setw(2) << seconds;
        return ss.str();
    }

    string formatMilliseconds(sf::Time time) {
        int milliseconds = time.asMilliseconds() % 1000;
        stringstream ss;
        ss << setfill('0') << setw(3) << milliseconds;
        return ss.str();
    }

    void updateTimeDisplay() {
        if (isRunning) {
            elapsedTime = clock.getElapsedTime();
        }

        timeText.setString(formatTime(elapsedTime));
        millisecondsText.setString("." + formatMilliseconds(elapsedTime));
    }

    void updateLapTimes() {
        lapTimeTexts.clear();
        float yPos = TIME_DISPLAY_HEIGHT + WINDOW_HEIGHT * 0.22f;
        bool alt = false;
        for (int i = lapTimes.size() - 1; i >= 0 && i >= (int)lapTimes.size() - MAX_LAP_TIMES; i--) {
            sf::Text lapText;
            lapText.setFont(font);
            lapText.setCharacterSize(WINDOW_HEIGHT * 0.035f);
            lapText.setFillColor(alt ? Colors::LapTimeAlt : Colors::LapTime);
            stringstream ss;
            ss << "Lap " << i + 1 << ": " << formatTime(lapTimes[i]) << "." << formatMilliseconds(lapTimes[i]);
            lapText.setString(ss.str());
            lapText.setPosition(WINDOW_WIDTH * 0.1f, yPos);
            lapTimeTexts.push_back(lapText);
            yPos += LAP_TIME_HEIGHT;
            alt = !alt;
        }
    }

    void handleButtonHover(sf::RectangleShape& button, sf::Text& text, const sf::Vector2i& mousePos) {
        if (button.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
            button.setFillColor(Colors::ButtonHover);
            button.setOutlineThickness(4);
        } else {
            button.setFillColor(Colors::Button);
            button.setOutlineThickness(0);
        }
    }

public:
    Stopwatch() : isRunning(false), isPaused(false) {
        if (!setupWindow()) {
            throw std::runtime_error("Failed to create window");
        }
        if (!setupFont()) {
            throw std::runtime_error("Failed to load font");
        }
        setupAnimations();
        setupUI();
        elapsedTime = sf::Time::Zero;
        lastLapTime = sf::Time::Zero;
    }

    void run() {
        try {
            sf::Clock deltaClock;
            while (window.isOpen()) {
                float deltaTime = deltaClock.restart().asSeconds();
                
                sf::Event event;
                while (window.pollEvent(event)) {
                    if (event.type == sf::Event::Closed) {
                        window.close();
                    }

                    if (event.type == sf::Event::MouseButtonPressed) {
                        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

                        if (startButton.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                            if (!isRunning) {
                                isRunning = true;
                                isPaused = false;
                                clock.restart();
                                startButtonText.setString("Stop");
                                timeDisplayScale.target = 1.1f;
                                timeDisplayScale.active = true;
                            } else {
                                isRunning = false;
                                isPaused = true;
                                startButtonText.setString("Start");
                                timeDisplayScale.target = 1.0f;
                                timeDisplayScale.active = true;
                            }
                        }
                        else if (resetButton.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                            isRunning = false;
                            isPaused = false;
                            elapsedTime = sf::Time::Zero;
                            lastLapTime = sf::Time::Zero;
                            lapTimes.clear();
                            startButtonText.setString("Start");
                            timeDisplayScale.target = 1.0f;
                            timeDisplayScale.active = true;
                            updateLapTimes();
                        }
                        else if (lapButton.getGlobalBounds().contains(mousePos.x, mousePos.y) && isRunning) {
                            lapTimes.push_back(elapsedTime - lastLapTime);
                            lastLapTime = elapsedTime;
                            lapTimeOpacity.target = 0.0f;
                            lapTimeOpacity.active = true;
                            updateLapTimes();
                            lapTimeOpacity.target = 1.0f;
                            lapTimeOpacity.active = true;
                        }
                    }
                }

                // Update button hover states
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                handleButtonHover(startButton, startButtonText, mousePos);
                handleButtonHover(resetButton, resetButtonText, mousePos);
                handleButtonHover(lapButton, lapButtonText, mousePos);

                updateTimeDisplay();
                updateAnimations(deltaTime);

                window.clear(Colors::Background);
                window.draw(background);
                window.draw(timeDisplayGlow);
                
                // Draw time display with scale
                timeDisplay.setScale(timeDisplayScale.value, timeDisplayScale.value);
                timeDisplay.setOrigin(timeDisplay.getSize().x / 2, timeDisplay.getSize().y / 2);
                timeDisplay.setPosition(WINDOW_WIDTH / 2, TIME_DISPLAY_HEIGHT / 2 + 20);
                window.draw(timeDisplay);
                
                // Draw time text with scale
                timeText.setScale(timeDisplayScale.value, timeDisplayScale.value);
                timeText.setOrigin(timeText.getLocalBounds().width / 2, timeText.getLocalBounds().height / 2);
                timeText.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT * 0.15f);
                window.draw(timeText);
                
                // Draw milliseconds text with scale
                millisecondsText.setScale(timeDisplayScale.value, timeDisplayScale.value);
                millisecondsText.setOrigin(millisecondsText.getLocalBounds().width / 2, millisecondsText.getLocalBounds().height / 2);
                millisecondsText.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT * 0.25f);
                window.draw(millisecondsText);

                // Draw buttons (no scaling/origin)
                drawButton(startButton, startButtonText);
                drawButton(resetButton, resetButtonText);
                drawButton(lapButton, lapButtonText);

                // Draw lap times with fade
                for (const auto& lapText : lapTimeTexts) {
                    window.draw(lapText);
                }

                window.display();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in run loop: " << e.what() << std::endl;
        }
    }
};

// Initialize static color constants
const sf::Color Stopwatch::Colors::Background = sf::Color(28, 29, 38);
const sf::Color Stopwatch::Colors::Card = sf::Color(44, 46, 66);
const sf::Color Stopwatch::Colors::CardShadow = sf::Color(20, 20, 30, 120);
const sf::Color Stopwatch::Colors::Text = sf::Color(235, 235, 245);
const sf::Color Stopwatch::Colors::Button = sf::Color(99, 102, 241);
const sf::Color Stopwatch::Colors::ButtonHover = sf::Color(129, 140, 248);
const sf::Color Stopwatch::Colors::ButtonActive = sf::Color(79, 70, 229);
const sf::Color Stopwatch::Colors::ButtonShadow = sf::Color(60, 60, 100, 120);
const sf::Color Stopwatch::Colors::LapTime = sf::Color(180, 180, 200);
const sf::Color Stopwatch::Colors::LapTimeAlt = sf::Color(120, 120, 140);
const sf::Color Stopwatch::Colors::Border = sf::Color(100, 100, 120);

int main() {
    try {
        Stopwatch stopwatch;
        stopwatch.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
