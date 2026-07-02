#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <string>

using namespace std;

string current_text = "";
pthread_mutex_t text_mutex = PTHREAD_MUTEX_INITIALIZER;
bool is_running = true;

void* auto_save_thread(void* arg) {
    while (is_running) {
        sleep(5);
        pthread_mutex_lock(&text_mutex);
        ofstream save_file("maqos_notepad_save.txt");
        if (save_file.is_open()) {
            save_file << current_text;
            save_file.close();
            cout << "[Notepad] Auto-saved buffer to maqos_notepad_save.txt" << endl;
        }
        pthread_mutex_unlock(&text_mutex);
    }
    return nullptr;
}

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "MaqOS Notepad");

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        cerr << "Failed to load font." << endl;
        return -1;
    }

    sf::Text displayText("", font, 20);
    displayText.setFillColor(sf::Color::White);
    displayText.setPosition(10, 10);

    pthread_t save_thread;
    pthread_create(&save_thread, nullptr, auto_save_thread, nullptr);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                is_running = false;
                window.close();
            } else if (event.type == sf::Event::TextEntered) {
                pthread_mutex_lock(&text_mutex);
                if (event.text.unicode == '\b') {
                    if (!current_text.empty()) {
                        current_text.pop_back();
                    }
                } else if (event.text.unicode < 128) {
                    current_text += static_cast<char>(event.text.unicode);
                }
                displayText.setString(current_text + "_");
                pthread_mutex_unlock(&text_mutex);
            }
        }

        window.clear(sf::Color(30, 30, 30));
        window.draw(displayText);
        window.display();
    }

    pthread_join(save_thread, nullptr);
    return 0;
}
