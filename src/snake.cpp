#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cmath>

using namespace std;

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int GRID_SIZE = 20;
const int GRID_WIDTH = WINDOW_WIDTH / GRID_SIZE;
const int GRID_HEIGHT = WINDOW_HEIGHT / GRID_SIZE;

bool gameOver;
int score, max_score;
sf::Vector2i snakeHead;
vector<sf::Vector2i> snakeBody;
sf::Vector2i fruit;
sf::Vector2i direction;
sf::Clock gameClock;

// Add new variables for animation
float moveTimer = 0.0f;
const float MOVE_DELAY = 0.12f; // seconds per move
bool movedThisFrame = false;
sf::Vector2f snakeDrawOffset(0, 0);

// Animation for new segment
bool growing = false;
float growTimer = 0.0f;
const float GROW_ANIMATION_TIME = 0.2f;
sf::Vector2i growSegmentPos;

void setup() {
	gameOver = false;
	score = 0;
	direction = sf::Vector2i(1, 0); // Start moving right
	snakeHead = sf::Vector2i(GRID_WIDTH / 2, GRID_HEIGHT / 2);
	snakeBody.clear();
	snakeBody.push_back(snakeHead);
	
	// Place fruit at random position
	srand(time(0));
	fruit.x = rand() % GRID_WIDTH;
	fruit.y = rand() % GRID_HEIGHT;
}

void handleInput(sf::RenderWindow& window) {
	sf::Event event;
	while (window.pollEvent(event)) {
		if (event.type == sf::Event::Closed)
			window.close();
		
		if (event.type == sf::Event::KeyPressed) {
			switch (event.key.code) {
				case sf::Keyboard::Left:
					if (direction.x != 1) direction = sf::Vector2i(-1, 0);
					break;
				case sf::Keyboard::Right:
					if (direction.x != -1) direction = sf::Vector2i(1, 0);
					break;
				case sf::Keyboard::Up:
					if (direction.y != 1) direction = sf::Vector2i(0, -1);
					break;
				case sf::Keyboard::Down:
					if (direction.y != -1) direction = sf::Vector2i(0, 1);
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
			}
		}
	}
}

void update(float dt) {
	moveTimer += dt;
	movedThisFrame = false;
	if (growing) {
		growTimer += dt;
		if (growTimer >= GROW_ANIMATION_TIME) {
			growing = false;
			growTimer = 0.0f;
		}
	}
	if (moveTimer >= MOVE_DELAY && !growing) {
		moveTimer -= MOVE_DELAY;
		movedThisFrame = true;
		// Move snake
		snakeHead += direction;
		// Check wall collision
		if (snakeHead.x < 0 || snakeHead.x >= GRID_WIDTH ||
			snakeHead.y < 0 || snakeHead.y >= GRID_HEIGHT) {
			gameOver = true;
			return;
		}
		// Check self collision
		for (const auto& segment : snakeBody) {
			if (segment == snakeHead) {
				gameOver = true;
				return;
			}
		}
		// Check fruit collision
		if (snakeHead == fruit) {
			score += 10;
			fruit.x = rand() % GRID_WIDTH;
			fruit.y = rand() % GRID_HEIGHT;
			// Start growth animation
			growing = true;
			growTimer = 0.0f;
			growSegmentPos = snakeBody.back();
		} else {
			snakeBody.pop_back();
		}
		snakeBody.insert(snakeBody.begin(), snakeHead);
	}
	// Calculate draw offset for smooth movement
	float interp = moveTimer / MOVE_DELAY;
	snakeDrawOffset = sf::Vector2f(-direction.x * (1.0f - interp) * GRID_SIZE, -direction.y * (1.0f - interp) * GRID_SIZE);
}

void draw(sf::RenderWindow& window, float dt) {
	window.clear(sf::Color(30, 30, 40)); // dark background
	// Draw boundary
	sf::RectangleShape border(sf::Vector2f(GRID_WIDTH * GRID_SIZE, GRID_HEIGHT * GRID_SIZE));
	border.setPosition(0, 0);
	border.setFillColor(sf::Color::Transparent);
	border.setOutlineThickness(4);
	border.setOutlineColor(sf::Color(200, 200, 255));
	window.draw(border);
	// Draw snake with smooth movement
	sf::RectangleShape segment(sf::Vector2f(GRID_SIZE - 2, GRID_SIZE - 2));
	segment.setFillColor(sf::Color(60, 220, 60));
	for (size_t i = 0; i < snakeBody.size(); ++i) {
		sf::Vector2f pos(snakeBody[i].x * GRID_SIZE + 1, snakeBody[i].y * GRID_SIZE + 1);
		if (i == 0) pos += snakeDrawOffset; // interpolate head
		// Animate the new segment if growing
		if (growing && i == snakeBody.size() - 1 && snakeBody[i] == growSegmentPos) {
			float scale = std::min(growTimer / GROW_ANIMATION_TIME, 1.0f);
			segment.setOrigin((GRID_SIZE - 2) / 2, (GRID_SIZE - 2) / 2);
			segment.setPosition(pos.x + (GRID_SIZE - 2) / 2, pos.y + (GRID_SIZE - 2) / 2);
			segment.setScale(scale, scale);
			segment.setFillColor(sf::Color(120, 255, 120));
		} else {
			segment.setOrigin(0, 0);
			segment.setPosition(pos);
			segment.setScale(1, 1);
			segment.setFillColor(i == 0 ? sf::Color(80, 255, 80) : sf::Color(60, 220, 60));
		}
		window.draw(segment);
	}
	// Animate fruit (pulsing)
	float pulse = 1.0f + 0.2f * std::sin(gameClock.getElapsedTime().asSeconds() * 4);
	sf::CircleShape fruitShape((GRID_SIZE / 2 - 2) * pulse);
	fruitShape.setFillColor(sf::Color(255, 80, 80));
	fruitShape.setOrigin(fruitShape.getRadius(), fruitShape.getRadius());
	fruitShape.setPosition(fruit.x * GRID_SIZE + GRID_SIZE / 2 + 1, fruit.y * GRID_SIZE + GRID_SIZE / 2 + 1);
	window.draw(fruitShape);
	// Draw score
	sf::Font font;
	if (font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
		sf::Text scoreText;
		scoreText.setFont(font);
		scoreText.setString("Score: " + to_string(score));
		scoreText.setCharacterSize(24);
		scoreText.setFillColor(sf::Color::White);
		scoreText.setPosition(10, 10);
		window.draw(scoreText);
	}
	window.display();
}

int main() {
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Snake Game");
	window.setFramerateLimit(60);
	setup();
	float lastTime = gameClock.getElapsedTime().asSeconds();
	while (window.isOpen()) {
		handleInput(window);
		float now = gameClock.getElapsedTime().asSeconds();
		float dt = now - lastTime;
		lastTime = now;
		if (!gameOver) {
			update(dt);
			draw(window, dt);
		} else {
			// Game over screen
			window.clear(sf::Color(30, 30, 40));
			sf::Font font;
			if (font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
				sf::Text gameOverText;
				gameOverText.setFont(font);
				gameOverText.setString("Game Over!\nPress ESC to exit");
				gameOverText.setCharacterSize(30);
				gameOverText.setFillColor(sf::Color::White);
				gameOverText.setPosition(WINDOW_WIDTH/4, WINDOW_HEIGHT/2);
				window.draw(gameOverText);
			}
			window.display();
		}
	}
	return 0;
}
