#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

enum GameState { MENU, PLAYING, GAME_OVER };
enum Difficulty { EASY, MEDIUM, HARD };

enum CellState { HIDDEN, REVEALED, FLAGGED };

struct Cell {
    bool isMine = false;
    int adjacentMines = 0;
    CellState state = HIDDEN;
};

class Minesweeper {
private:
    sf::RenderWindow window;
    sf::Font font;
    vector<vector<Cell>> grid;
    
    int gridCols = 10;
    int gridRows = 10;
    int cellSize = 35;
    int numMines = 10;
    
    GameState state = MENU;
    bool win = false;
    int cellsRevealed = 0;

    void setupGrid() {
        grid.assign(gridCols, vector<Cell>(gridRows));
        win = false;
        cellsRevealed = 0;
        state = PLAYING;
        
        // Resize window based on grid
        window.setSize(sf::Vector2u(gridCols * cellSize, gridRows * cellSize + 60));
        sf::FloatRect visibleArea(0, 0, gridCols * cellSize, gridRows * cellSize + 60);
        window.setView(sf::View(visibleArea));
    }

    void placeMines(int firstX, int firstY) {
        int placed = 0;
        while (placed < numMines) {
            int x = rand() % gridCols;
            int y = rand() % gridRows;
            // Prevent mine on first click AND surrounding 3x3 of first click
            if (!grid[x][y].isMine && abs(x - firstX) > 1 && abs(y - firstY) > 1) {
                grid[x][y].isMine = true;
                placed++;
            }
        }

        // Calculate adjacent mines
        for (int i = 0; i < gridCols; i++) {
            for (int j = 0; j < gridRows; j++) {
                if (grid[i][j].isMine) continue;
                int count = 0;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int nx = i + dx, ny = j + dy;
                        if (nx >= 0 && nx < gridCols && ny >= 0 && ny < gridRows && grid[nx][ny].isMine) {
                            count++;
                        }
                    }
                }
                grid[i][j].adjacentMines = count;
            }
        }
    }

    void reveal(int x, int y) {
        if (x < 0 || x >= gridCols || y < 0 || y >= gridRows || grid[x][y].state != HIDDEN) return;
        
        grid[x][y].state = REVEALED;
        cellsRevealed++;

        if (grid[x][y].isMine) {
            state = GAME_OVER;
            return;
        }

        if (cellsRevealed == gridCols * gridRows - numMines) {
            state = GAME_OVER;
            win = true;
        }

        if (grid[x][y].adjacentMines == 0) {
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    reveal(x + dx, y + dy);
                }
            }
        }
    }

    sf::Color getNumberColor(int num) {
        switch(num) {
            case 1: return sf::Color(0, 0, 250); // Blue
            case 2: return sf::Color(0, 128, 0); // Green
            case 3: return sf::Color(250, 0, 0); // Red
            case 4: return sf::Color(0, 0, 128); // Dark Blue
            case 5: return sf::Color(128, 0, 0); // Maroon
            case 6: return sf::Color(0, 128, 128); // Cyan
            case 7: return sf::Color(0, 0, 0); // Black
            case 8: return sf::Color(128, 128, 128); // Gray
            default: return sf::Color::Black;
        }
    }

public:
    Minesweeper() : window(sf::VideoMode(400, 450), "MaqOS - Minesweeper") {
        window.setFramerateLimit(30);
        srand(time(NULL));

        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            if (!font.loadFromFile("arial.ttf")) {
                cerr << "Failed to load font!\n";
            }
        }
    }

    void run() {
        bool firstClick = true;

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();

                if (state == MENU) {
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        int y = event.mouseButton.y;
                        if (y >= 120 && y <= 170) {
                            gridCols = 10; gridRows = 10; numMines = 10; cellSize = 35;
                            setupGrid(); firstClick = true;
                        } else if (y >= 200 && y <= 250) {
                            gridCols = 16; gridRows = 16; numMines = 40; cellSize = 30;
                            setupGrid(); firstClick = true;
                        } else if (y >= 280 && y <= 330) {
                            gridCols = 24; gridRows = 20; numMines = 99; cellSize = 28;
                            setupGrid(); firstClick = true;
                        }
                    }
                } else if (state == PLAYING) {
                    if (event.type == sf::Event::MouseButtonPressed) {
                        int x = event.mouseButton.x / cellSize;
                        int y = (event.mouseButton.y - 60) / cellSize;

                        if (x >= 0 && x < gridCols && y >= 0 && y < gridRows) {
                            if (event.mouseButton.button == sf::Mouse::Left) {
                                if (firstClick) {
                                    placeMines(x, y);
                                    firstClick = false;
                                }
                                if (grid[x][y].state == HIDDEN) reveal(x, y);
                            } else if (event.mouseButton.button == sf::Mouse::Right) {
                                if (grid[x][y].state == HIDDEN) grid[x][y].state = FLAGGED;
                                else if (grid[x][y].state == FLAGGED) grid[x][y].state = HIDDEN;
                            }
                        }
                    }
                } else if (state == GAME_OVER) {
                    if (event.type == sf::Event::MouseButtonPressed) {
                        // Click anywhere to return to menu
                        state = MENU;
                        window.setSize(sf::Vector2u(400, 450));
                        sf::FloatRect visibleArea(0, 0, 400, 450);
                        window.setView(sf::View(visibleArea));
                    }
                }
            }

            render();
        }
    }

private:
    void render() {
        window.clear(sf::Color(192, 192, 192)); // Classic Windows Gray

        if (state == MENU) {
            sf::Text title("Minesweeper", font, 36);
            title.setFillColor(sf::Color::Black);
            title.setStyle(sf::Text::Bold);
            title.setPosition(90, 30);
            window.draw(title);

            auto drawMenuBtn = [&](float y, string text, sf::Color col) {
                sf::RectangleShape btn(sf::Vector2f(200, 50));
                btn.setPosition(100, y);
                btn.setFillColor(col);
                btn.setOutlineColor(sf::Color(100, 100, 100));
                btn.setOutlineThickness(2);
                window.draw(btn);
                
                sf::Text txt(text, font, 20);
                txt.setFillColor(sf::Color::White);
                sf::FloatRect b = txt.getLocalBounds();
                txt.setPosition(100 + 100 - b.width/2.0f, y + 25 - b.height/2.0f - 5);
                window.draw(txt);
            };

            drawMenuBtn(120, "Easy (10x10)", sf::Color(50, 150, 50));
            drawMenuBtn(200, "Medium (16x16)", sf::Color(200, 150, 0));
            drawMenuBtn(280, "Hard (24x20)", sf::Color(200, 50, 50));

        } else {
            // Top bar
            sf::RectangleShape topBar(sf::Vector2f(window.getSize().x, 60));
            topBar.setFillColor(sf::Color(160, 160, 160));
            window.draw(topBar);

            string statusMsg = "Left Click: Reveal | Right Click: Flag";
            sf::Color statusColor = sf::Color::Black;
            if (state == GAME_OVER) {
                statusMsg = win ? "YOU WIN! Click to Menu" : "GAME OVER! Click to Menu";
                statusColor = win ? sf::Color(0, 150, 0) : sf::Color(200, 0, 0);
            }
            
            sf::Text status(statusMsg, font, 18);
            status.setStyle(sf::Text::Bold);
            status.setFillColor(statusColor);
            status.setPosition(10, 15);
            window.draw(status);

            // Draw grid
            for (int i = 0; i < gridCols; i++) {
                for (int j = 0; j < gridRows; j++) {
                    float px = i * cellSize;
                    float py = j * cellSize + 60;

                    sf::RectangleShape cell(sf::Vector2f(cellSize, cellSize));
                    cell.setPosition(px, py);

                    if (grid[i][j].state == HIDDEN || grid[i][j].state == FLAGGED) {
                        cell.setFillColor(sf::Color(192, 192, 192));
                        cell.setOutlineThickness(0);
                        window.draw(cell);

                        // Pseudo-3D bevel effect for hidden tiles
                        sf::VertexArray lines(sf::Lines, 8);
                        // Top white
                        lines[0].position = sf::Vector2f(px, py); lines[0].color = sf::Color::White;
                        lines[1].position = sf::Vector2f(px + cellSize, py); lines[1].color = sf::Color::White;
                        // Left white
                        lines[2].position = sf::Vector2f(px, py); lines[2].color = sf::Color::White;
                        lines[3].position = sf::Vector2f(px, py + cellSize); lines[3].color = sf::Color::White;
                        // Bottom dark gray
                        lines[4].position = sf::Vector2f(px, py + cellSize); lines[4].color = sf::Color(128, 128, 128);
                        lines[5].position = sf::Vector2f(px + cellSize, py + cellSize); lines[5].color = sf::Color(128, 128, 128);
                        // Right dark gray
                        lines[6].position = sf::Vector2f(px + cellSize, py); lines[6].color = sf::Color(128, 128, 128);
                        lines[7].position = sf::Vector2f(px + cellSize, py + cellSize); lines[7].color = sf::Color(128, 128, 128);
                        window.draw(lines);

                        if (grid[i][j].state == FLAGGED) {
                            // Draw Flag (Red Triangle + Black Pole)
                            sf::RectangleShape pole(sf::Vector2f(2, cellSize * 0.5f));
                            pole.setFillColor(sf::Color::Black);
                            pole.setPosition(px + cellSize/2.0f, py + cellSize*0.25f);
                            window.draw(pole);

                            sf::ConvexShape flag;
                            flag.setPointCount(3);
                            flag.setPoint(0, sf::Vector2f(px + cellSize/2.0f, py + cellSize*0.25f));
                            flag.setPoint(1, sf::Vector2f(px + cellSize/2.0f, py + cellSize*0.5f));
                            flag.setPoint(2, sf::Vector2f(px + cellSize*0.2f, py + cellSize*0.375f));
                            flag.setFillColor(sf::Color::Red);
                            window.draw(flag);
                        }

                        // Show mines if game over
                        if (state == GAME_OVER && !win && grid[i][j].isMine && grid[i][j].state != FLAGGED) {
                            sf::CircleShape mine(cellSize * 0.3f);
                            mine.setFillColor(sf::Color::Black);
                            mine.setPosition(px + cellSize*0.2f, py + cellSize*0.2f);
                            window.draw(mine);
                        }
                    } else { // REVEALED
                        cell.setFillColor(sf::Color(220, 220, 220));
                        cell.setOutlineColor(sf::Color(128, 128, 128));
                        cell.setOutlineThickness(1);
                        window.draw(cell);

                        if (grid[i][j].isMine) {
                            // Hit Mine (Red background)
                            cell.setFillColor(sf::Color::Red);
                            window.draw(cell);
                            
                            sf::CircleShape mine(cellSize * 0.3f);
                            mine.setFillColor(sf::Color::Black);
                            mine.setPosition(px + cellSize*0.2f, py + cellSize*0.2f);
                            window.draw(mine);
                        } else if (grid[i][j].adjacentMines > 0) {
                            sf::Text num(to_string(grid[i][j].adjacentMines), font, cellSize * 0.7f);
                            num.setFillColor(getNumberColor(grid[i][j].adjacentMines));
                            sf::FloatRect b = num.getLocalBounds();
                            num.setOrigin(b.left + b.width/2.0f, b.top + b.height/2.0f);
                            num.setPosition(px + cellSize/2.0f, py + cellSize/2.0f);
                            window.draw(num);
                        }
                    }
                }
            }
        }

        window.display();
    }
};

int main() {
    Minesweeper game;
    game.run();
    return 0;
}

