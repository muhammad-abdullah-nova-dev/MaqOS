#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

const int TILE_SIZE = 75;

enum GameState { MENU, PLAYING, GAME_OVER };
enum GameMode { MULTIPLAYER, VS_AI };

class ChessGUI {
private:
    sf::RenderWindow window;
    sf::Font font;
    char board[8][8];
    int selectedX = -1;
    int selectedY = -1;
    bool whiteTurn = true;
    GameState state = MENU;
    GameMode mode = MULTIPLAYER;

    void setupBoard() {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                board[i][j] = '\0';

        // Black pieces
        board[0][0] = 'r'; board[0][1] = 'n'; board[0][2] = 'b'; board[0][3] = 'q';
        board[0][4] = 'k'; board[0][5] = 'b'; board[0][6] = 'n'; board[0][7] = 'r';
        for (int j = 0; j < 8; j++) board[1][j] = 'p';

        // White pieces
        board[7][0] = 'R'; board[7][1] = 'N'; board[7][2] = 'B'; board[7][3] = 'Q';
        board[7][4] = 'K'; board[7][5] = 'B'; board[7][6] = 'N'; board[7][7] = 'R';
        for (int j = 0; j < 8; j++) board[6][j] = 'P';
    }

    bool isWhitePiece(char p) { return p >= 'A' && p <= 'Z'; }
    bool isBlackPiece(char p) { return p >= 'a' && p <= 'z'; }

    sf::String getPieceIcon(char p) {
        switch(p) {
            case 'K': return sf::String((sf::Uint32)0x2654);
            case 'Q': return sf::String((sf::Uint32)0x2655);
            case 'R': return sf::String((sf::Uint32)0x2656);
            case 'B': return sf::String((sf::Uint32)0x2657);
            case 'N': return sf::String((sf::Uint32)0x2658);
            case 'P': return sf::String((sf::Uint32)0x2659);
            case 'k': return sf::String((sf::Uint32)0x265A);
            case 'q': return sf::String((sf::Uint32)0x265B);
            case 'r': return sf::String((sf::Uint32)0x265C);
            case 'b': return sf::String((sf::Uint32)0x265D);
            case 'n': return sf::String((sf::Uint32)0x265E);
            case 'p': return sf::String((sf::Uint32)0x265F);
        }
        return sf::String("");
    }

    bool isPathClear(int startX, int startY, int endX, int endY) {
        int dx = (endX > startX) ? 1 : ((endX < startX) ? -1 : 0);
        int dy = (endY > startY) ? 1 : ((endY < startY) ? -1 : 0);
        
        int x = startX + dx;
        int y = startY + dy;
        
        while (x != endX || y != endY) {
            if (board[y][x] != '\0') return false;
            x += dx;
            y += dy;
        }
        return true;
    }

    bool isValidMove(char p, int startX, int startY, int endX, int endY) {
        int dx = abs(endX - startX);
        int dy = abs(endY - startY);
        bool isCapture = board[endY][endX] != '\0';
        
        switch (p) {
            case 'P': // White Pawn
                if (dx == 0 && !isCapture) {
                    if (endY == startY - 1) return true;
                    if (startY == 6 && endY == 4 && board[5][startX] == '\0') return true;
                } else if (dx == 1 && endY == startY - 1 && isCapture) {
                    return true;
                }
                return false;
            case 'p': // Black Pawn
                if (dx == 0 && !isCapture) {
                    if (endY == startY + 1) return true;
                    if (startY == 1 && endY == 3 && board[2][startX] == '\0') return true;
                } else if (dx == 1 && endY == startY + 1 && isCapture) {
                    return true;
                }
                return false;
            case 'R': case 'r': // Rook
                if (dx == 0 || dy == 0) return isPathClear(startX, startY, endX, endY);
                return false;
            case 'B': case 'b': // Bishop
                if (dx == dy) return isPathClear(startX, startY, endX, endY);
                return false;
            case 'Q': case 'q': // Queen
                if (dx == 0 || dy == 0 || dx == dy) return isPathClear(startX, startY, endX, endY);
                return false;
            case 'N': case 'n': // Knight
                if ((dx == 2 && dy == 1) || (dx == 1 && dy == 2)) return true;
                return false;
            case 'K': case 'k': // King
                if (dx <= 1 && dy <= 1) return true;
                return false;
        }
        return false;
    }

    bool isSquareAttacked(int targetX, int targetY, bool byWhite) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                char p = board[y][x];
                if (p != '\0' && (byWhite ? isWhitePiece(p) : isBlackPiece(p))) {
                    if (isValidMove(p, x, y, targetX, targetY)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool isKingInCheck(bool whiteKing) {
        int kx = -1, ky = -1;
        char targetKing = whiteKing ? 'K' : 'k';
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (board[y][x] == targetKing) {
                    kx = x; ky = y; break;
                }
            }
        }
        if (kx == -1) return true; // King dead = checkmate
        return isSquareAttacked(kx, ky, !whiteKing);
    }

    bool simulateMoveAndCheck(int startX, int startY, int endX, int endY, bool checkWhiteKing) {
        char p = board[startY][startX];
        char target = board[endY][endX];
        
        board[endY][endX] = p;
        board[startY][startX] = '\0';
        
        bool inCheck = isKingInCheck(checkWhiteKing);
        
        board[startY][startX] = p;
        board[endY][endX] = target;
        
        return inCheck;
    }

    bool isCheckmate(bool checkWhiteKing) {
        if (!isKingInCheck(checkWhiteKing)) return false;
        
        for (int startY = 0; startY < 8; startY++) {
            for (int startX = 0; startX < 8; startX++) {
                char p = board[startY][startX];
                if (p != '\0' && (checkWhiteKing ? isWhitePiece(p) : isBlackPiece(p))) {
                    for (int endY = 0; endY < 8; endY++) {
                        for (int endX = 0; endX < 8; endX++) {
                            if (isValidMove(p, startX, startY, endX, endY)) {
                                if (board[endY][endX] == '\0' || (checkWhiteKing ? isBlackPiece(board[endY][endX]) : isWhitePiece(board[endY][endX]))) {
                                    if (!simulateMoveAndCheck(startX, startY, endX, endY, checkWhiteKing)) {
                                        return false; // Found an escape!
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    void makeAIMove() {
        sf::sleep(sf::milliseconds(500)); // Delay for realism
        vector<pair<int, int>> blackPieces;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (isBlackPiece(board[i][j])) {
                    blackPieces.push_back({j, i});
                }
            }
        }

        if (!blackPieces.empty()) {
            bool moved = false;
            int attempts = 0;
            while (!moved && attempts < 2000) {
                attempts++;
                int pIdx = rand() % blackPieces.size();
                int startX = blackPieces[pIdx].first;
                int startY = blackPieces[pIdx].second;
                char p = board[startY][startX];

                int endX = rand() % 8;
                int endY = rand() % 8;

                if (isValidMove(p, startX, startY, endX, endY)) {
                    if (board[endY][endX] == '\0' || isWhitePiece(board[endY][endX])) {
                        if (!simulateMoveAndCheck(startX, startY, endX, endY, false)) { // AI must not move into check
                            board[endY][endX] = p;
                            board[startY][startX] = '\0';
                            moved = true;
                            whiteTurn = true;
                        }
                    }
                }
            }
            if (!moved) {
                // If 2000 random attempts fail, fallback to systematic search
                for(int i=0; i<8 && !moved; i++) {
                    for(int j=0; j<8 && !moved; j++) {
                        char p = board[i][j];
                        if (isBlackPiece(p)) {
                            for(int y=0; y<8 && !moved; y++) {
                                for(int x=0; x<8 && !moved; x++) {
                                    if (isValidMove(p, j, i, x, y) && (board[y][x] == '\0' || isWhitePiece(board[y][x]))) {
                                        if (!simulateMoveAndCheck(j, i, x, y, false)) {
                                            board[y][x] = p;
                                            board[i][j] = '\0';
                                            moved = true;
                                            whiteTurn = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (!moved) whiteTurn = true; // No valid moves
            }
        }
        
        if (isCheckmate(true)) state = GAME_OVER;
    }

public:
    ChessGUI() : window(sf::VideoMode(8 * TILE_SIZE, 8 * TILE_SIZE + 40), "MaqOS - Chess") {
        window.setFramerateLimit(30);
        srand(time(NULL));
        
        // Aggressive font fallback to guarantee chess piece unicode support
        bool fontLoaded = false;
        if (!fontLoaded) fontLoaded = font.loadFromFile("C:/Windows/Fonts/seguisym.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("C:/Windows/Fonts/ArialUni.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/mnt/c/Windows/Fonts/seguisym.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/mnt/c/Windows/Fonts/ArialUni.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/freefont/FreeSerif.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("arial.ttf");
        if (!fontLoaded) cerr << "Failed to load any fonts!\n";
    }

    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                }

                if (state == MENU) {
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        int y = event.mouseButton.y;
                        if (y >= 200 && y <= 250) {
                            mode = MULTIPLAYER;
                            state = PLAYING;
                            setupBoard();
                        } else if (y >= 300 && y <= 350) {
                            mode = VS_AI;
                            state = PLAYING;
                            setupBoard();
                        }
                    }
                } else if (state == GAME_OVER) {
                    if (event.type == sf::Event::MouseButtonPressed) {
                        state = MENU; // click anywhere to restart
                    }
                } else if (state == PLAYING) {
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        if (mode == MULTIPLAYER || (mode == VS_AI && whiteTurn)) {
                            int x = event.mouseButton.x / TILE_SIZE;
                            int y = (event.mouseButton.y - 40) / TILE_SIZE;

                            if (x >= 0 && x < 8 && y >= 0 && y < 8) {
                                if (selectedX == -1) {
                                    // Select piece
                                    char p = board[y][x];
                                    if (p != '\0') {
                                        if ((whiteTurn && isWhitePiece(p)) || (!whiteTurn && isBlackPiece(p))) {
                                            selectedX = x;
                                            selectedY = y;
                                        }
                                    }
                                } else {
                                    // Move piece
                                    char p = board[selectedY][selectedX];
                                    if (x == selectedX && y == selectedY) {
                                        selectedX = -1;
                                        selectedY = -1;
                                    } else if ((whiteTurn && isWhitePiece(board[y][x])) || (!whiteTurn && isBlackPiece(board[y][x]))) {
                                        selectedX = x;
                                        selectedY = y;
                                    } else {
                                        // Attempt move
                                        if (isValidMove(p, selectedX, selectedY, x, y)) {
                                            if (!simulateMoveAndCheck(selectedX, selectedY, x, y, whiteTurn)) {
                                                board[y][x] = p;
                                                board[selectedY][selectedX] = '\0';
                                                selectedX = -1;
                                                selectedY = -1;
                                                whiteTurn = !whiteTurn; // switch turn
                                                
                                                if (isCheckmate(!whiteTurn)) {
                                                    state = GAME_OVER;
                                                }
                                            }
                                        } else {
                                            selectedX = -1;
                                            selectedY = -1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (state == PLAYING && mode == VS_AI && !whiteTurn) {
                render(); 
                makeAIMove();
            }

            render();
        }
    }

private:
    void render() {
        window.clear(sf::Color(30, 30, 40));

        if (state == MENU) {
            sf::Text title("MaqOS Chess", font, 40);
            title.setFillColor(sf::Color::White);
            title.setPosition(180, 50);
            window.draw(title);

            sf::RectangleShape btn1(sf::Vector2f(300, 50));
            btn1.setPosition(150, 200);
            btn1.setFillColor(sf::Color(70, 100, 150));
            window.draw(btn1);

            sf::Text txt1("1. Multiplayer (Local)", font, 24);
            txt1.setFillColor(sf::Color::White);
            txt1.setPosition(180, 210);
            window.draw(txt1);

            sf::RectangleShape btn2(sf::Vector2f(300, 50));
            btn2.setPosition(150, 300);
            btn2.setFillColor(sf::Color(150, 70, 70));
            window.draw(btn2);

            sf::Text txt2("2. Single Player (vs AI)", font, 24);
            txt2.setFillColor(sf::Color::White);
            txt2.setPosition(180, 310);
            window.draw(txt2);

        } else if (state == GAME_OVER) {
            sf::Text title("CHECKMATE!", font, 50);
            title.setFillColor(sf::Color::Red);
            title.setPosition(150, 150);
            window.draw(title);
            
            string winner = whiteTurn ? "Black Wins!" : "White Wins!";
            sf::Text sub(winner + " Click to restart.", font, 24);
            sub.setFillColor(sf::Color::White);
            sub.setPosition(150, 250);
            window.draw(sub);
        } else if (state == PLAYING) {
            // Top bar
            string turnMsg = whiteTurn ? "White's Turn" : "Black's Turn";
            if (isKingInCheck(whiteTurn)) turnMsg += " (IN CHECK!)";
            if (mode == VS_AI && !whiteTurn) turnMsg = "AI is thinking...";
            
            sf::Text status(turnMsg, font, 20);
            if (isKingInCheck(whiteTurn)) status.setFillColor(sf::Color::Red);
            else status.setFillColor(sf::Color::White);
            status.setPosition(10, 5);
            window.draw(status);

            // Draw board
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    sf::RectangleShape tile(sf::Vector2f(TILE_SIZE, TILE_SIZE));
                    tile.setPosition(j * TILE_SIZE, i * TILE_SIZE + 40);

                    if ((i + j) % 2 == 0) {
                        tile.setFillColor(sf::Color(240, 217, 181));
                    } else {
                        tile.setFillColor(sf::Color(181, 136, 99));
                    }

                    if (j == selectedX && i == selectedY) {
                        tile.setFillColor(sf::Color(130, 150, 100));
                    }

                    window.draw(tile);

                    char p = board[i][j];
                    if (p != '\0') {
                        sf::Text piece(getPieceIcon(p), font, 55);
                        
                        if (isWhitePiece(p)) {
                            piece.setFillColor(sf::Color::White);
                            sf::Color outline = sf::Color::Black;
                            piece.setFillColor(outline);
                            float px = j * TILE_SIZE + 10;
                            float py = i * TILE_SIZE + 40 - 5;
                            piece.setPosition(px-1, py); window.draw(piece);
                            piece.setPosition(px+1, py); window.draw(piece);
                            piece.setPosition(px, py-1); window.draw(piece);
                            piece.setPosition(px, py+1); window.draw(piece);
                            piece.setFillColor(sf::Color::White);
                            piece.setPosition(px, py);
                        } else {
                            piece.setFillColor(sf::Color::Black);
                            piece.setPosition(j * TILE_SIZE + 10, i * TILE_SIZE + 40 - 5);
                        }
                        
                        window.draw(piece);
                    }
                }
            }
        }

        window.display();
    }
};

int main() {
    ChessGUI game;
    game.run();
    return 0;
}
