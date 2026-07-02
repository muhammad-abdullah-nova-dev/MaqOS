#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>

using namespace std;

class AudioPlayerGUI {
    sf::RenderWindow window;
    sf::Font font;
    
    sf::SoundBuffer audioBuffer;
    sf::Sound sound;
    
    bool isPlaying = false;
    bool isVLCMode = false;
    string currentFile = "No file selected";
    
    string openFileDialog() {
        char buffer[256];
        string result = "";
        FILE* pipe = popen("zenity --file-selection --title=\"Select Audio File\" --file-filter=\"Audio files (mp3, wav, ogg, flac) | *.mp3 *.wav *.ogg *.flac\" 2>/dev/null", "r");
        if (!pipe) return "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

    void loadAndPlay(const string& file) {
        sound.stop();
        if (audioBuffer.loadFromFile(file)) {
            sound.setBuffer(audioBuffer);
            sound.setVolume(100.f); // Maximize native volume
            sound.play();
            isPlaying = true;
        } else {
            isPlaying = false;
        }
    }

public:
    AudioPlayerGUI() : window(sf::VideoMode(500, 300), "MaqOS - Spotify Lite") {
        setenv("PULSE_LATENCY_MSEC", "150", 1);
        window.setFramerateLimit(120);
        
        bool fontLoaded = false;
        if (!fontLoaded) fontLoaded = font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("arial.ttf");
    }

    ~AudioPlayerGUI() {
        sound.stop();
        if (isVLCMode) system("killall vlc 2>/dev/null");
    }

    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    sound.stop();
                    if (isVLCMode) system("killall vlc 2>/dev/null");
                    window.close();
                }
                
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    int x = event.mouseButton.x;
                    int y = event.mouseButton.y;
                    
                    // Run Here Button (50, 220, 190x40)
                    if (x >= 50 && x <= 240 && y >= 220 && y <= 260) {
                        string file = openFileDialog();
                        if (!file.empty()) {
                            currentFile = file;
                            isVLCMode = false;
                            system("killall vlc 2>/dev/null"); // Stop VLC if it was running
                            loadAndPlay(currentFile);
                        }
                    }
                    // Open in VLC Button (260, 220, 190x40)
                    else if (x >= 260 && x <= 450 && y >= 220 && y <= 260) {
                        string file = openFileDialog();
                        if (!file.empty()) {
                            currentFile = file;
                            sound.stop();
                            isPlaying = false;
                            isVLCMode = true;
                            // Launch VLC for Beast Quality
                            string cmd = "vlc \"" + currentFile + "\" 2>/dev/null &";
                            int ret = system(cmd.c_str());
                            (void)ret;
                        }
                    }
                    
                    // Play Button (100, 150, 80x40)
                    else if (x >= 100 && x <= 180 && y >= 150 && y <= 190 && !isVLCMode) {
                        if (currentFile != "No file selected") {
                            if (sound.getStatus() == sf::Sound::Paused || sound.getStatus() == sf::Sound::Stopped) {
                                sound.play();
                                isPlaying = true;
                            }
                        }
                    }
                    // Pause Button (210, 150, 80x40)
                    else if (x >= 210 && x <= 290 && y >= 150 && y <= 190 && !isVLCMode) {
                        sound.pause();
                        isPlaying = false;
                    }
                    // Stop Button (320, 150, 80x40)
                    else if (x >= 320 && x <= 400 && y >= 150 && y <= 190) {
                        sound.stop();
                        isPlaying = false;
                        if (isVLCMode) {
                            system("killall vlc 2>/dev/null");
                            isVLCMode = false;
                        }
                    }
                }
            }
            
            if (isPlaying && sound.getStatus() == sf::Sound::Stopped) {
                isPlaying = false;
            }

            render();
        }
    }

private:
    void render() {
        window.clear(sf::Color(18, 18, 18));
        
        sf::Text title("MaqOS Audio Player", font, 32);
        title.setPosition(100, 20);
        title.setFillColor(sf::Color(30, 215, 96));
        window.draw(title);

        string displayFile = currentFile;
        if (displayFile.length() > 40) displayFile = "..." + displayFile.substr(displayFile.length() - 37);

        string statusText = "Status: Paused / Stopped\n" + displayFile;
        if (isPlaying) statusText = "Now Playing [Native]: \n" + displayFile;
        if (isVLCMode) statusText = "Playing in VLC [Beast Quality]: \n" + displayFile;

        sf::Text status(statusText, font, 14);
        status.setPosition(50, 80);
        status.setFillColor(sf::Color(179, 179, 179));
        window.draw(status);

        // Play Button (Dimmed if VLC Mode)
        sf::RectangleShape btnPlay(sf::Vector2f(80, 40));
        btnPlay.setPosition(100, 150);
        btnPlay.setFillColor(isVLCMode ? sf::Color(30, 100, 50) : sf::Color(30, 215, 96));
        window.draw(btnPlay);
        sf::Text txtPlay("Play", font, 18);
        txtPlay.setPosition(120, 158);
        txtPlay.setFillColor(sf::Color::Black);
        window.draw(txtPlay);

        // Pause Button (Dimmed if VLC Mode)
        sf::RectangleShape btnPause(sf::Vector2f(80, 40));
        btnPause.setPosition(210, 150);
        btnPause.setFillColor(isVLCMode ? sf::Color(50, 50, 50) : sf::Color(83, 83, 83));
        window.draw(btnPause);
        sf::Text txtPause("Pause", font, 18);
        txtPause.setPosition(225, 158);
        window.draw(txtPause);

        // Stop Button
        sf::RectangleShape btnStop(sf::Vector2f(80, 40));
        btnStop.setPosition(320, 150);
        btnStop.setFillColor(sf::Color(200, 50, 50));
        window.draw(btnStop);
        sf::Text txtStop("Stop", font, 18);
        txtStop.setPosition(340, 158);
        window.draw(txtStop);

        // Run Here Button
        sf::RectangleShape btnRunHere(sf::Vector2f(190, 40));
        btnRunHere.setPosition(50, 220);
        btnRunHere.setFillColor(sf::Color(40, 40, 40));
        btnRunHere.setOutlineThickness(1);
        btnRunHere.setOutlineColor(sf::Color(83, 83, 83));
        window.draw(btnRunHere);
        sf::Text txtRunHere("Run Here", font, 18);
        txtRunHere.setPosition(100, 228);
        txtRunHere.setFillColor(sf::Color::White);
        window.draw(txtRunHere);

        // Open in VLC Button
        sf::RectangleShape btnVLC(sf::Vector2f(190, 40));
        btnVLC.setPosition(260, 220);
        btnVLC.setFillColor(sf::Color(255, 140, 0)); // VLC Orange
        btnVLC.setOutlineThickness(1);
        btnVLC.setOutlineColor(sf::Color(200, 100, 0));
        window.draw(btnVLC);
        sf::Text txtVLC("Open in VLC", font, 18);
        txtVLC.setPosition(300, 228);
        txtVLC.setFillColor(sf::Color::White);
        window.draw(txtVLC);

        // Progress Bar Background
        sf::RectangleShape bar(sf::Vector2f(400, 4));
        bar.setPosition(50, 135);
        bar.setFillColor(sf::Color(70, 70, 70));
        window.draw(bar);

        // Dynamic Progress Bar Fill
        if (!isVLCMode && sound.getStatus() != sf::Sound::Stopped && audioBuffer.getDuration().asSeconds() > 0) {
            float progressRatio = sound.getPlayingOffset().asSeconds() / audioBuffer.getDuration().asSeconds();
            if (progressRatio > 1.0f) progressRatio = 1.0f;
            
            sf::RectangleShape progress(sf::Vector2f(400 * progressRatio, 4));
            progress.setPosition(50, 135);
            progress.setFillColor(sf::Color(30, 215, 96));
            window.draw(progress);
        }

        window.display();
    }
};

int main() {
    AudioPlayerGUI app;
    app.run();
    return 0;
}

