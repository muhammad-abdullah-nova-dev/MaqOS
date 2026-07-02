#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // For rand() and srand()
#include <ctime>   // For time()
#include <sstream> // For stringstream

// Function to center text origin
void centerOrigin(sf::Text& text) {
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top + bounds.height / 2.0f);
}

// Game states
enum class GameState {
    START_SCREEN,
    PLAYING,
    GAME_OVER
};

int main() {
    // Initialize random seed
    srand(static_cast<unsigned int>(time(0)));

    // Window settings
    unsigned int windowWidth = 800;
    unsigned int windowHeight = 600;
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "Number Guessing Game", sf::Style::Default);
    window.setFramerateLimit(60);

    // Font
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        // Try a common Linux font path as a fallback
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            std::cerr << "Error: Could not load font. Make sure 'arial.ttf' is in the executable's directory or provide a valid path." << std::endl;
            return -1;
        }
    }

    // Text elements
    sf::Text titleText("Guess the Number!", font, 50);
    titleText.setFillColor(sf::Color::White);
    centerOrigin(titleText);

    sf::Text instructionText("Enter a number between 1 and 100", font, 30);
    instructionText.setFillColor(sf::Color::White);
    centerOrigin(instructionText);

    sf::Text feedbackText("", font, 28);
    feedbackText.setFillColor(sf::Color::Yellow);
    centerOrigin(feedbackText);

    sf::Text inputPromptText("Your guess: ", font, 28);
    inputPromptText.setFillColor(sf::Color::Cyan);
    // Origin set to left, middle for alignment with input string
    sf::FloatRect inputPromptBounds = inputPromptText.getLocalBounds();
    inputPromptText.setOrigin(0, inputPromptBounds.top + inputPromptBounds.height / 2.0f);


    sf::Text currentInputText("", font, 28);
    currentInputText.setFillColor(sf::Color::White);
    // Origin set to left, middle for alignment
    sf::FloatRect currentInputBounds = currentInputText.getLocalBounds();
    currentInputText.setOrigin(0, currentInputBounds.top + currentInputBounds.height / 2.0f);


    sf::Text attemptsText("", font, 24);
    attemptsText.setFillColor(sf::Color::Green);
    centerOrigin(attemptsText);

    sf::Text gameOverText("", font, 40);
    gameOverText.setFillColor(sf::Color::Red);
    centerOrigin(gameOverText);

    sf::Text restartText("Press 'R' to Play Again, 'Esc' to Exit", font, 25);
    restartText.setFillColor(sf::Color::White);
    centerOrigin(restartText);
    
    sf::Text startScreenText("Press Enter to Start", font, 35);
    startScreenText.setFillColor(sf::Color::Cyan);
    centerOrigin(startScreenText);


    // Game variables
    int secretNumber = 0;
    int attempts = 0;
    std::string currentInputString = "";
    GameState currentState = GameState::START_SCREEN;

    // Function to reset and start a new game
    auto startGame = [&]() {
        secretNumber = rand() % 100 + 1; // Generate number between 1 and 100
        attempts = 0;
        currentInputString = "";
        feedbackText.setString("");
        attemptsText.setString("");
        currentInputText.setString("");
        currentState = GameState::PLAYING;
        instructionText.setString("Enter a number between 1 and 100");
    };

    // Function to update text positions (for responsiveness)
    auto updateTextPositions = [&]() {
        sf::Vector2u size = window.getSize();
        windowWidth = size.x;
        windowHeight = size.y;

        titleText.setPosition(windowWidth / 2.0f, windowHeight * 0.1f);
        
        if (currentState == GameState::START_SCREEN) {
            instructionText.setPosition(windowWidth / 2.0f, windowHeight * 0.4f);
            startScreenText.setPosition(windowWidth / 2.0f, windowHeight * 0.6f);
        } else if (currentState == GameState::PLAYING) {
            instructionText.setPosition(windowWidth / 2.0f, windowHeight * 0.3f);
            feedbackText.setPosition(windowWidth / 2.0f, windowHeight * 0.4f);
            
            // Position input prompt and current input text side-by-side
            // Recalculate bounds for currentInputText as its content changes
            sf::FloatRect currentInputGlobalBounds = currentInputText.getGlobalBounds();
            float totalInputWidth = inputPromptText.getGlobalBounds().width + currentInputGlobalBounds.width + 10.f; // 10.f for spacing
            
            inputPromptText.setPosition(windowWidth / 2.0f - totalInputWidth / 2.0f, windowHeight * 0.5f);
            currentInputText.setPosition(inputPromptText.getPosition().x + inputPromptText.getGlobalBounds().width + 10.f, windowHeight * 0.5f);

            attemptsText.setPosition(windowWidth / 2.0f, windowHeight * 0.6f);
        } else if (currentState == GameState::GAME_OVER) {
            gameOverText.setPosition(windowWidth / 2.0f, windowHeight * 0.4f);
            attemptsText.setPosition(windowWidth / 2.0f, windowHeight * 0.5f);
            restartText.setPosition(windowWidth / 2.0f, windowHeight * 0.65f);
        }
    };
    
    updateTextPositions(); // Initial positioning

    // Game loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::Resized) {
                // Update the view to the new size of the window
                sf::FloatRect visibleArea(0, 0, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                window.setView(sf::View(visibleArea));
                updateTextPositions(); // Re-calculate positions on resize
            }

            if (currentState == GameState::START_SCREEN) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Return) { // Corrected: Enter is sf::Keyboard::Return
                        startGame();
                        updateTextPositions();
                    } else if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }
                }
            } else if (currentState == GameState::PLAYING) {
                if (event.type == sf::Event::TextEntered) {
                    if (event.text.unicode >= '0' && event.text.unicode <= '9') {
                        if (currentInputString.length() < 3) { // Limit input length (e.g., max 3 digits for 1-100)
                           currentInputString += static_cast<char>(event.text.unicode);
                           currentInputText.setString(currentInputString);
                           updateTextPositions(); // Recalculate because input string width changes
                        }
                    } else if (event.text.unicode == 8 && !currentInputString.empty()) { // Backspace
                        currentInputString.pop_back();
                        currentInputText.setString(currentInputString);
                        updateTextPositions();
                    }
                }

                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Return && !currentInputString.empty()) { // Corrected: Enter is sf::Keyboard::Return
                        int guessedNumber = 0;
                        try {
                             // Use stringstream for robust conversion
                            std::stringstream ss(currentInputString);
                            ss >> guessedNumber;
                            if (ss.fail() || !ss.eof()) { // Check if conversion failed or if there's leftover non-numeric stuff
                                throw std::invalid_argument("Not a valid number");
                            }

                            if (guessedNumber < 1 || guessedNumber > 100) {
                                feedbackText.setString("Number must be between 1 and 100!");
                            } else {
                                attempts++;
                                attemptsText.setString("Attempts: " + std::to_string(attempts)); // Update attempts text immediately
                                if (guessedNumber == secretNumber) {
                                    feedbackText.setString("Correct!");
                                    gameOverText.setString("You found it! The number was " + std::to_string(secretNumber));
                                    // attemptsText is already set for game over
                                    currentState = GameState::GAME_OVER;
                                } else if (guessedNumber < secretNumber) {
                                    feedbackText.setString("Too low!");
                                } else {
                                    feedbackText.setString("Too high!");
                                }
                            }
                        } catch (const std::exception& e) {
                             feedbackText.setString("Invalid input. Enter numbers only.");
                        }
                        currentInputString = ""; // Clear input after guess
                        currentInputText.setString("");
                        updateTextPositions(); // Update positions after clearing input and feedback
                    } else if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }
                }
            } else if (currentState == GameState::GAME_OVER) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::R) {
                        startGame();
                        updateTextPositions();
                    } else if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }
                }
            }
        }

        // Clear screen
        window.clear(sf::Color(30, 30, 80)); // Dark blue background

        // Draw elements based on state
        window.draw(titleText);

        if (currentState == GameState::START_SCREEN) {
            window.draw(instructionText);
            window.draw(startScreenText);
        } else if (currentState == GameState::PLAYING) {
            window.draw(instructionText);
            window.draw(feedbackText);
            window.draw(inputPromptText);
            window.draw(currentInputText);
            window.draw(attemptsText);
        } else if (currentState == GameState::GAME_OVER) {
            window.draw(gameOverText);
            window.draw(attemptsText);
            window.draw(restartText);
        }

        // Display
        window.display();
    }

    return 0;
}
