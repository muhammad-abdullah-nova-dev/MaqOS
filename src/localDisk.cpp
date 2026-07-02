/**
 * @file localDisk.cpp
 * @brief MaqOS Local Disk Explorer Component
 * 
 * This application provides a visual representation of the system's disk drives (C: and D:).
 * It uses the SFML library for rendering a GUI that mimics a file explorer or "This PC" view.
 * Users can click on the disk icons to launch related system utilities like the console 
 * file manager or main menu.
 */

#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib>

/**
 * @brief Main entry point for the Local Disk application.
 * 
 * Initializes the window, loads graphical assets (icons/fonts), sets up the UI elements,
 * and enters the main event loop to handle user interactions.
 */
int main() {
    // Create a window for the Local Disk application with a resolution of 800x600 pixels
    sf::RenderWindow window(sf::VideoMode(800, 600), "Local Disk");

    // --- Asset Loading ---
    // Textures are used to store the images for the disk icons
    sf::Texture icon1Texture, icon2Texture;
    
    // Load the disk icon image. If loading fails, output an error and terminate.
    if (!icon1Texture.loadFromFile("disk_icon.png") || !icon2Texture.loadFromFile("disk_icon.png")) {
        std::cerr << "Failed to load icons.\n";
        return 1;
    }

    // --- UI Setup: Icons ---
    // Sprites are the visual representations of textures that can be moved/scaled
    sf::Sprite icon1(icon1Texture);
    sf::Sprite icon2(icon2Texture);

    // Scale down the icons to half their original size (0.5x)
    icon1.setScale(0.5f, 0.5f);
    icon2.setScale(0.5f, 0.5f);

    // Position the icons on the screen (Disk C: on the left, Disk D: on the right)
    icon1.setPosition(100, 100);
    icon2.setPosition(400, 100);

    // --- UI Setup: Typography ---
    // Load the Segoe UI variable font for a modern "MaqOS" look
    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        std::cerr << "Failed to load font.\n";
        return 1;
    }

    // --- UI Setup: Labels ---
    // Create text labels for the drives
    sf::Text label1("Local Disk (C:)", font, 16);
    label1.setFillColor(sf::Color::Black);
    // Position label1 directly below icon1
    label1.setPosition(icon1.getPosition().x, icon1.getPosition().y + icon1.getGlobalBounds().height + 5);

    sf::Text label2("Local Disk (D:)", font, 16);
    label2.setFillColor(sf::Color::Black);
    // Position label2 directly below icon2
    label2.setPosition(icon2.getPosition().x, icon2.getPosition().y + icon2.getGlobalBounds().height + 5);

    // --- UI Setup: Selection Boxes ---
    // Create decorative border boxes around each drive icon and its label
    
    // Box 1 for Disk C:
    sf::FloatRect box1Bounds = icon1.getGlobalBounds();
    box1Bounds.height += 100; // Increase height to encompass the text label area
    sf::RectangleShape box1(sf::Vector2f(box1Bounds.width + 200, box1Bounds.height));
    box1.setPosition(icon1.getPosition().x - 20, icon1.getPosition().y - 20);
    box1.setFillColor(sf::Color::Transparent);
    box1.setOutlineThickness(1.5);
    box1.setOutlineColor(sf::Color::Black);

    // Box 2 for Disk D:
    sf::FloatRect box2Bounds = icon2.getGlobalBounds();
    box2Bounds.height += 100;
    sf::RectangleShape box2(sf::Vector2f(box2Bounds.width + 200, box2Bounds.height));
    box2.setPosition(icon2.getPosition().x - 20, icon2.getPosition().y - 20);
    box2.setFillColor(sf::Color::Transparent);
    box2.setOutlineThickness(1.5);
    box2.setOutlineColor(sf::Color::Black);

    // --- Main Event Loop ---
    // This loop keeps the application running until the window is closed
    while (window.isOpen()) {
        sf::Event event;
        // Poll for window events (keyboard, mouse, window closing)
        while (window.pollEvent(event)) {
            // Close window on 'Close' event or 'Escape' key press
            if (event.type == sf::Event::Closed ||
                (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
                window.close();
            }

            // Handle Mouse Click Interactions
            if (event.type == sf::Event::MouseButtonPressed) {
                // Get the current mouse position relative to the window
                sf::Vector2f mousePos(sf::Mouse::getPosition(window));

                // Check if the user clicked within the bounds of Disk C:
                if (icon1.getGlobalBounds().contains(mousePos)) {
                    std::cout << "Local Disk (C:) clicked!\n";
                    // Launch the console file manager application
                    system("./file");
                }
                // Check if the user clicked within the bounds of Disk D:
                if (icon2.getGlobalBounds().contains(mousePos)) {
                    std::cout << "Local Disk (D:) clicked!\n";
                    // Launch the system menu/navigation application
                    system("./menu");
                }
            }
        }

        // --- Rendering ---
        // Clear the window with a light beige background color (#F0E8E1)
        window.clear(sf::Color(240, 232, 225)); 

        // Draw all UI components to the back buffer
        window.draw(box1);
        window.draw(icon1);
        window.draw(label1);
        window.draw(box2);
        window.draw(icon2);
        window.draw(label2);

        // Display the contents of the back buffer to the window
        window.display();
    }

    return 0;
}
