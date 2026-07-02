#include <iostream>
#include <SFML/Graphics.hpp>
using namespace std;

int main()
{
    sf::RenderWindow window(sf::VideoMode(1900, 1200), "SFML window");

    // Load wallpaper
    sf::Texture wallpaperTexture;
    if (!wallpaperTexture.loadFromFile("bg.jpg")) {
        return 1;
    }
    sf::Sprite wallpaperSprite(wallpaperTexture);

    // Scale wallpaper to window size
    float scaleX = static_cast<float>(window.getSize().x) / wallpaperTexture.getSize().x;
    float scaleY = static_cast<float>(window.getSize().y) / wallpaperTexture.getSize().y;
    wallpaperSprite.setScale(scaleX, scaleY);

    // Load "This PC" icon
    sf::Texture iconTexture;
    if (!iconTexture.loadFromFile("thisPC.png")) {
        return 1;
    }
    sf::Sprite iconSprite(iconTexture);
    iconSprite.setPosition(100, 100);
    iconSprite.setScale(0.5f, 0.5f);

    // Load font and create label
    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        return 1;
    }
    sf::Text iconLabel("This PC", font, 16);
    iconLabel.setFillColor(sf::Color::White);
    iconLabel.setPosition(100, 160);  // Slightly below the icon

    // Create taskbar (50% opacity)
    sf::RectangleShape taskbar;
    float taskbarHeight = 60.f;
    taskbar.setSize(sf::Vector2f(window.getSize().x, taskbarHeight));
    taskbar.setPosition(0, window.getSize().y - taskbarHeight);
    taskbar.setFillColor(sf::Color(128, 128, 128, 179)); // Gray with ~70% opacity


    // Load shutdown icon
    sf::Texture shutdownTexture;
    if (!shutdownTexture.loadFromFile("shutdown.png")) {
        return 1;
    }
    sf::Sprite shutdownIcon(shutdownTexture);

    // Position at bottom-right
    float shutdownScale = 0.4f;
    shutdownIcon.setScale(shutdownScale, shutdownScale);
    // Set scale
shutdownIcon.setScale(shutdownScale, shutdownScale);

// Get scaled bounds
sf::FloatRect shutdownBounds = shutdownIcon.getGlobalBounds();

// Position at bottom-right corner, 20px from right
shutdownIcon.setPosition(
    window.getSize().x - shutdownBounds.width - 1800,
    window.getSize().y - shutdownBounds.height - (taskbarHeight - shutdownBounds.height) / 2
);


    // Main loop
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed ||
                (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
                window.close();
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                sf::Vector2f mouseFloat(mousePos.x, mousePos.y);

                // Click on "This PC"
                if (iconSprite.getGlobalBounds().contains(mouseFloat)) {
                    cout << "Icon clicked! Opening new window...\n";

                     system("./2");
                    //while (newWindow.isOpen()) {
                      //  sf::Event newEvent;
                        //while (newWindow.pollEvent(newEvent)) {
                          //  if (newEvent.type == sf::Event::Closed ||
                            //    (newEvent.type == sf::Event::KeyPressed && newEvent.key.code == sf::Keyboard::Escape)) {
                              //  newWindow.close();
                            //}
                        //}
                        //newWindow.clear(sf::Color::Blue);
                        //newWindow.display();
                    //}
                }
                // Click on shutdown icon
                else if (shutdownIcon.getGlobalBounds().contains(mouseFloat)) {
                    cout << "Shutdown icon clicked. Exiting...\n";
                    exit(0);
                }
            }
        }

        // Draw desktop
        window.clear();
        window.draw(wallpaperSprite);
        window.draw(iconSprite);
        window.draw(iconLabel);
        window.draw(taskbar);
        window.draw(shutdownIcon);
        window.display();
    }

    return 0;
}

