#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

enum Player { NONE, HUMAN, COMPUTER };

class TicTacToe {
private:
    sf::RenderWindow window;
    sf::Font font;
    Player board[9];
    Player currentPlayer;
    bool gameOver;
    string resultText;

public:
    TicTacToe() : window(sf::VideoMode(600, 600), "MaqOS - Tic Tac Toe"), currentPlayer(HUMAN), gameOver(false) {
        window.setFramerateLimit(30);
        srand(time(NULL));

        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            if (!font.loadFromFile("arial.ttf")) {
                cerr << "Failed to load font!\n";
            }
        }

        for (int i = 0; i < 9; i++) board[i] = NONE;
    }

    void run() {
        while (window.isOpen()) {
            handleEvents();
            if (!gameOver && currentPlayer == COMPUTER) {
                makeComputerMove();
            }
            render();
        }
    }

private:
    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (gameOver) {
                    resetGame();
                } else if (currentPlayer == HUMAN) {
                    int x = event.mouseButton.x / 200;
                    int y = event.mouseButton.y / 200;
                    int index = y * 3 + x;

                    if (index >= 0 && index < 9 && board[index] == NONE) {
                        board[index] = HUMAN;
                        if (!checkWin(HUMAN) && !checkDraw()) {
                            currentPlayer = COMPUTER;
                        }
                    }
                }
            }
        }
    }

    void makeComputerMove() {
        sf::sleep(sf::milliseconds(400)); // slight delay for realism
        vector<int> emptyCells;
        for (int i = 0; i < 9; i++) {
            if (board[i] == NONE) emptyCells.push_back(i);
        }

        if (!emptyCells.empty()) {
            int move = emptyCells[rand() % emptyCells.size()];
            board[move] = COMPUTER;
            if (!checkWin(COMPUTER)) {
                checkDraw();
            }
        }
        if (!gameOver) currentPlayer = HUMAN;
    }

    bool checkWin(Player p) {
        int wins[8][3] = {
            {0,1,2}, {3,4,5}, {6,7,8}, // rows
            {0,3,6}, {1,4,7}, {2,5,8}, // cols
            {0,4,8}, {2,4,6}           // diagonals
        };

        for (int i = 0; i < 8; i++) {
            if (board[wins[i][0]] == p && board[wins[i][1]] == p && board[wins[i][2]] == p) {
                gameOver = true;
                resultText = (p == HUMAN) ? "You Win! Click to restart." : "Computer Wins! Click to restart.";
                return true;
            }
        }
        return false;
    }

    bool checkDraw() {
        for (int i = 0; i < 9; i++) {
            if (board[i] == NONE) return false;
        }
        gameOver = true;
        resultText = "It's a Draw! Click to restart.";
        return true;
    }

    void resetGame() {
        for (int i = 0; i < 9; i++) board[i] = NONE;
        currentPlayer = HUMAN;
        gameOver = false;
        resultText = "";
    }

    void render() {
        window.clear(sf::Color(30, 30, 40));

        // Draw grid lines
        sf::RectangleShape lineVertical(sf::Vector2f(5, 600));
        lineVertical.setFillColor(sf::Color(100, 100, 150));
        lineVertical.setPosition(197.5f, 0);
        window.draw(lineVertical);
        lineVertical.setPosition(397.5f, 0);
        window.draw(lineVertical);

        sf::RectangleShape lineHorizontal(sf::Vector2f(600, 5));
        lineHorizontal.setFillColor(sf::Color(100, 100, 150));
        lineHorizontal.setPosition(0, 197.5f);
        window.draw(lineHorizontal);
        lineHorizontal.setPosition(0, 397.5f);
        window.draw(lineHorizontal);

        // Draw X and O
        for (int i = 0; i < 9; i++) {
            int x = (i % 3) * 200 + 100;
            int y = (i / 3) * 200 + 100;

            if (board[i] == HUMAN) {
                sf::Text xText("X", font, 120);
                xText.setFillColor(sf::Color(100, 200, 255));
                sf::FloatRect bounds = xText.getLocalBounds();
                xText.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top + bounds.height / 2.0f);
                xText.setPosition(x, y);
                window.draw(xText);
            } else if (board[i] == COMPUTER) {
                sf::Text oText("O", font, 120);
                oText.setFillColor(sf::Color(255, 100, 100));
                sf::FloatRect bounds = oText.getLocalBounds();
                oText.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top + bounds.height / 2.0f);
                oText.setPosition(x, y);
                window.draw(oText);
            }
        }

        // Draw Game Over Text
        if (gameOver) {
            sf::RectangleShape overlay(sf::Vector2f(600, 600));
            overlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(overlay);

            sf::Text res(resultText, font, 36);
            res.setFillColor(sf::Color::Yellow);
            sf::FloatRect bounds = res.getLocalBounds();
            res.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top + bounds.height / 2.0f);
            res.setPosition(300, 300);
            window.draw(res);
        }

        window.display();
    }
};

int main() {
    TicTacToe game;
    game.run();
    return 0;
}

