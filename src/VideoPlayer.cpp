#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

using namespace std;

class VideoPlayerGUI {
    sf::RenderWindow window;
    sf::Font font;
    string currentFile = "No video selected";
    bool isVLCMode = false;
    
    FILE* videoPipe = nullptr;
    sf::Texture videoTexture;
    sf::Sprite videoSprite;
    
    uint8_t* frontBuffer = nullptr;
    uint8_t* backBuffer = nullptr;
    
    std::thread readerThread;
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> isThreadRunning{true};
    std::atomic<bool> hasNewFrame{false};
    bool hasRenderedFirstFrame = false;
    std::mutex bufferMutex;
    
    const int VIDEO_W = 800;
    const int VIDEO_H = 600;
    const size_t FRAME_SIZE = 800 * 600 * 4;

    string openFileDialog() {
        char buffer[256];
        string result = "";
        FILE* pipe = popen("zenity --file-selection --title=\"Select Video File\" --file-filter=\"Video files (mp4, mkv, avi) | *.mp4 *.mkv *.avi\" 2>/dev/null", "r");
        if (!pipe) return "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

    void stopVideo() {
        isPlaying = false;
        if (videoPipe) {
            pclose(videoPipe);
            videoPipe = nullptr;
        }
        // Kill any audio backends we may have launched
        system("killall -9 cvlc ffplay 2>/dev/null");
    }

    void stopVLC() {
        system("killall vlc 2>/dev/null");
        isVLCMode = false;
    }

    void videoReaderTask() {
        while (isThreadRunning) {
            if (!isPlaying || !videoPipe) {
                sf::sleep(sf::milliseconds(10));
                continue;
            }

            size_t totalBytesRead = 0;
            while (totalBytesRead < FRAME_SIZE && isPlaying && videoPipe) {
                size_t bytesRead = fread(backBuffer + totalBytesRead, 1, FRAME_SIZE - totalBytesRead, videoPipe);
                if (bytesRead > 0) {
                    totalBytesRead += bytesRead;
                } else {
                    break;
                }
            }

            if (totalBytesRead == FRAME_SIZE) {
                std::lock_guard<std::mutex> lock(bufferMutex);
                memcpy(frontBuffer, backBuffer, FRAME_SIZE);
                hasNewFrame = true;
            } else {
                isPlaying = false;
            }
        }
    }

public:
    VideoPlayerGUI() : window(sf::VideoMode(VIDEO_W, VIDEO_H), "MaqOS - Video Player Pro") {
        // High PulseAudio buffer to eliminate ALL audio glitching on WSL
        setenv("PULSE_LATENCY_MSEC", "150", 1);
        
        window.setFramerateLimit(120);
        
        bool fontLoaded = false;
        if (!fontLoaded) fontLoaded = font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        if (!fontLoaded) fontLoaded = font.loadFromFile("arial.ttf");

        videoTexture.create(VIDEO_W, VIDEO_H);
        videoSprite.setTexture(videoTexture);
        
        frontBuffer = new uint8_t[FRAME_SIZE];
        backBuffer = new uint8_t[FRAME_SIZE];
        
        readerThread = std::thread(&VideoPlayerGUI::videoReaderTask, this);
    }

    ~VideoPlayerGUI() {
        isThreadRunning = false;
        stopVideo();
        stopVLC();
        if (readerThread.joinable()) {
            readerThread.join();
        }
        delete[] frontBuffer;
        delete[] backBuffer;
    }

    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    stopVideo();
                    stopVLC();
                    window.close();
                }
                
                // Click anywhere to stop native video playback
                if (isPlaying && event.type == sf::Event::MouseButtonPressed) {
                    stopVideo();
                }
                else if (!isPlaying && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    int x = event.mouseButton.x;
                    int y = event.mouseButton.y;
                    
                    // "Run Here" Button (100, 350, 280x60)
                    if (x >= 100 && x <= 380 && y >= 350 && y <= 410) {
                        string file = openFileDialog();
                        if (!file.empty()) {
                            currentFile = file;
                            stopVideo();
                            stopVLC();
                            hasRenderedFirstFrame = false;
                            
                            // Use cvlc (headless VLC) for crystal-clear audio instead of glitchy ffplay
                            string audioCmd = "PULSE_LATENCY_MSEC=150 cvlc --no-video --play-and-exit \"" + currentFile + "\" 2>/dev/null &";
                            int ret = system(audioCmd.c_str());
                            (void)ret;

                            // Launch video frame pipe
                            string videoCmd = "ffmpeg -re -i \"" + currentFile + "\" -f rawvideo -pix_fmt rgba -s 800x600 - 2>/dev/null";
                            videoPipe = popen(videoCmd.c_str(), "r");
                            
                            if (videoPipe) {
                                isPlaying = true;
                            }
                        }
                    }
                    // "Open in VLC" Button (420, 350, 280x60)
                    else if (x >= 420 && x <= 700 && y >= 350 && y <= 410) {
                        string file = openFileDialog();
                        if (!file.empty()) {
                            currentFile = file;
                            stopVideo();
                            stopVLC();
                            isVLCMode = true;
                            // Launch full VLC for ultimate beast quality
                            string cmd = "vlc \"" + currentFile + "\" 2>/dev/null &";
                            int ret = system(cmd.c_str());
                            (void)ret;
                        }
                    }
                }
            }

            // Sync decoded frames from background thread
            if (hasNewFrame) {
                std::lock_guard<std::mutex> lock(bufferMutex);
                videoTexture.update(frontBuffer);
                hasNewFrame = false;
                hasRenderedFirstFrame = true;
            }

            // Cleanup if native video finished
            if (!isPlaying && videoPipe) {
                stopVideo();
            }

            render();
        }
    }

private:
    void render() {
        window.clear(sf::Color(15, 15, 20));
        
        if (isPlaying && hasRenderedFirstFrame) {
            window.draw(videoSprite);
        } else {
            // Draw Menu Screen
            sf::Text title("MaqOS Video Player", font, 40);
            title.setPosition(220, 100);
            title.setFillColor(sf::Color(220, 50, 50));
            window.draw(title);

            string displayFile = currentFile;
            if (displayFile.length() > 60) displayFile = "..." + displayFile.substr(displayFile.length() - 57);

            sf::Text status(displayFile, font, 18);
            status.setPosition(150, 200);
            status.setFillColor(sf::Color(200, 200, 200));
            window.draw(status);
            
            string hintText = "Multi-Threaded 120 FPS Engine Ready.";
            if (isVLCMode) hintText = "Playing in VLC [Beast Quality Audio+Video]";
            
            sf::Text hint(hintText, font, 14);
            hint.setPosition(220, 260);
            hint.setFillColor(isVLCMode ? sf::Color(255, 140, 0) : sf::Color(100, 200, 100));
            window.draw(hint);

            // "Run Here" Button
            sf::RectangleShape btnRunHere(sf::Vector2f(280, 60));
            btnRunHere.setPosition(100, 350);
            btnRunHere.setFillColor(sf::Color(220, 50, 50));
            btnRunHere.setOutlineThickness(2);
            btnRunHere.setOutlineColor(sf::Color(150, 30, 30));
            window.draw(btnRunHere);
            
            sf::Text txtRunHere("Run Here", font, 22);
            txtRunHere.setPosition(185, 365);
            txtRunHere.setFillColor(sf::Color::White);
            window.draw(txtRunHere);

            // "Open in VLC" Button
            sf::RectangleShape btnVLC(sf::Vector2f(280, 60));
            btnVLC.setPosition(420, 350);
            btnVLC.setFillColor(sf::Color(255, 140, 0)); // VLC Orange
            btnVLC.setOutlineThickness(2);
            btnVLC.setOutlineColor(sf::Color(200, 100, 0));
            window.draw(btnVLC);
            
            sf::Text txtVLC("Open in VLC", font, 22);
            txtVLC.setPosition(490, 365);
            txtVLC.setFillColor(sf::Color::White);
            window.draw(txtVLC);
        }

        window.display();
    }
};

int main() {
    VideoPlayerGUI app;
    app.run();
    return 0;
}


