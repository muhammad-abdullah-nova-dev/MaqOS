// ============================================================
// MaqOS Kernel Panel â€” Elevated System Dashboard
// Features: Admin Auth, Process List, Memory Monitor, Log Viewer
// ============================================================
#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <csignal>

using namespace std;

// ---- Helper: read a file into string ----
string read_file(const string& path) {
    ifstream f(path);
    if (!f.is_open()) return "";
    stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ---- Process info struct ----
struct ProcInfo {
    int pid;
    string name;
    string state;
    long mem_kb;
};

// ---- Read process list from /proc ----
vector<ProcInfo> get_process_list() {
    vector<ProcInfo> procs;
    DIR* dir = opendir("/proc");
    if (!dir) return procs;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string dname = entry->d_name;
        bool isNum = !dname.empty();
        for (char c : dname) { if (!isdigit(c)) { isNum = false; break; } }
        if (!isNum) continue;

        int pid = stoi(dname);
        string status_path = "/proc/" + dname + "/status";
        ifstream sf(status_path);
        if (!sf.is_open()) continue;

        ProcInfo p;
        p.pid = pid;
        p.name = "unknown";
        p.state = "?";
        p.mem_kb = 0;

        string line;
        while (getline(sf, line)) {
            if (line.substr(0, 5) == "Name:") {
                p.name = line.substr(6);
                // trim leading whitespace
                size_t start = p.name.find_first_not_of(" \t");
                if (start != string::npos) p.name = p.name.substr(start);
            }
            if (line.substr(0, 6) == "State:") {
                p.state = line.substr(7, 1);
            }
            if (line.substr(0, 6) == "VmRSS:") {
                istringstream iss(line.substr(6));
                iss >> p.mem_kb;
            }
        }
        procs.push_back(p);
    }
    closedir(dir);
    return procs;
}

// ---- Get memory info ----
struct MemInfo {
    long total_mb, used_mb, free_mb, swap_total_mb, swap_used_mb;
};

MemInfo get_mem_info() {
    MemInfo m = {0,0,0,0,0};
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        m.total_mb = si.totalram / (1024 * 1024);
        m.free_mb  = si.freeram  / (1024 * 1024);
        m.used_mb  = m.total_mb - m.free_mb;
        m.swap_total_mb = si.totalswap / (1024 * 1024);
        m.swap_used_mb  = (si.totalswap - si.freeswap) / (1024 * 1024);
    }
    return m;
}

// ---- Read last N lines of system log ----
vector<string> read_log_tail(const string& path, int lines) {
    vector<string> all_lines;
    ifstream f(path);
    string line;
    while (getline(f, line)) all_lines.push_back(line);

    vector<string> tail;
    int start = (int)all_lines.size() - lines;
    if (start < 0) start = 0;
    for (int i = start; i < (int)all_lines.size(); i++)
        tail.push_back(all_lines[i]);
    return tail;
}

// ---- Check if process is a MaqOS task ----
bool is_maqos_task(const string& name) {
    vector<string> known = {
        "calculator", "simple_calendar", "clock", "stopWatch", "minesweeper",
        "chess", "ticTacToc", "numberGuess", "snake", "fileCreate", "fileDelete",
        "fileInfo", "audioplayer", "VideoPlayer", "task_manager", "File_copy",
        "file_cut", "notepad", "main", "K", "ffmpeg", "vlc", "cvlc", "zenity", "ffplay"
    };
    for (const auto& k : known) {
        // Use substring matching to handle /proc/ 15-char truncations
        if (name.find(k) != string::npos || k.find(name) != string::npos) return true;
    }
    return false;
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1400, 900), "MaqOS Kernel Panel â€” Elevated Privileges");
    window.setFramerateLimit(30);

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        // Fallback to system font
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            cerr << "[KernelPanel] Failed to load any font.\n";
            return -1;
        }
    }

    // ========================================
    // Kernel Authentication Screen
    // ========================================
    string kpass;
    bool authenticated = false;

    sf::Text authTitle("KERNEL MODE â€” Admin Authentication Required", font, 28);
    authTitle.setFillColor(sf::Color(255, 80, 80));
    authTitle.setPosition(300, 200);

    sf::Text authLabel("Enter Admin Password:", font, 20);
    authLabel.setFillColor(sf::Color::White);
    authLabel.setPosition(400, 320);

    sf::RectangleShape passBox(sf::Vector2f(400, 40));
    passBox.setPosition(400, 360);
    passBox.setFillColor(sf::Color(30, 30, 30));
    passBox.setOutlineColor(sf::Color::Red);
    passBox.setOutlineThickness(2);

    sf::Text passDisplay("", font, 20);
    passDisplay.setFillColor(sf::Color::White);
    passDisplay.setPosition(410, 365);

    sf::Text authError("", font, 18);
    authError.setFillColor(sf::Color::Red);
    authError.setPosition(400, 420);

    sf::Text authHint("Default password: admin | Press ESC to go back", font, 14);
    authHint.setFillColor(sf::Color(120, 120, 140));
    authHint.setPosition(420, 460);

    while (window.isOpen() && !authenticated) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) return 0;
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) return 0;
            if (event.type == sf::Event::TextEntered) {
                if (event.text.unicode == '\b' && !kpass.empty()) kpass.pop_back();
                else if (event.text.unicode == '\r') {
                    if (kpass == "admin") { authenticated = true; }
                    else { authError.setString("Access Denied. Try again."); kpass.clear(); }
                }
                else if (event.text.unicode < 128 && event.text.unicode >= 32) {
                    kpass += static_cast<char>(event.text.unicode);
                }
            }
        }
        passDisplay.setString(string(kpass.length(), '*'));

        window.clear(sf::Color(10, 10, 20));
        window.draw(authTitle);
        window.draw(authLabel);
        window.draw(passBox);
        window.draw(passDisplay);
        window.draw(authError);
        window.draw(authHint);
        window.display();
    }

    if (!window.isOpen()) return 0;

    // ========================================
    // Kernel Dashboard — Tabbed Interface
    // ========================================
    int activeTab = 0; // 0=User Tasks, 1=All Processes, 2=Memory, 3=System Log, 4=Commands
    vector<string> tabNames = {"User Tasks", "All Processes", "Memory", "System Log", "Commands"};

    sf::Clock refreshClock;
    vector<ProcInfo> procList;
    MemInfo memInfo;
    vector<string> logLines;
    int procScroll = 0;
    int selectedPid = -1;
    string killMsg;
    sf::Clock killMsgClock;

    // Initial data load
    procList = get_process_list();
    memInfo = get_mem_info();
    logLines = read_log_tail("maqos_system.log", 30);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) window.close();
                if (event.key.code == sf::Keyboard::F5) {
                    procList = get_process_list();
                    memInfo = get_mem_info();
                    logLines = read_log_tail("maqos_system.log", 30);
                }
                // Scroll process list
                if (event.key.code == sf::Keyboard::Up && procScroll > 0) procScroll--;
                if (event.key.code == sf::Keyboard::Down) procScroll++;
            }
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                float mx = event.mouseButton.x, my = event.mouseButton.y;
                // Tab clicks
                for (int i = 0; i < (int)tabNames.size(); i++) {
                    float tx = 10 + i * 200.f;
                    // Tabs are drawn at y=65, height=35. So y range is 65 to 100.
                    if (mx >= tx && mx <= tx + 190 && my >= 65 && my <= 100) activeTab = i;
                }
                
                // Process List (Tabs 0 and 1) clicks
                if ((activeTab == 0 || activeTab == 1) && my >= 115 + 25) {
                    int clickedRow = (my - 140) / 20;
                    if (clickedRow >= 0 && clickedRow < 32) { // 32 max visible rows
                        vector<ProcInfo> displayList;
                        if (activeTab == 0) {
                            for (auto& p : procList) if (is_maqos_task(p.name)) displayList.push_back(p);
                        } else {
                            displayList = procList;
                        }

                        int idx = procScroll + clickedRow;
                        if (idx >= 0 && idx < (int)displayList.size()) {
                            selectedPid = displayList[idx].pid;
                        }
                    }
                }

                // Commands (Tab 4) clicks
                if (activeTab == 4) {
                    // Kill Button
                    if (mx >= 50 && mx <= 350 && my >= 140 && my <= 190) {
                        if (selectedPid != -1) {
                            if (kill(selectedPid, SIGKILL) == 0) {
                                killMsg = "SIGKILL sent to PID " + to_string(selectedPid);
                                selectedPid = -1; // reset after kill
                            } else {
                                killMsg = "Failed to kill PID " + to_string(selectedPid) + " (Permission denied?)";
                            }
                        } else {
                            killMsg = "Please select a process from the 'Processes' tab first.";
                        }
                        killMsgClock.restart();
                        procList = get_process_list(); // refresh immediately
                    }
                    
                    // Clear Log Button
                    if (mx >= 50 && mx <= 350 && my >= 220 && my <= 270) {
                        ofstream clear_log("maqos_system.log", ios::trunc);
                        clear_log.close();
                        killMsg = "System log cleared successfully.";
                        killMsgClock.restart();
                        logLines = read_log_tail("maqos_system.log", 30);
                    }
                }
            }
        }

        // Auto-refresh every 3 seconds
        if (refreshClock.getElapsedTime().asSeconds() > 3.f) {
            procList = get_process_list();
            memInfo = get_mem_info();
            logLines = read_log_tail("maqos_system.log", 30);
            refreshClock.restart();
        }

        // ---- DRAW ----
        window.clear(sf::Color(10, 10, 20));

        // Header bar
        sf::RectangleShape header(sf::Vector2f(window.getSize().x, 60));
        header.setFillColor(sf::Color(40, 10, 10));
        window.draw(header);

        sf::Text headerTitle("MaqOS KERNEL PANEL - ELEVATED PRIVILEGES", font, 20);
        headerTitle.setFillColor(sf::Color(255, 100, 100));
        headerTitle.setPosition(10, 15);
        window.draw(headerTitle);

        // Tab bar
        for (int i = 0; i < (int)tabNames.size(); i++) {
            sf::RectangleShape tab(sf::Vector2f(190, 35));
            tab.setPosition(10 + i * 200.f, 65);
            tab.setFillColor(activeTab == i ? sf::Color(80, 30, 30) : sf::Color(40, 40, 50));
            tab.setOutlineColor(sf::Color(120, 50, 50));
            tab.setOutlineThickness(1);
            window.draw(tab);

            sf::Text tabText(tabNames[i], font, 16);
            tabText.setFillColor(activeTab == i ? sf::Color::White : sf::Color(150, 150, 160));
            tabText.setPosition(tab.getPosition().x + 20, tab.getPosition().y + 7);
            window.draw(tabText);
        }

        float contentY = 115;

        // ---- TAB 0 & 1: Process Lists ----
        if (activeTab == 0 || activeTab == 1) {
            vector<ProcInfo> displayList;
            if (activeTab == 0) {
                for (auto& p : procList) if (is_maqos_task(p.name)) displayList.push_back(p);
            } else {
                displayList = procList;
            }

            sf::Text colHeader("PID         NAME                              STATE    MEM (KB)", font, 14);
            colHeader.setFillColor(sf::Color(200, 200, 220));
            colHeader.setPosition(20, contentY);
            window.draw(colHeader);

            int maxVisible = 32;
            for (int i = procScroll; i < (int)displayList.size() && i < procScroll + maxVisible; i++) {
                auto& p = displayList[i];
                stringstream ss;
                ss << p.pid;
                string line = ss.str();
                while (line.size() < 12) line += " ";
                line += p.name;
                while (line.size() < 50) line += " ";
                line += p.state;
                while (line.size() < 59) line += " ";
                line += to_string(p.mem_kb);

                sf::Text row(line, font, 13);
                if (p.pid == selectedPid) {
                    // Highlight selected row
                    sf::RectangleShape highlight(sf::Vector2f(1360, 20));
                    highlight.setPosition(15, contentY + 25 + (i - procScroll) * 20);
                    highlight.setFillColor(sf::Color(80, 40, 40));
                    window.draw(highlight);
                    row.setFillColor(sf::Color::White);
                } else {
                    row.setFillColor(i % 2 == 0 ? sf::Color(180, 200, 180) : sf::Color(200, 180, 180));
                }
                
                row.setPosition(20, contentY + 25 + (i - procScroll) * 20);
                window.draw(row);
            }

            // Background for hint
            sf::RectangleShape hintBg(sf::Vector2f(1400, 40));
            hintBg.setPosition(0, window.getSize().y - 40);
            hintBg.setFillColor(sf::Color(10, 10, 20));
            window.draw(hintBg);

            sf::Text hint("Scroll: Up/Down arrows | F5: Refresh | Total: " + to_string(displayList.size()), font, 14);
            hint.setFillColor(sf::Color(100, 100, 120));
            hint.setPosition(20, window.getSize().y - 30);
            window.draw(hint);
        }

        // ---- TAB 2: Memory Info ----
        if (activeTab == 2) {
            auto drawBar = [&](const string& label, float used, float total, float y, sf::Color col) {
                sf::Text lbl(label, font, 18);
                lbl.setFillColor(sf::Color::White);
                lbl.setPosition(30, y);
                window.draw(lbl);

                sf::RectangleShape bg(sf::Vector2f(800, 30));
                bg.setPosition(30, y + 30);
                bg.setFillColor(sf::Color(40, 40, 50));
                window.draw(bg);

                float pct = (total > 0) ? used / total : 0;
                sf::RectangleShape bar(sf::Vector2f(800 * pct, 30));
                bar.setPosition(30, y + 30);
                bar.setFillColor(col);
                window.draw(bar);

                stringstream ss;
                ss << (long)used << " MB / " << (long)total << " MB (" << (int)(pct * 100) << "%)";
                sf::Text val(ss.str(), font, 14);
                val.setFillColor(sf::Color::White);
                val.setPosition(40, y + 35);
                window.draw(val);
            };

            drawBar("RAM Usage",  memInfo.used_mb,       memInfo.total_mb,      contentY + 20,  sf::Color(50, 150, 80));
            drawBar("Swap Usage", memInfo.swap_used_mb,   memInfo.swap_total_mb, contentY + 120, sf::Color(200, 150, 50));

            // CPU cores
            long cores = sysconf(_SC_NPROCESSORS_ONLN);
            sf::Text cpuText("CPU Cores: " + to_string(cores), font, 20);
            cpuText.setFillColor(sf::Color(150, 200, 255));
            cpuText.setPosition(30, contentY + 240);
            window.draw(cpuText);

            // Uptime
            struct sysinfo si;
            sysinfo(&si);
            int up_h = si.uptime / 3600;
            int up_m = (si.uptime % 3600) / 60;
            sf::Text uptime("System Uptime: " + to_string(up_h) + "h " + to_string(up_m) + "m", font, 20);
            uptime.setFillColor(sf::Color(150, 200, 255));
            uptime.setPosition(30, contentY + 280);
            window.draw(uptime);
        }

        // ---- TAB 3: System Log ----
        if (activeTab == 3) {
            sf::Text logTitle("System Log (maqos_system.log) — Last 30 entries", font, 16);
            logTitle.setFillColor(sf::Color(200, 200, 220));
            logTitle.setPosition(20, contentY);
            window.draw(logTitle);

            for (int i = 0; i < (int)logLines.size(); i++) {
                sf::Color lc = sf::Color(160, 180, 160);
                if (logLines[i].find("ERROR") != string::npos) lc = sf::Color(255, 100, 100);
                if (logLines[i].find("DEADLOCK") != string::npos) lc = sf::Color(255, 50, 50);
                if (logLines[i].find("GRANTED") != string::npos) lc = sf::Color(100, 255, 100);

                sf::Text logLine(logLines[i], font, 12);
                logLine.setFillColor(lc);
                logLine.setPosition(20, contentY + 30 + i * 18);
                window.draw(logLine);
            }
        }

        // ---- TAB 4: System Commands ----
        if (activeTab == 4) {
            sf::Text cmdTitle("Kernel Commands (Elevated System Control)", font, 22);
            cmdTitle.setFillColor(sf::Color(255, 120, 120));
            cmdTitle.setPosition(30, contentY + 20);
            window.draw(cmdTitle);

            // Kill button
            sf::RectangleShape killBtn(sf::Vector2f(300, 50));
            killBtn.setPosition(50, 140);
            killBtn.setFillColor(selectedPid != -1 ? sf::Color(180, 40, 40) : sf::Color(80, 40, 40));
            killBtn.setOutlineColor(sf::Color(255, 100, 100));
            killBtn.setOutlineThickness(2);
            window.draw(killBtn);

            string killLbl = (selectedPid != -1) ? "Kill Selected PID: " + to_string(selectedPid) : "Kill Process (None Selected)";
            sf::Text killText(killLbl, font, 18);
            killText.setFillColor(sf::Color::White);
            killText.setPosition(80, 155);
            window.draw(killText);
            
            // Clear Log button
            sf::RectangleShape logBtn(sf::Vector2f(300, 50));
            logBtn.setPosition(50, 220);
            logBtn.setFillColor(sf::Color(40, 100, 180));
            logBtn.setOutlineColor(sf::Color(100, 150, 255));
            logBtn.setOutlineThickness(2);
            window.draw(logBtn);

            sf::Text logText("Clear System Log", font, 18);
            logText.setFillColor(sf::Color::White);
            logText.setPosition(100, 235);
            window.draw(logText);

            // Status message
            if (!killMsg.empty() && killMsgClock.getElapsedTime().asSeconds() < 5.f) {
                sf::Text msg("Status: " + killMsg, font, 16);
                msg.setFillColor(sf::Color(255, 200, 100));
                msg.setPosition(50, 300);
                window.draw(msg);
            }

            // Info text
            sf::Text infoText(
                "Usage Instructions:\n\n"
                "  1. Go to the 'Processes' tab and click on a process to select it.\n"
                "  2. Return here to send a SIGKILL signal to forcefully terminate it.\n"
                "  3. You can also clear the system logs to free up space.\n\n"
                "Warning: Killing core OS processes may crash your Ubuntu environment.", font, 15);
            infoText.setFillColor(sf::Color(170, 170, 190));
            infoText.setPosition(50, 360);
            window.draw(infoText);
        }

        window.display();
    }

    return 0;
}

