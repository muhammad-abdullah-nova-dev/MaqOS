#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <pthread.h>
#include <cstdlib>
#include <csignal>
#include <vector>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <atomic>
#include <array>

#define APPS_NUM 20 // Updated to include all the required apps
using namespace std;

// Global RAM simulation variables
int MAX_RAM = 4096; // 4GB simulated RAM in MB
int used_ram = 0;
mutex ram_mutex;

bool show_f1_guide = false;

// Global HDD simulation variables
int MAX_HDD = 256 * 1024; // in MB
atomic<int> used_hdd(0);
mutex hdd_mutex;

// Logging
#include <fstream>
ofstream maqos_log;
mutex log_mutex;

void log_event(const string& msg) {
    lock_guard<mutex> lock(log_mutex);
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));
    maqos_log << "[" << buf << "] " << msg << "\n";
    maqos_log.flush();
}

bool allocate_hdd(int mb) {
    lock_guard<mutex> lock(hdd_mutex);
    if (used_hdd + mb > MAX_HDD) return false;
    used_hdd += mb;
    return true;
}

void free_hdd(int mb) {
    lock_guard<mutex> lock(hdd_mutex);
    used_hdd = max(0, used_hdd.load() - mb);
}

#include <semaphore.h>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <thread>

sem_t ram_sem;     // Binary semaphore — protects RAM allocation
sem_t core_sem;    // Counting semaphore — one slot per core

int max_threads;

struct ProcessEntry {
    pid_t pid;
    string name;
    int ram_mb;
    int hdd_mb;
    int priority;          // 0=highest, 5=lowest
    int burst_time;        // seconds (set per app)
    int wait_ticks;        // for aging
    time_t arrival_time;
    enum State { READY, RUNNING, MINIMIZED, BLOCKED, TERMINATED } state;
};

vector<ProcessEntry> process_table;
mutex proc_table_mutex;

queue<ProcessEntry> sys_queue;         // Level 0
auto cmp = [](const ProcessEntry& a, const ProcessEntry& b){ return a.priority > b.priority; };
priority_queue<ProcessEntry, vector<ProcessEntry>, decltype(cmp)> interactive_queue(cmp); // Level 1
queue<ProcessEntry> bg_queue;          // Level 2

mutex sched_mutex;
condition_variable sched_cv;
atomic<int> active_processes(0);
const int RR_QUANTUM_SECONDS = 2;
atomic<bool> shutdown_requested(false);

const int MIN_PRIORITY = 0;
const int AGING_THRESHOLD = 10;

enum OSMode { USER_MODE, KERNEL_MODE };
OSMode current_mode = USER_MODE;

string get_current_time();
bool allocate_ram(int amount);
void free_ram(int amount);

// Function to load textures safely
bool loadTextureFromFile(sf::Texture& texture, const string& filename) {
    if (!texture.loadFromFile("assets/images/" + filename)) {
        cerr << "Failed to load texture: " << filename << endl;
        return false;
    }
    return true;
}

int get_queue_level(const string& name) {
    if (name == "Clock" || name == "Calendar" || name == "TaskManager") return 0;
    if (name == "AudioPlayer" || name == "FileCopy" || name == "FileMove") return 2;
    return 1; // everything else is interactive
}

void apply_aging() {
    auto age_queue = [](queue<ProcessEntry>& q) {
        queue<ProcessEntry> temp;
        while (!q.empty()) {
            ProcessEntry p = q.front(); q.pop();
            p.wait_ticks++;
            if (p.wait_ticks % AGING_THRESHOLD == 0 && p.priority > MIN_PRIORITY) {
                p.priority--;
                log_event("AGING: " + p.name + " priority boosted to " + to_string(p.priority));
            }
            temp.push(p);
        }
        q = temp;
    };
    
    auto age_pq = [](priority_queue<ProcessEntry, vector<ProcessEntry>, decltype(cmp)>& pq) {
        vector<ProcessEntry> temp;
        while (!pq.empty()) {
            ProcessEntry p = pq.top(); pq.pop();
            p.wait_ticks++;
            if (p.wait_ticks % AGING_THRESHOLD == 0 && p.priority > MIN_PRIORITY) {
                p.priority--;
                log_event("AGING: " + p.name + " priority boosted to " + to_string(p.priority));
            }
            temp.push_back(p);
        }
        for (auto& p : temp) pq.push(p);
    };

    lock_guard<mutex> lock(sched_mutex);
    age_pq(interactive_queue);
    age_queue(bg_queue);
}

void context_switch(const string& from, const string& to) {
    cout << "[Kernel] Context Switch: " << from << " -> " << to << endl;
    log_event("CONTEXT SWITCH: " + from + " -> " + to);
}

void minimize_process(pid_t pid) {
    kill(pid, SIGSTOP);
    lock_guard<mutex> lock(proc_table_mutex);
    for (auto& p : process_table) {
        if (p.pid == pid) {
            p.state = ProcessEntry::MINIMIZED;
            log_event("MINIMIZED: " + p.name + " PID=" + to_string(pid));
        }
    }
}

void restore_process(pid_t pid) {
    kill(pid, SIGCONT);
    lock_guard<mutex> lock(proc_table_mutex);
    for (auto& p : process_table) {
        if (p.pid == pid) {
            p.state = ProcessEntry::RUNNING;
            log_event("RESTORED: " + p.name + " PID=" + to_string(pid));
        }
    }
}

void close_process(pid_t pid, int ram, int hdd) {
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, WNOHANG);
    free_ram(ram);
    free_hdd(hdd);
    log_event("CLOSED: PID=" + to_string(pid));
}

void simulate_io_interrupt(pid_t pid) {
    lock_guard<mutex> lock(proc_table_mutex);
    for (auto& p : process_table) {
        if (p.pid == pid && p.state == ProcessEntry::RUNNING) {
            p.state = ProcessEntry::BLOCKED;
            log_event("INTERRUPT: " + p.name + " moved to BLOCKED state (simulated)");
            string pname = p.name;
            thread([pname, pid]() {
                this_thread::sleep_for(chrono::seconds(3));
                // Simulated: mark as running again after 3s
                log_event("INTERRUPT RESOLVED: " + pname + " RESUMED (simulated)");
            }).detach();
        }
    }
}

void* maqos_scheduler(void* arg) {
    string last_scheduled = "OS_Idle";
    while (!shutdown_requested) {
        apply_aging();
        unique_lock<mutex> lock(sched_mutex);
        sched_cv.wait_for(lock, chrono::milliseconds(500), []{
            return !sys_queue.empty() || !interactive_queue.empty()
                || !bg_queue.empty() || shutdown_requested.load();
        });

        if (shutdown_requested) break;

        ProcessEntry next_entry;
        int level = -1;

        if (!sys_queue.empty()) { next_entry = sys_queue.front(); sys_queue.pop(); level = 0; }
        else if (!interactive_queue.empty()) { next_entry = interactive_queue.top(); interactive_queue.pop(); level = 1; }
        else if (!bg_queue.empty()) { next_entry = bg_queue.front(); bg_queue.pop(); level = 2; }

        if (level == -1) continue;

        // Simulated context switch — log it but don't SIGSTOP/SIGCONT GUI apps
        context_switch(last_scheduled, next_entry.name);
        last_scheduled = next_entry.name;

        {
            lock_guard<mutex> table_lock(proc_table_mutex);
            for(auto& p : process_table) {
                if(p.pid == next_entry.pid) p.state = ProcessEntry::RUNNING;
            }
        }

        if (level == 1) {
            // Simulated Round Robin quantum expiry
            string rr_name = next_entry.name;
            thread([rr_name]() {
                this_thread::sleep_for(chrono::seconds(RR_QUANTUM_SECONDS));
                log_event("RR QUANTUM EXPIRED: " + rr_name + " (simulated preemption)");
            }).detach();
        }

        log_event("SCHEDULED (Level " + to_string(level) + "): " + next_entry.name + " PID=" + to_string(next_entry.pid));
    }
    return nullptr;
}

// Deadlock Detection
enum Resource { FILE_SYSTEM = 0, AUDIO_DEVICE = 1, DISPLAY = 2, NUM_RESOURCES = 3 };
const string resource_names[] = {"FileSystem", "AudioDevice", "Display"};

vector<array<int, NUM_RESOURCES>> allocation;
vector<array<int, NUM_RESOURCES>> request;
array<int, NUM_RESOURCES> available = {2, 1, 3};

bool deadlock_dfs(int node, vector<vector<int>>& graph, vector<int>& visited, vector<int>& stack) {
    visited[node] = 1;
    stack[node]   = 1;
    for (int neighbor : graph[node]) {
        if (!visited[neighbor] && deadlock_dfs(neighbor, graph, visited, stack)) return true;
        if (stack[neighbor]) return true;
    }
    stack[node] = 0;
    return false;
}

void check_deadlock() {
    lock_guard<mutex> lock(proc_table_mutex);
    int n = process_table.size();
    if (n == 0) return;
    
    if (allocation.size() < (size_t)n) {
        std::array<int, NUM_RESOURCES> zero_arr = {0,0,0};
        allocation.resize(n, zero_arr);
        request.resize(n, zero_arr);
    }

    vector<vector<int>> graph(n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            for (int r = 0; r < NUM_RESOURCES; r++) {
                if (request[i][r] > 0 && allocation[j][r] > 0) {
                    graph[i].push_back(j);
                }
            }
        }
    }

    vector<int> visited(n, 0), stack(n, 0);
    vector<int> deadlocked;
    for (int i = 0; i < n; i++) {
        if (!visited[i] && deadlock_dfs(i, graph, visited, stack)) {
            deadlocked.push_back(i);
        }
    }

    if (!deadlocked.empty()) {
        string msg = "DEADLOCK DETECTED among processes: ";
        for (int idx : deadlocked) msg += process_table[idx].name + " ";
        cout << "\n[MaqOS] !! " << msg << "\n";
        log_event(msg);
    }
}

void* deadlock_monitor(void* arg) {
    while (!shutdown_requested) {
        this_thread::sleep_for(chrono::seconds(5));
        check_deadlock();
    }
    return nullptr;
}

pid_t fork_and_exec(const string& exe, const string& name, int ram, int hdd, int priority, bool is_console = false) {
    log_event("LAUNCH ATTEMPT: " + name + " exe=" + exe + " RAM=" + to_string(ram) + " HDD=" + to_string(hdd));

    // --- HARD THREAD LIMIT ENFORCEMENT ---
    if (active_processes.load() >= max_threads) {
        log_event("DENIED: " + name + " — thread limit reached (" + to_string(max_threads) + "/" + to_string(max_threads) + ")");
        cout << "[MaqOS] Cannot launch " << name << ": all " << max_threads << " thread slots are in use." << endl;
        return -1;
    }

    int req_pipe[2], ack_pipe[2];
    if (pipe(req_pipe) == -1 || pipe(ack_pipe) == -1) {
        log_event("ERROR: pipe() failed for " + name);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        log_event("ERROR: fork() failed for " + name);
        return -1;
    }

    if (pid == 0) {
        // === CHILD PROCESS ===
        // Detach from parent's session so SFML/X11 doesn't interfere
        setsid();

        // Close ALL inherited file descriptors (SFML, X11, OpenGL, etc.)
        // EXCEPT stdin(0), stdout(1), stderr(2), and our IPC pipes.
        int max_fd = (int)sysconf(_SC_OPEN_MAX);
        if (max_fd < 0 || max_fd > 4096) max_fd = 1024;
        for (int fd = 3; fd < max_fd; fd++) {
            if (fd != req_pipe[1] && fd != ack_pipe[0])
                close(fd);
        }

        // IPC handshake: send resource request to parent
        int req[2] = {ram, hdd};
        write(req_pipe[1], req, sizeof(req));
        close(req_pipe[1]);

        // Wait for parent's acknowledgment
        int ack = 0;
        read(ack_pipe[0], &ack, sizeof(ack));
        close(ack_pipe[0]);

        if (ack == 0) {
            _exit(0); // Resources denied
        }

        // Replace this process with the target app binary
        if (is_console) {
            // 1. Try xterm (keeps process foreground)
            execlp("xterm", "xterm", "-title", name.c_str(), "-e", exe.c_str(), nullptr);
            
            // 2. Fallback to gnome-terminal using --wait (so it doesn't daemonize and die instantly)
            execlp("gnome-terminal", "gnome-terminal", "--wait", "--", exe.c_str(), nullptr);
            
            // 3. Fallback to raw execution if emulators aren't installed
            execlp(exe.c_str(), exe.c_str(), nullptr);
        } else {
            execlp(exe.c_str(), exe.c_str(), nullptr);
        }
        // If we reach here, exec failed
        perror(("[MaqOS] execlp FAILED for " + name).c_str());
        _exit(1);

    } else {
        // === PARENT PROCESS ===
        close(req_pipe[1]); close(ack_pipe[0]);

        // Read child's resource request
        int req[2] = {0, 0};
        read(req_pipe[0], req, sizeof(req));
        close(req_pipe[0]);

        // Attempt to allocate resources
        int ack = 0;
        if (allocate_ram(req[0]) && allocate_hdd(req[1])) {
            ack = 1;
            log_event("RAM GRANTED: " + to_string(req[0]) + "MB, HDD: " + to_string(req[1]) + "MB to " + name);
        } else {
            log_event("RESOURCE DENIED for " + name + " — insufficient RAM/HDD");
        }

        // Send acknowledgment to child
        write(ack_pipe[1], &ack, sizeof(ack));
        close(ack_pipe[1]);

        if (ack == 1) {
            ProcessEntry entry;
            entry.pid          = pid;
            entry.name         = name;
            entry.ram_mb       = req[0];
            entry.hdd_mb       = req[1];
            entry.priority     = priority;
            entry.arrival_time = time(nullptr);
            entry.wait_ticks   = 0;
            entry.state        = ProcessEntry::RUNNING;
            {
                lock_guard<mutex> lock(proc_table_mutex);
                process_table.push_back(entry);
            }
            active_processes++;
            log_event("CREATED: " + name + " PID=" + to_string(pid) + " RAM=" + to_string(ram) + "MB");
            
            // Queue for simulated scheduling (app runs immediately, scheduler logs it)
            int ql = get_queue_level(name);
            {
                lock_guard<mutex> lock(sched_mutex);
                if (ql == 0) sys_queue.push(entry);
                else if (ql == 1) interactive_queue.push(entry);
                else bg_queue.push(entry);
            }
            sched_cv.notify_one();
            log_event("QUEUED (Level " + to_string(ql) + "): " + name + " PID=" + to_string(pid));
        }
        return (ack == 1) ? pid : -1;
    }
}

void reap_children() {
    lock_guard<mutex> lock(proc_table_mutex);
    for (auto& p : process_table) {
        if (p.state == ProcessEntry::TERMINATED) continue;
        int status;
        pid_t result = waitpid(p.pid, &status, WNOHANG);
        if (result > 0) {
            free_ram(p.ram_mb);
            free_hdd(p.hdd_mb);
            p.state = ProcessEntry::TERMINATED;
            active_processes--;
            sem_post(&core_sem);
            log_event("TERMINATED: " + p.name + " PID=" + to_string(p.pid));
        }
    }
    process_table.erase(
        remove_if(process_table.begin(), process_table.end(),
            [](const ProcessEntry& p){ return p.state == ProcessEntry::TERMINATED; }),
        process_table.end());
}

int MAX_RAM_GB = 4;
int MAX_HDD_GB = 256;

// Opens a native Linux file dialog using Zenity to select a wallpaper
string open_wallpaper_dialog() {
    char buffer[256];
    string result = "";
    FILE* pipe = popen("zenity --file-selection --title=\"Select Wallpaper\" --file-filter=\"Images | *.png *.jpg *.jpeg *.bmp\"", "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        cout << "Usage: ./maqos <RAM_GB> <HDD_GB> <Cores>\n";
        cout << "Example: ./maqos 2 256 8\n";
        return 1;
    }
    MAX_RAM_GB = atoi(argv[1]);
    MAX_HDD_GB = atoi(argv[2]);
    max_threads = atoi(argv[3]);

    MAX_RAM = MAX_RAM_GB * 1024; // convert to MB
    MAX_HDD = MAX_HDD_GB * 1024; // convert to MB

    cout << "\nMaqOS — Maqsad Operating System\n";
    cout << "Hardware: " << MAX_RAM_GB << "GB RAM | "
         << MAX_HDD_GB << "GB HDD | "
         << max_threads << " Cores\n\n";

    maqos_log.open("maqos_system.log", ios::app);
    maqos_log << "==============================================\n";
    maqos_log << "  MaqOS — Maqsad Operating System v1.0\n";
    maqos_log << "  Boot time: " << get_current_time() << "\n";
    maqos_log << "  Hardware: " << MAX_RAM_GB << "GB RAM | "
              << MAX_HDD_GB << "GB HDD | " << max_threads << " Cores\n";
    maqos_log << "==============================================\n";

    sem_init(&ram_sem, 0, 1);
    sem_init(&core_sem, 0, max_threads);

    pthread_t sched_thread;
    pthread_create(&sched_thread, nullptr, maqos_scheduler, nullptr);
    pthread_t dl_thread;
    pthread_create(&dl_thread, nullptr, deadlock_monitor, nullptr);

    // Create SFML window with requested dimensions and default style (allows maximizing)
    sf::RenderWindow window(sf::VideoMode(1280, 720), "MaqOS — Maqsad Operating System", sf::Style::Default);
    window.setFramerateLimit(60);

    // Load space-themed wallpaper
    sf::Texture wallpaperTexture;
    sf::Sprite wallpaper;
    if (!loadTextureFromFile(wallpaperTexture, "maqos_wallpaper.png")) {
        // Create a starry background as fallback
        cout << "Creating starry night background as fallback" << endl;
        sf::Image starryBg;
        starryBg.create(1280, 720, sf::Color(10, 10, 30)); // Dark blue base
        
        // Add some stars
        srand(time(NULL));
        for (int i = 0; i < 200; i++) {
            int x = rand() % 1280;
            int y = rand() % 720;
            int brightness = 150 + rand() % 105; // 150-255
            starryBg.setPixel(x, y, sf::Color(brightness, brightness, brightness));
        }
        
        wallpaperTexture.loadFromImage(starryBg);
    }
    
    wallpaper.setTexture(wallpaperTexture);
    // Scale to fit window
    float scaleX = static_cast<float>(window.getSize().x) / wallpaperTexture.getSize().x;
    float scaleY = static_cast<float>(window.getSize().y) / wallpaperTexture.getSize().y;
    wallpaper.setScale(scaleX, scaleY);

    // App data structure - stores app name and icon filename pairs
    // Using vector of pairs instead of map for more control over ordering
    vector<pair<string, string>> app_data = {
        {"Calculator", "calculator.png"},
        {"Calendar", "Calendar.png"},
        {"Clock", "clock.png"},
        {"Stopwatch", "stopWatch.png"},
        {"Minesweeper", "Minesweeper.png"},
        {"Chess", "chess.png"},
        {"Tic Tac Toc", "ticTacToc.png"},
        {"Number Guess", "numberGuessGame.png"},
        {"Snake", "snake.png"},
        {"File Create", "writeFile.png"},
        {"File Delete", "fileDelete.png"},
        {"File Info", "fileInfo.png"},
        {"Audio Player", "audioPlayer.png"},
        {"Video Player", "videoPlayer.png"},
        {"Task Manager", "TaskManager.png"},
        {"File Copy", "fileCopy.png"},
        {"File Move", "fileMove.png"},
        {"Notepad", "Notepad.png"},
        {"Browser", "browser.png"}
    };
    
    // Load app icons
    sf::Texture textures[APPS_NUM];
    sf::Sprite sprites[APPS_NUM];
    
    // Load all textures and ensure consistent sizing
    const float ICON_SIZE = 64.0f; // Fixed size for all icons
    
    for (size_t i = 0; i < app_data.size(); i++) {
        if (!loadTextureFromFile(textures[i], app_data[i].second)) {
            // Create a default colored square if texture fails to load
            textures[i].create(64, 64);
            sf::Image img;
            img.create(64, 64, sf::Color(100, 100, 255));
            textures[i].update(img);
        }
        sprites[i].setTexture(textures[i]);
        
        // Calculate scale to make all icons the same size
        float scaleFactorX = ICON_SIZE / textures[i].getSize().x;
        float scaleFactorY = ICON_SIZE / textures[i].getSize().y;
        sprites[i].setScale(scaleFactorX, scaleFactorY);
    }
    
    // Position app icons in a grid layout
    const int ICONS_PER_ROW = 6;
    const int HORIZONTAL_SPACING = 150;
    const int VERTICAL_SPACING = 150;
    const int START_X = (window.getSize().x - (ICONS_PER_ROW - 1) * HORIZONTAL_SPACING) / 2;
    const int START_Y = 100;
    
    for (size_t i = 0; i < app_data.size(); i++) {
        int row = i / ICONS_PER_ROW;
        int col = i % ICONS_PER_ROW;
        
        float x = START_X + col * HORIZONTAL_SPACING;
        float y = START_Y + row * VERTICAL_SPACING;
        
        // Center the icon at this position
        sf::FloatRect bounds = sprites[i].getGlobalBounds();
        sprites[i].setPosition(x - bounds.width / 2, y - bounds.height / 2);
    }
    
    // Load font
    sf::Font font;
    if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
        cerr << "Failed to load font. Text will not be displayed." << endl;
    }
    
    // Create text for app names and filenames with improved visibility
    vector<sf::Text> app_labels;
    vector<sf::Text> file_labels;
    
    for (size_t i = 0; i < app_data.size(); i++) {
        // App name labels
        sf::Text app_label;
        app_label.setFont(font);
        app_label.setString(app_data[i].first);
        app_label.setCharacterSize(14);
        app_label.setFillColor(sf::Color::White);
        app_label.setOutlineColor(sf::Color::Black);
        app_label.setOutlineThickness(1.0f);
        
        // File name labels
        sf::Text file_label;
        file_label.setFont(font);
        file_label.setString(app_data[i].second);
        file_label.setCharacterSize(12);
        file_label.setFillColor(sf::Color(200, 200, 255)); // Light blue color
        file_label.setOutlineColor(sf::Color::Black);
        file_label.setOutlineThickness(0.5f);
        
        // Position text below icon
        sf::FloatRect iconBounds = sprites[i].getGlobalBounds();
        app_label.setPosition(
            iconBounds.left + (iconBounds.width - app_label.getLocalBounds().width) / 2,
            iconBounds.top + iconBounds.height + 5
        );
        
        file_label.setPosition(
            iconBounds.left + (iconBounds.width - file_label.getLocalBounds().width) / 2,
            iconBounds.top + iconBounds.height + 25
        );
        
        app_labels.push_back(app_label);
        file_labels.push_back(file_label);
    }
    
    // Create clock display
    sf::Text clockDisplay;
    clockDisplay.setFont(font);
    clockDisplay.setCharacterSize(16); // Smaller for taskbar
    clockDisplay.setFillColor(sf::Color::White);
    clockDisplay.setOutlineColor(sf::Color::Black);
    clockDisplay.setOutlineThickness(1.0f);
    // position will be dynamically updated to the right side of the taskbar
    
    // Create system stats display (threads and RAM)
    sf::Text systemStats;
    systemStats.setFont(font);
    systemStats.setCharacterSize(18);
    systemStats.setFillColor(sf::Color::White);
    systemStats.setOutlineColor(sf::Color::Black);
    systemStats.setOutlineThickness(1.0f);
    systemStats.setPosition(20, 20);
    
    // Auto-start essential system tasks
    fork_and_exec("./bin/clock", "Clock", 40, 2, 0);
    fork_and_exec("./bin/simple_calendar", "Calendar", 60, 5, 0);

    bool show_start_menu = false;
    sf::Texture startLogoTex;
    bool hasStartLogo = loadTextureFromFile(startLogoTex, "MaqOS.png");

    // Main loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
                float scaleX = static_cast<float>(event.size.width) / wallpaperTexture.getSize().x;
                float scaleY = static_cast<float>(event.size.height) / wallpaperTexture.getSize().y;
                wallpaper.setScale(scaleX, scaleY);
            }
            
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(
                        event.mouseButton.x, event.mouseButton.y));
                    
                    // --- TASKBAR INTERACTION LOGIC ---
                    float taskbarHeight = 48.f;
                    if (mousePos.y >= window.getSize().y - taskbarHeight) {
                        vector<int> runningAppIndices;
                        vector<pid_t> runningAppPids;
                        {
                            lock_guard<mutex> lock(proc_table_mutex);
                            for (auto& p : process_table) {
                                for (size_t i = 0; i < app_data.size(); i++) {
                                    if (app_data[i].first == p.name) {
                                        runningAppIndices.push_back(i);
                                        runningAppPids.push_back(p.pid);
                                        break;
                                    }
                                }
                            }
                        }
                        
                        float totalIconsWidth = (runningAppIndices.size() + 1) * 44.f;
                        float startX = (window.getSize().x - totalIconsWidth) / 2.f;
                        
                        // Check Start Button Click
                        sf::FloatRect startBtnRect(startX, window.getSize().y - 42.f, 36.f, 36.f);
                        if (startBtnRect.contains(mousePos)) {
                            show_start_menu = !show_start_menu;
                            log_event("START BUTTON CLICKED");
                        } else {
                            // If clicked taskbar but NOT start button, close start menu
                            show_start_menu = false;
                        }
                        
                        // Check Running Apps Click
                        float currentX = startX + 44.f;
                        for (size_t k = 0; k < runningAppIndices.size(); k++) {
                            sf::FloatRect iconRect(currentX, window.getSize().y - 40.f, 32.f, 32.f);
                            if (iconRect.contains(mousePos)) {
                                lock_guard<mutex> lock(proc_table_mutex);
                                for (auto& p : process_table) {
                                    if (p.pid == runningAppPids[k]) {
                                        if (p.state == ProcessEntry::RUNNING) minimize_process(p.pid);
                                        else if (p.state == ProcessEntry::MINIMIZED) restore_process(p.pid);
                                        break;
                                    }
                                }
                                break;
                            }
                            currentX += 44.f;
                        }
                        continue; // Skip desktop icon checks since we clicked the taskbar
                    }
                    // ---------------------------------
                    
                    // Check which app was clicked on desktop (only if start menu is closed or clicked outside)
                    bool clickedDesktop = true;
                    
                    // --- START MENU INTERACTION LOGIC ---
                    if (show_start_menu) {
                        float menuWidth = 560.f;
                        float menuHeight = 640.f;
                        float menuX = (window.getSize().x - menuWidth) / 2.f;
                        float menuY = window.getSize().y - 52.f - menuHeight - 20.f;
                        sf::FloatRect startMenuRect(menuX, menuY, menuWidth, menuHeight);
                        
                        if (startMenuRect.contains(mousePos)) {
                            clickedDesktop = false;
                            // Check power button
                            sf::FloatRect pwrBtnRect(menuX + menuWidth - 55, menuY + menuHeight - 50, 40.f, 40.f);
                            if (pwrBtnRect.contains(mousePos)) {
                                window.close(); // SHUTDOWN
                            }
                            
                            // Check Change Wallpaper button
                            sf::FloatRect wpBtnRect(menuX + menuWidth - 200, menuY + menuHeight - 48, 130.f, 32.f);
                            if (wpBtnRect.contains(mousePos)) {
                                show_start_menu = false; // Hide menu so file dialog is visible
                                string newPath = open_wallpaper_dialog();
                                if (!newPath.empty()) {
                                    if (wallpaperTexture.loadFromFile(newPath)) {
                                        float scaleX = static_cast<float>(window.getSize().x) / wallpaperTexture.getSize().x;
                                        float scaleY = static_cast<float>(window.getSize().y) / wallpaperTexture.getSize().y;
                                        wallpaper.setScale(scaleX, scaleY);
                                    }
                                }
                            }

                            // Check app clicks
                            int smCols = 6;
                            for (size_t i = 0; i < app_data.size(); i++) {
                                int row = i / smCols;
                                int col = i % smCols;
                                sf::FloatRect iconRect(menuX + 45 + col * 82, menuY + 130 + row * 90, 48.f, 48.f);
                                if (iconRect.contains(mousePos)) {
                                    show_start_menu = false; // close menu
                                    switch(i) {
                                        case 0: fork_and_exec("./bin/calculator", app_data[i].first, 80,  5,  2); break;
                                        case 1: fork_and_exec("./bin/simple_calendar", app_data[i].first, 60,  5,  0); break;
                                        case 2: fork_and_exec("./bin/clock",      app_data[i].first,      40,  2,  0); break;
                                        case 3: fork_and_exec("./bin/stopWatch",  app_data[i].first,  50,  2,  2); break;
                                        case 4: fork_and_exec("./bin/minesweeper",app_data[i].first,120,10,  2); break;
                                        case 5: fork_and_exec("./bin/chess",      app_data[i].first,      150, 15, 2); break;
                                        case 6: fork_and_exec("./bin/ticTacToc",  app_data[i].first,  80,  8,  2); break;
                                        case 7: fork_and_exec("./bin/numberGuess",app_data[i].first,60,  5,  2); break;
                                        case 8: fork_and_exec("./bin/snake",      app_data[i].first,      100, 8,  2); break;
                                        case 9: fork_and_exec("./bin/fileCreate", app_data[i].first, 70,  10, 4); break;
                                        case 10:fork_and_exec("./bin/fileDelete", app_data[i].first, 70,  5,  4); break;
                                        case 11:fork_and_exec("./bin/fileInfo",   app_data[i].first,   60,  5,  4); break;
                                        case 12:fork_and_exec("./bin/audioplayer",app_data[i].first,100, 30, 4); break;
                                        case 13:fork_and_exec("./bin/VideoPlayer",app_data[i].first,300,100, 2); break;
                                        case 14:fork_and_exec("./bin/task_manager",app_data[i].first,120,5, 1); break;
                                        case 15:fork_and_exec("./bin/File_copy",  app_data[i].first,   150, 50, 4); break;
                                        case 16:fork_and_exec("./bin/file_cut",   app_data[i].first,   150, 50, 4); break;
                                        case 17:fork_and_exec("./bin/notepad",    app_data[i].first,    100, 20, 2); break;
                                        case 18:fork_and_exec("./bin/browse",     app_data[i].first,    200, 50, 4); break;
                                    }
                                    break;
                                }
                            }
                        } else {
                            // Clicked outside, close menu
                            show_start_menu = false;
                        }
                    }
                    
                    if (clickedDesktop) {
                        for (size_t i = 0; i < app_data.size(); i++) {
                            if (sprites[i].getGlobalBounds().contains(mousePos)) {
                                switch(i) {
                                    case 0: fork_and_exec("./bin/calculator", app_data[i].first, 80,  5,  2); break;
                                    case 1: fork_and_exec("./bin/simple_calendar", app_data[i].first, 60,  5,  0); break;
                                    case 2: fork_and_exec("./bin/clock",      app_data[i].first,      40,  2,  0); break;
                                    case 3: fork_and_exec("./bin/stopWatch",  app_data[i].first,  50,  2,  2); break;
                                    case 4: fork_and_exec("./bin/minesweeper",app_data[i].first,120,10,  2); break;
                                    case 5: fork_and_exec("./bin/chess",      app_data[i].first,      150, 15, 2); break;
                                    case 6: fork_and_exec("./bin/ticTacToc",  app_data[i].first,  80,  8,  2); break;
                                    case 7: fork_and_exec("./bin/numberGuess",app_data[i].first,60,  5,  2); break;
                                    case 8: fork_and_exec("./bin/snake",      app_data[i].first,      100, 8,  2); break;
                                    case 9: fork_and_exec("./bin/fileCreate", app_data[i].first, 70,  10, 4); break;
                                    case 10:fork_and_exec("./bin/fileDelete", app_data[i].first, 70,  5,  4); break;
                                    case 11:fork_and_exec("./bin/fileInfo",   app_data[i].first,   60,  5,  4); break;
                                    case 12:fork_and_exec("./bin/audioplayer",app_data[i].first,100, 30, 4); break;
                                    case 13:fork_and_exec("./bin/VideoPlayer",app_data[i].first,300,100, 2); break;
                                    case 14:fork_and_exec("./bin/task_manager",app_data[i].first,120,5, 1); break;
                                    case 15:fork_and_exec("./bin/File_copy",  app_data[i].first,   150, 50, 4); break;
                                    case 16:fork_and_exec("./bin/file_cut",   app_data[i].first,   150, 50, 4); break;
                                    case 17:fork_and_exec("./bin/notepad",    app_data[i].first,    100, 20, 2); break;
                                    case 18:fork_and_exec("./bin/browse",     app_data[i].first,    200, 50, 4); break;
                                }
                                break;
                            }
                        }
                    }
                } else if (event.mouseButton.button == sf::Mouse::Right) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(
                        event.mouseButton.x, event.mouseButton.y));
                        
                    // --- TASKBAR RIGHT CLICK TO CLOSE APP ---
                    float taskbarHeight = 48.f;
                    if (mousePos.y >= window.getSize().y - taskbarHeight) {
                        vector<int> runningAppIndices;
                        vector<pid_t> runningAppPids;
                        {
                            lock_guard<mutex> lock(proc_table_mutex);
                            for (auto& p : process_table) {
                                for (size_t i = 0; i < app_data.size(); i++) {
                                    if (app_data[i].first == p.name) {
                                        runningAppIndices.push_back(i);
                                        runningAppPids.push_back(p.pid);
                                        break;
                                    }
                                }
                            }
                        }
                        
                        float totalIconsWidth = (runningAppIndices.size() + 1) * 44.f;
                        float startX = (window.getSize().x - totalIconsWidth) / 2.f;
                        float currentX = startX + 44.f; // Skip start button
                        
                        for (size_t k = 0; k < runningAppIndices.size(); k++) {
                            sf::FloatRect iconRect(currentX, window.getSize().y - 40.f, 32.f, 32.f);
                            if (iconRect.contains(mousePos)) {
                                // Right click on taskbar icon -> CLOSE APP
                                log_event("KILL SIGNAL SENT VIA TASKBAR TO PID " + to_string(runningAppPids[k]));
                                kill(runningAppPids[k], SIGTERM);
                                break;
                            }
                            currentX += 44.f;
                        }
                        continue; // Skip desktop icon checks
                    }
                    // ----------------------------------------

                    for (size_t i = 0; i < app_data.size(); i++) {
                        if (sprites[i].getGlobalBounds().contains(mousePos)) {
                            // Find running process and show context options or just toggle minimize
                            lock_guard<mutex> lock(proc_table_mutex);
                            for (auto& p : process_table) {
                                if (p.name == app_data[i].first) {
                                    if (p.state == ProcessEntry::RUNNING) minimize_process(p.pid);
                                    else if (p.state == ProcessEntry::MINIMIZED) restore_process(p.pid);
                                    break;
                                }
                            }
                        }
                    }
                }
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::I) {
                    lock_guard<mutex> lock(proc_table_mutex);
                    for (auto& p : process_table) {
                        if (p.state == ProcessEntry::RUNNING) {
                            simulate_io_interrupt(p.pid);
                            break;
                        }
                    }
                } else if (event.key.code == sf::Keyboard::F1) {
                    show_f1_guide = !show_f1_guide;
                }
            }
        }

        // Kernel mode toggle checking
        if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            sf::Vector2i mPos = sf::Mouse::getPosition(window);
            sf::FloatRect kBtnRect(window.getSize().x - 260, window.getSize().y - 42.f, 140.f, 36.f);
            if (kBtnRect.contains(sf::Vector2f(mPos.x, mPos.y))) {
                static sf::Clock clickClock;
                if (clickClock.getElapsedTime().asSeconds() > 0.5f) {
                    current_mode = (current_mode == USER_MODE) ? KERNEL_MODE : USER_MODE;
                    log_event(current_mode == KERNEL_MODE ? "Switched to KERNEL MODE" : "Switched to USER MODE");
                    if (current_mode == KERNEL_MODE) {
                        fork_and_exec("./bin/K", "KernelPanel", 50, 5, 0);
                    }
                    clickClock.restart();
                }
            }
        }

        // Clean up finished threads
        reap_children();

        // Update clock text with better formatting
        time_t now = time(0);
        tm* ltm = localtime(&now);
        
        const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        
        stringstream ss;
        ss << setw(2) << setfill('0') << ltm->tm_hour << ":"
           << setw(2) << setfill('0') << ltm->tm_min << ":"
           << setw(2) << setfill('0') << ltm->tm_sec << "\n"
           << dayNames[ltm->tm_wday] << ", " << ltm->tm_mday << " " << monthNames[ltm->tm_mon];
        
        clockDisplay.setCharacterSize(13);
        clockDisplay.setString(ss.str());
        
        // --- DRAW DESKTOP BACKGROUND & TITLE ---
        window.clear();
        window.draw(wallpaper);

        sf::Text osTitle("MaqOS Nexus v2.0", font, 24);
        osTitle.setFillColor(sf::Color(255, 255, 255, 120));
        osTitle.setPosition((window.getSize().x - osTitle.getLocalBounds().width) / 2.f, 20);
        window.draw(osTitle);

        // --- DRAW SYSTEM DASHBOARD (Top Left) ---
        sf::RectangleShape dash(sf::Vector2f(280, 160));
        dash.setPosition(25, 25);
        dash.setFillColor(sf::Color(20, 20, 30, 180));
        dash.setOutlineThickness(1);
        dash.setOutlineColor(sf::Color(255, 255, 255, 30));
        window.draw(dash);

        sf::Text dashTitle("SYSTEM MONITOR", font, 12);
        dashTitle.setFillColor(sf::Color(0, 160, 255));
        dashTitle.setStyle(sf::Text::Bold);
        dashTitle.setPosition(40, 40);
        window.draw(dashTitle);

        // RAM Gauge
        float ramPct = min(1.0f, (float)used_ram / MAX_RAM);
        sf::RectangleShape ramBg(sf::Vector2f(200, 4));
        ramBg.setPosition(40, 75);
        ramBg.setFillColor(sf::Color(60, 60, 80));
        window.draw(ramBg);
        sf::RectangleShape ramFg(sf::Vector2f(200 * ramPct, 4));
        ramFg.setPosition(40, 75);
        ramFg.setFillColor(sf::Color(0, 255, 150));
        window.draw(ramFg);
        sf::Text ramVal("RAM: " + to_string(used_ram) + "MB", font, 11);
        ramVal.setPosition(40, 85);
        window.draw(ramVal);

        // CPU/Threads Gauge
        float cpuPct = min(1.0f, (float)active_processes.load() / max_threads);
        sf::RectangleShape cpuBg(sf::Vector2f(200, 4));
        cpuBg.setPosition(40, 115);
        cpuBg.setFillColor(sf::Color(60, 60, 80));
        window.draw(cpuBg);
        sf::RectangleShape cpuFg(sf::Vector2f(200 * cpuPct, 4));
        cpuFg.setPosition(40, 115);
        cpuFg.setFillColor(sf::Color(255, 100, 0));
        window.draw(cpuFg);
        sf::Text cpuVal("Threads: " + to_string(active_processes.load()) + " / " + to_string(max_threads), font, 11);
        cpuVal.setPosition(40, 125);
        window.draw(cpuVal);

        // --- DRAW DESKTOP ICONS ---
        for (size_t i = 0; i < app_data.size(); i++) {
            sf::Vector2i mPos = sf::Mouse::getPosition(window);
            if (sprites[i].getGlobalBounds().contains(sf::Vector2f(mPos.x, mPos.y))) {
                sf::RectangleShape hov(sf::Vector2f(80, 80));
                hov.setOrigin(40, 40);
                hov.setPosition(sprites[i].getPosition().x + 32, sprites[i].getPosition().y + 32);
                hov.setFillColor(sf::Color(255, 255, 255, 25));
                window.draw(hov);
            }
            window.draw(sprites[i]);
            
            sf::Text label(app_data[i].first, font, 11);
            label.setOutlineThickness(1);
            label.setOutlineColor(sf::Color(0, 0, 0, 180));
            label.setPosition(
                sprites[i].getPosition().x + (64.f - label.getLocalBounds().width) / 2.f,
                sprites[i].getPosition().y + 70.f
            );
            window.draw(label);
        }
        
        // === DRAW TASKBAR (Premium Glassmorphism) ===
        float taskbarHeight = 52.f;
        sf::RectangleShape taskbar(sf::Vector2f(window.getSize().x, taskbarHeight));
        taskbar.setPosition(0, window.getSize().y - taskbarHeight);
        taskbar.setFillColor(sf::Color(15, 15, 25, 220)); // Frosted glass
        taskbar.setOutlineThickness(1);
        taskbar.setOutlineColor(sf::Color(255, 255, 255, 30));
        window.draw(taskbar);

        // System Tray (Right Side)
        sf::RectangleShape trayPanel(sf::Vector2f(220, 36));
        trayPanel.setPosition(window.getSize().x - 230, window.getSize().y - 44.f);
        trayPanel.setFillColor(sf::Color(255, 255, 255, 15));
        window.draw(trayPanel);

        clockDisplay.setPosition(window.getSize().x - 110, window.getSize().y - 46.f);
        window.draw(clockDisplay);

        // Mode Indicator (Tray)
        sf::CircleShape modeDot(5);
        modeDot.setPosition(window.getSize().x - 215, window.getSize().y - 28.f);
        modeDot.setFillColor(current_mode == KERNEL_MODE ? sf::Color(255, 50, 50) : sf::Color(50, 255, 50));
        window.draw(modeDot);

        sf::Text kText(current_mode == KERNEL_MODE ? "KERNEL" : "USER", font, 12);
        kText.setPosition(window.getSize().x - 200, window.getSize().y - 32.f);
        window.draw(kText);

        // Collect running apps
        vector<int> runningAppIndices;
        {
            lock_guard<mutex> lock(proc_table_mutex);
            for (auto& p : process_table) {
                for (size_t i = 0; i < app_data.size(); i++) {
                    if (app_data[i].first == p.name) {
                        runningAppIndices.push_back(i);
                        break;
                    }
                }
            }
        }

        // --- Centered Dock (Windows 11 Style) ---
        float totalIconsWidth = (runningAppIndices.size() + 1) * 48.f;
        float dockX = (window.getSize().x - totalIconsWidth) / 2.f;
        float dockY = window.getSize().y - 46.f;

        // Start Button
        sf::RectangleShape startBtn(sf::Vector2f(40.f, 40.f));
        startBtn.setPosition(dockX, dockY);
        startBtn.setFillColor(show_start_menu ? sf::Color(255, 255, 255, 40) : sf::Color::Transparent);
        window.draw(startBtn);

        // Start Logo
        float bx = dockX + 10.f; float by = dockY + 10.f;
        float bs = 8.f; float sp = 11.f;
        sf::RectangleShape b1(sf::Vector2f(bs, bs)); b1.setPosition(bx, by); b1.setFillColor(sf::Color(0, 160, 255));
        sf::RectangleShape b2(sf::Vector2f(bs, bs)); b2.setPosition(bx+sp, by); b2.setFillColor(sf::Color(100, 100, 255));
        sf::RectangleShape b3(sf::Vector2f(bs, bs)); b3.setPosition(bx, by+sp); b3.setFillColor(sf::Color(0, 200, 200));
        sf::RectangleShape b4(sf::Vector2f(bs, bs)); b4.setPosition(bx+sp, by+sp); b4.setFillColor(sf::Color(150, 50, 255));
        window.draw(b1); window.draw(b2); window.draw(b3); window.draw(b4);

        dockX += 48.f;

        // Running Apps in Dock
        {
            lock_guard<mutex> lock(proc_table_mutex);
            for (int idx : runningAppIndices) {
                sf::Sprite tbIcon(textures[idx]);
                tbIcon.setScale(32.f/textures[idx].getSize().x, 32.f/textures[idx].getSize().y);
                tbIcon.setPosition(dockX + 4, dockY + 4);
                window.draw(tbIcon);

                sf::RectangleShape indicator(sf::Vector2f(12, 3));
                indicator.setFillColor(sf::Color(0, 150, 255));
                indicator.setPosition(dockX + 14, window.getSize().y - 5);
                window.draw(indicator);
                dockX += 48.f;
            }
        }

        // === DRAW WINDOWS 11 STYLE START MENU (Overhauled) ===
        if (show_start_menu) {
            float menuW = 560.f; float menuH = 640.f;
            float mx = (window.getSize().x - menuW) / 2.f;
            float my = window.getSize().y - 52.f - menuH - 20.f;
            
            sf::RectangleShape smPanel(sf::Vector2f(menuW, menuH));
            smPanel.setPosition(mx, my);
            smPanel.setFillColor(sf::Color(25, 25, 35, 250));
            smPanel.setOutlineThickness(1);
            smPanel.setOutlineColor(sf::Color(80, 80, 100));
            window.draw(smPanel);

            // Search Bar (Mockup)
            sf::RectangleShape search(sf::Vector2f(menuW - 60, 40));
            search.setPosition(mx + 30, my + 30);
            search.setFillColor(sf::Color(45, 45, 60));
            window.draw(search);
            sf::Text st("Type here to search...", font, 14);
            st.setFillColor(sf::Color(150, 150, 170));
            st.setPosition(mx + 45, my + 40);
            window.draw(st);

            sf::Text pLabel("Pinned", font, 16);
            pLabel.setStyle(sf::Text::Bold);
            pLabel.setPosition(mx + 35, my + 90);
            window.draw(pLabel);

            // App Grid
            int cols = 6;
            for (size_t i = 0; i < app_data.size(); i++) {
                int r = i / cols; int c = i % cols;
                sf::Sprite ic(textures[i]);
                ic.setScale(40.f/textures[i].getSize().x, 40.f/textures[i].getSize().y);
                ic.setPosition(mx + 45 + c * 82, my + 130 + r * 90);
                window.draw(ic);

                sf::Text nm(app_data[i].first, font, 10);
                nm.setPosition(ic.getPosition().x + (40-nm.getLocalBounds().width)/2, ic.getPosition().y + 45);
                window.draw(nm);
            }

            // User Bar (Bottom)
            sf::RectangleShape uBar(sf::Vector2f(menuW, 64));
            uBar.setPosition(mx, my + menuH - 64);
            uBar.setFillColor(sf::Color(20, 20, 30));
            window.draw(uBar);

            sf::CircleShape uPic(16);
            uPic.setPosition(mx + 25, my + menuH - 48);
            uPic.setFillColor(sf::Color(0, 150, 255));
            window.draw(uPic);

            sf::Text uNm("Admin User", font, 14);
            uNm.setPosition(mx + 65, my + menuH - 42);
            window.draw(uNm);

            // Change Wallpaper Button
            sf::RectangleShape wpBtn(sf::Vector2f(130, 32));
            wpBtn.setPosition(mx + menuW - 200, my + menuH - 48);
            wpBtn.setFillColor(sf::Color(50, 100, 150));
            window.draw(wpBtn);
            sf::Text wpText("Set Wallpaper", font, 12);
            wpText.setPosition(wpBtn.getPosition().x + 25, wpBtn.getPosition().y + 8);
            window.draw(wpText);

            // Power Icon (Integrated into User Bar)
            sf::CircleShape pwr(14);
            pwr.setPosition(mx + menuW - 50, my + menuH - 46);
            pwr.setFillColor(sf::Color(200, 50, 50));
            window.draw(pwr);
            sf::Text pI("!", font, 18);
            pI.setStyle(sf::Text::Bold);
            pI.setPosition(mx + menuW - 41, my + menuH - 47);
            window.draw(pI);
        }

        // Draw F1 Guide Overlay
        if (show_f1_guide) {
            sf::RectangleShape overlay(sf::Vector2f(600, 400));
            overlay.setPosition((window.getSize().x - 600) / 2.f, (window.getSize().y - 400) / 2.f);
            overlay.setFillColor(sf::Color(20, 20, 20, 230));
            overlay.setOutlineColor(sf::Color::White);
            overlay.setOutlineThickness(2.f);
            window.draw(overlay);

            sf::Text guideText(
                "MaqOS Instruction Guide\n\n"
                "- Left Click Icon: Launch App\n"
                "- Right Click Icon: Minimize/Restore running app\n"
                "- Press 'I': Simulate I/O Interrupt for a running process\n"
                "- Press 'F1': Toggle this menu\n"
                "- Kernel Mode Button: Bottom Right to access Kernel Panel\n\n"
                "System architecture features:\n"
                "- Round Robin & Priority Scheduling\n"
                "- Aging applied to prevent starvation\n"
                "- Real fork() and exec() multi-processing\n"
                "- Automatic Deadlock Detection", font, 18);
            guideText.setPosition(overlay.getPosition().x + 20, overlay.getPosition().y + 20);
            window.draw(guideText);
        }

        window.display();
    }

    // Clean up
    shutdown_requested = true;
    sched_cv.notify_all();
    
    for (auto& p : process_table) {
        kill(p.pid, SIGTERM);
        waitpid(p.pid, nullptr, 0);
    }

    sem_destroy(&ram_sem);
    sem_destroy(&core_sem);

    cout << "Thank you for using MaqOS — Maqsad Operating System. Goodbye!" << endl;
    system("./bin/shutdown");
    return 0;
}

// RAM management functions
bool allocate_ram(int amount) {
    sem_wait(&ram_sem);
    if (used_ram + amount > MAX_RAM) {
        sem_post(&ram_sem);
        return false; // Not enough RAM
    }
    used_ram += amount;
    sem_post(&ram_sem);
    return true;
}

void free_ram(int amount) {
    sem_wait(&ram_sem);
    used_ram -= amount;
    if (used_ram < 0) used_ram = 0; // Safeguard
    sem_post(&ram_sem);
}



string get_current_time() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    
    stringstream ss;
    ss << setw(2) << setfill('0') << ltm->tm_hour << ":"
       << setw(2) << setfill('0') << ltm->tm_min << ":"
       << setw(2) << setfill('0') << ltm->tm_sec;
    
    return ss.str();
}



