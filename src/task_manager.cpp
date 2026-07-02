#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <mutex>
#include <chrono>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#elif defined(__linux__) || defined(__unix__)
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#endif

class Process {
public:
    unsigned long pid;
    std::string name;
    float cpuUsage;
    size_t memoryUsage; // in KB
    std::string status;
    
    Process(unsigned long p, const std::string& n, float cpu = 0.0f, size_t mem = 0, const std::string& s = "Running")
        : pid(p), name(n), cpuUsage(cpu), memoryUsage(mem), status(s) {}
};

class SystemInfo {
private:
    float cpuUsage;
    size_t totalMemory;
    size_t usedMemory;
    size_t totalSwap;
    size_t usedSwap;
    std::vector<Process> processes;
    std::mutex dataMutex;
    
    // Platform-specific implementation details
#ifdef _WIN32
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuTotal;
    
    void initCpuCounter() {
        PdhOpenQuery(NULL, NULL, &cpuQuery);
        PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
    }
    
    float getCurrentCpuUsage() {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
        return static_cast<float>(counterVal.doubleValue);
    }
    
    void updateMemoryInfo() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        
        totalMemory = memInfo.ullTotalPhys / 1024; // KB
        usedMemory = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / 1024; // KB
        totalSwap = memInfo.ullTotalPageFile / 1024; // KB
        usedSwap = (memInfo.ullTotalPageFile - memInfo.ullAvailPageFile) / 1024; // KB
    }
    
    void updateProcessList() {
        DWORD processes[1024];
        DWORD bytesReturned;
        if (!EnumProcesses(processes, sizeof(processes), &bytesReturned)) {
            return;
        }
        
        DWORD numProcesses = bytesReturned / sizeof(DWORD);
        std::vector<Process> newProcesses;
        
        for (DWORD i = 0; i < numProcesses; i++) {
            if (processes[i] == 0) continue;
            
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
            if (hProcess == NULL) continue;
            
            // Get process name
            TCHAR processName[MAX_PATH] = TEXT("<unknown>");
            HMODULE hMod;
            DWORD cbNeeded;
            
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                GetModuleBaseName(hProcess, hMod, processName, sizeof(processName)/sizeof(TCHAR));
            }
            
            // Get memory info
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                std::string name = processName;
                newProcesses.emplace_back(processes[i], name, 0.0f, pmc.WorkingSetSize / 1024);
            }
            
            CloseHandle(hProcess);
        }
        
        // Lock to update process list
        std::lock_guard<std::mutex> lock(dataMutex);
        processes = std::move(newProcesses);
    }
    
#elif defined(__linux__) || defined(__unix__)
    unsigned long long lastTotalUser = 0, lastTotalUserLow = 0, lastTotalSys = 0, lastTotalIdle = 0;
    
    float getCurrentCpuUsage() {
        std::ifstream fileStat("/proc/stat");
        std::string line;
        
        if (std::getline(fileStat, line)) {
            std::istringstream iss(line);
            std::string cpu;
            unsigned long long totalUser, totalUserLow, totalSys, totalIdle, totalIOwait, totalIrq, totalSoftIrq, totalSteal;
            
            iss >> cpu >> totalUser >> totalUserLow >> totalSys >> totalIdle >> totalIOwait >> totalIrq >> totalSoftIrq >> totalSteal;
            
            if (lastTotalUser == 0) {
                // First call, initialize values
                lastTotalUser = totalUser;
                lastTotalUserLow = totalUserLow;
                lastTotalSys = totalSys;
                lastTotalIdle = totalIdle;
                return 0.0f;
            }
            
            unsigned long long totalUserDiff = totalUser - lastTotalUser;
            unsigned long long totalUserLowDiff = totalUserLow - lastTotalUserLow;
            unsigned long long totalSysDiff = totalSys - lastTotalSys;
            unsigned long long totalIdleDiff = totalIdle - lastTotalIdle;
            
            lastTotalUser = totalUser;
            lastTotalUserLow = totalUserLow;
            lastTotalSys = totalSys;
            lastTotalIdle = totalIdle;
            
            unsigned long long totalDiff = totalUserDiff + totalUserLowDiff + totalSysDiff + totalIdleDiff;
            return totalDiff > 0 ? 100.0f * (totalDiff - totalIdleDiff) / totalDiff : 0.0f;
        }
        
        return 0.0f;
    }
    
    void updateMemoryInfo() {
        struct sysinfo memInfo;
        sysinfo(&memInfo);
        
        totalMemory = memInfo.totalram / 1024; // KB
        usedMemory = (memInfo.totalram - memInfo.freeram) / 1024; // KB
        totalSwap = memInfo.totalswap / 1024; // KB
        usedSwap = (memInfo.totalswap - memInfo.freeswap) / 1024; // KB
    }
    
    // Get process CPU usage from /proc/<pid>/stat
    float getProcessCpuUsage(unsigned long pid) {
        std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream stat_file(statPath);
        
        if (!stat_file.is_open()) return 0.0f;
        
        std::string line;
        std::getline(stat_file, line);
        std::istringstream iss(line);
        
        std::vector<std::string> stat_values;
        std::string value;
        while (iss >> value) {
            stat_values.push_back(value);
        }
        
        if (stat_values.size() < 17) return 0.0f;
        
        // Calculate CPU usage based on utime and stime
        unsigned long utime = std::stoul(stat_values[13]);
        unsigned long stime = std::stoul(stat_values[14]);
        
        // This is a simplified calculation - a real implementation would track deltas over time
        unsigned long total_time = utime + stime;
        
        // Normalize by the number of cores
        long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
        return (100.0f * total_time / (num_cores * sysconf(_SC_CLK_TCK)));
    }
    
    void updateProcessList() {
        std::vector<Process> newProcesses;
        DIR* dir = opendir("/proc");
        
        if (dir != nullptr) {
            struct dirent* entry;
            
            while ((entry = readdir(dir)) != nullptr) {
                // Check if entry name is a number (PID)
                if (entry->d_type == DT_DIR) {
                    std::string name = entry->d_name;
                    bool isNumeric = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
                    
                    if (isNumeric) {
                        unsigned long pid = std::stoul(name);
                        std::string cmdlinePath = "/proc/" + name + "/cmdline";
                        std::string statusPath = "/proc/" + name + "/status";
                        
                        // Get process name
                        std::ifstream cmdlineFile(cmdlinePath);
                        std::string processName;
                        std::getline(cmdlineFile, processName, '\0');
                        
                        if (processName.empty()) {
                            std::ifstream statusFile(statusPath);
                            std::string line;
                            while (std::getline(statusFile, line)) {
                                if (line.find("Name:") == 0) {
                                    processName = line.substr(6); // Skip "Name: "
                                    // Trim leading whitespace
                                    processName.erase(0, processName.find_first_not_of(" \t"));
                                    break;
                                }
                            }
                        }
                        
                        // Get memory usage from status
                        std::ifstream statusFile(statusPath);
                        std::string line;
                        size_t memoryUsageKB = 0;
                        
                        while (std::getline(statusFile, line)) {
                            if (line.find("VmRSS:") == 0) {
                                std::istringstream iss(line.substr(6)); // Skip "VmRSS:"
                                iss >> memoryUsageKB;
                                break;
                            }
                        }
                        
                        // Get CPU usage
                        float cpuUsage = getProcessCpuUsage(pid);
                        
                        // Only add if we got a name
                        if (!processName.empty()) {
                            newProcesses.emplace_back(pid, processName, cpuUsage, memoryUsageKB);
                        }
                    }
                }
            }
            
            closedir(dir);
        }
        
        // Lock to update process list
        std::lock_guard<std::mutex> lock(dataMutex);
        processes = std::move(newProcesses);
    }
#endif

public:
    SystemInfo() : cpuUsage(0.0f), totalMemory(0), usedMemory(0), totalSwap(0), usedSwap(0) {
#ifdef _WIN32
        initCpuCounter();
#endif
    }
    
    ~SystemInfo() {
#ifdef _WIN32
        PdhCloseQuery(cpuQuery);
#endif
    }
    
    void update() {
        cpuUsage = getCurrentCpuUsage();
        updateMemoryInfo();
        updateProcessList();
    }
    
    float getCpuUsage() const {
        return cpuUsage;
    }
    
    size_t getTotalMemory() const {
        return totalMemory;
    }
    
    size_t getUsedMemory() const {
        return usedMemory;
    }
    
    size_t getTotalSwap() const {
        return totalSwap;
    }
    
    size_t getUsedSwap() const {
        return usedSwap;
    }
    
    std::vector<Process> getProcesses() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return processes;
    }
    
    void sortProcessesBy(const std::string& criteria) {
        std::lock_guard<std::mutex> lock(dataMutex);
        
        if (criteria == "name") {
            std::sort(processes.begin(), processes.end(), 
                [](const Process& a, const Process& b) { return a.name < b.name; });
        } else if (criteria == "pid") {
            std::sort(processes.begin(), processes.end(), 
                [](const Process& a, const Process& b) { return a.pid < b.pid; });
        } else if (criteria == "cpu") {
            std::sort(processes.begin(), processes.end(), 
                [](const Process& a, const Process& b) { return a.cpuUsage > b.cpuUsage; });
        } else if (criteria == "memory") {
            std::sort(processes.begin(), processes.end(), 
                [](const Process& a, const Process& b) { return a.memoryUsage > b.memoryUsage; });
        }
    }
};

class TaskManager {
private:
    sf::RenderWindow window;
    sf::Font font;
    SystemInfo sysInfo;
    
    // UI state variables
    bool running;
    int selectedTab;
    int selectedProcess;
    std::string sortCriteria;
    std::vector<std::string> tabs;
    
    // UI constants
    const float HEADER_HEIGHT = 40.0f;
    const float TAB_HEIGHT = 30.0f;
    const float MARGIN = 10.0f;
    const int PROCESS_ROW_HEIGHT = 24;
    
    // UI elements
    sf::RectangleShape headerBg;
    sf::RectangleShape tabsBg;
    sf::RectangleShape contentBg;
    
    // Graph-related members
    const int GRAPH_HISTORY_SIZE = 60;
    std::vector<float> cpuHistory;
    std::vector<float> memHistory;
    
    // Data collection thread
    std::thread updateThread;
    bool threadRunning;
    
    int killMessageTimer = 0;
    std::string killMessage = "";
    
    void initUI() {
        // Load font
        if (!font.loadFromFile("assets/fonts/SegoeUIVF.ttf")) {
            // Try system fonts if arial.ttf isn't found
#ifdef _WIN32
            font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
#elif defined(__linux__) || defined(__unix__)
            font.loadFromFile("assets/fonts/SegoeUIVF.ttf");
#endif
        }
        
        // Setup header
        headerBg.setSize(sf::Vector2f(window.getSize().x, HEADER_HEIGHT));
        headerBg.setFillColor(sf::Color(50, 50, 50));
        
        // Setup tabs
        tabsBg.setSize(sf::Vector2f(window.getSize().x, TAB_HEIGHT));
        tabsBg.setPosition(0, HEADER_HEIGHT);
        tabsBg.setFillColor(sf::Color(70, 70, 70));
        
        // Setup content area
        contentBg.setSize(sf::Vector2f(window.getSize().x, window.getSize().y - HEADER_HEIGHT - TAB_HEIGHT));
        contentBg.setPosition(0, HEADER_HEIGHT + TAB_HEIGHT);
        contentBg.setFillColor(sf::Color(30, 30, 30));
        
        // Initialize graph history
        cpuHistory.resize(GRAPH_HISTORY_SIZE, 0.0f);
        memHistory.resize(GRAPH_HISTORY_SIZE, 0.0f);
    }
    
    void startUpdateThread() {
        threadRunning = true;
        updateThread = std::thread([this]() {
            while (threadRunning) {
                sysInfo.update();
                
                // Add current values to history
                cpuHistory.push_back(sysInfo.getCpuUsage());
                if (cpuHistory.size() > GRAPH_HISTORY_SIZE) {
                    cpuHistory.erase(cpuHistory.begin());
                }
                
                float memPercentage = 100.0f * sysInfo.getUsedMemory() / std::max<size_t>(1, sysInfo.getTotalMemory());
                memHistory.push_back(memPercentage);
                if (memHistory.size() > GRAPH_HISTORY_SIZE) {
                    memHistory.erase(memHistory.begin());
                }
                
                // Update less frequently to reduce CPU load
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        });
    }
    
    void drawText(const std::string& text, float x, float y, unsigned int size = 14, sf::Color color = sf::Color::White) {
        sf::Text sfText;
        sfText.setFont(font);
        sfText.setString(text);
        sfText.setCharacterSize(size);
        sfText.setFillColor(color);
        sfText.setPosition(x, y);
        window.draw(sfText);
    }
    
    void drawProgressBar(float x, float y, float width, float height, float percentage, sf::Color barColor) {
        // Background
        sf::RectangleShape background(sf::Vector2f(width, height));
        background.setPosition(x, y);
        background.setFillColor(sf::Color(60, 60, 60));
        window.draw(background);
        
        // Bar
        sf::RectangleShape bar(sf::Vector2f(width * percentage / 100.0f, height));
        bar.setPosition(x, y);
        bar.setFillColor(barColor);
        window.draw(bar);
        
        // Text
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << percentage << "%";
        sf::Text text;
        text.setFont(font);
        text.setString(ss.str());
        text.setCharacterSize(12);
        text.setFillColor(sf::Color::White);
        text.setPosition(x + width / 2 - text.getLocalBounds().width / 2, y + height / 2 - text.getLocalBounds().height / 2 - 2);
        window.draw(text);
    }
    
    void drawGraph(const std::vector<float>& history, float x, float y, float width, float height, sf::Color color, const std::string& title) {
        // Draw background
        sf::RectangleShape background(sf::Vector2f(width, height));
        background.setPosition(x, y);
        background.setFillColor(sf::Color(40, 40, 40));
        background.setOutlineColor(sf::Color(80, 80, 80));
        background.setOutlineThickness(1);
        window.draw(background);
        
        // Draw title
        drawText(title, x + 5, y + 5, 14, sf::Color(200, 200, 200));
        
        // Draw graph grid lines
        for (int i = 0; i < 5; i++) {
            float lineY = y + 20 + (height - 25) * i / 4;
            
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(x + 5, lineY), sf::Color(80, 80, 80)),
                sf::Vertex(sf::Vector2f(x + width - 5, lineY), sf::Color(80, 80, 80))
            };
            window.draw(line, 2, sf::Lines);
            
            // Draw percentage labels
            std::stringstream ss;
            ss << 100 - i * 25 << "%";
            drawText(ss.str(), x + 5, lineY - 15, 10, sf::Color(150, 150, 150));
        }
        
        // Draw graph
        if (history.size() > 1) {
            sf::VertexArray graph(sf::LineStrip, history.size());
            
            for (size_t i = 0; i < history.size(); i++) {
                float value = std::min(100.0f, std::max(0.0f, history[i]));
                float graphX = x + 5 + (width - 10) * i / (history.size() - 1);
                float graphY = y + height - 5 - (height - 25) * value / 100.0f;
                graph[i] = sf::Vertex(sf::Vector2f(graphX, graphY), color);
            }
            
            window.draw(graph);
        }
    }
    
    void drawHeader() {
        window.draw(headerBg);
        
        drawText("MaqOS Task Manager", MARGIN, MARGIN, 18);
        
        // System time
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        
        char timeBuffer[20];
#ifdef _WIN32
        struct tm timeInfo;
        localtime_s(&timeInfo, &currentTime);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);
#else
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", localtime(&currentTime));
#endif
        
        drawText(timeBuffer, window.getSize().x - 70, MARGIN, 14);
    }
    
    void drawTabs() {
        window.draw(tabsBg);
        
        float tabWidth = window.getSize().x / tabs.size();
        
        for (size_t i = 0; i < tabs.size(); i++) {
            sf::Color tabColor = (selectedTab == static_cast<int>(i)) 
                ? sf::Color(90, 90, 90) 
                : sf::Color(70, 70, 70);
            
            sf::RectangleShape tabBg(sf::Vector2f(tabWidth - 2, TAB_HEIGHT - 2));
            tabBg.setPosition(i * tabWidth + 1, HEADER_HEIGHT + 1);
            tabBg.setFillColor(tabColor);
            window.draw(tabBg);
            
            sf::Text text;
            text.setFont(font);
            text.setString(tabs[i]);
            text.setCharacterSize(14);
            text.setFillColor(sf::Color::White);
            
            // Center text in tab
            float textX = i * tabWidth + (tabWidth - text.getLocalBounds().width) / 2;
            float textY = HEADER_HEIGHT + (TAB_HEIGHT - text.getLocalBounds().height) / 2 - 5;
            text.setPosition(textX, textY);
            window.draw(text);
        }
    }
    
    void drawPerformanceTab() {
        float contentY = HEADER_HEIGHT + TAB_HEIGHT + MARGIN;
        float contentWidth = window.getSize().x - 2 * MARGIN;
        float contentHeight = window.getSize().y - contentY - MARGIN;
        
        // CPU usage section
        drawText("CPU Usage:", MARGIN, contentY, 16);
        float cpuUsage = sysInfo.getCpuUsage();
        drawProgressBar(MARGIN, contentY + 25, contentWidth, 20, cpuUsage, 
                       sf::Color(0, 122, 204, 200));
        
        // Memory usage section
        drawText("Memory Usage:", MARGIN, contentY + 60, 16);
        float memPercentage = 100.0f * sysInfo.getUsedMemory() / std::max<size_t>(1, sysInfo.getTotalMemory());
        drawProgressBar(MARGIN, contentY + 85, contentWidth, 20, memPercentage, 
                       sf::Color(0, 158, 115, 200));
        
        std::stringstream memText;
        memText << std::fixed << std::setprecision(1) 
                << sysInfo.getUsedMemory() / 1024.0f << " MB / " 
                << sysInfo.getTotalMemory() / 1024.0f << " MB";
        drawText(memText.str(), MARGIN, contentY + 110, 14, sf::Color(200, 200, 200));
        
        // Swap usage section
        drawText("Swap Usage:", MARGIN, contentY + 140, 16);
        float swapPercentage = 100.0f * sysInfo.getUsedSwap() / std::max<size_t>(1, sysInfo.getTotalSwap());
        drawProgressBar(MARGIN, contentY + 165, contentWidth, 20, swapPercentage, 
                       sf::Color(230, 159, 0, 200));
        
        std::stringstream swapText;
        swapText << std::fixed << std::setprecision(1) 
                 << sysInfo.getUsedSwap() / 1024.0f << " MB / " 
                 << sysInfo.getTotalSwap() / 1024.0f << " MB";
        drawText(swapText.str(), MARGIN, contentY + 190, 14, sf::Color(200, 200, 200));
        
        // Graphs
        float graphWidth = (contentWidth - MARGIN) / 2;
        float graphHeight = 180;
        float graphY = contentY + 230;
        
        drawGraph(cpuHistory, MARGIN, graphY, graphWidth, graphHeight, 
                 sf::Color(0, 122, 204), "CPU History");
        
        drawGraph(memHistory, MARGIN + graphWidth + MARGIN, graphY, graphWidth, graphHeight, 
                 sf::Color(0, 158, 115), "Memory History");
    }
    
    void drawProcessesTab() {
        float contentY = HEADER_HEIGHT + TAB_HEIGHT + MARGIN;
        float contentWidth = window.getSize().x - 2 * MARGIN;
        
        // Process list header
        sf::RectangleShape headerBg(sf::Vector2f(contentWidth, 30));
        headerBg.setPosition(MARGIN, contentY);
        headerBg.setFillColor(sf::Color(60, 60, 60));
        window.draw(headerBg);
        
        // Column headers and widths
        struct Column {
            std::string name;
            float width;
            std::string sortKey;
        };
        
        std::vector<Column> columns = {
            {"Name", contentWidth * 0.4f, "name"},
            {"PID", contentWidth * 0.1f, "pid"},
            {"CPU %", contentWidth * 0.15f, "cpu"},
            {"Memory", contentWidth * 0.25f, "memory"},
            {"Status", contentWidth * 0.1f, ""}
        };
        
        float xPos = MARGIN;
        for (const auto& col : columns) {
            sf::Color headerColor = (sortCriteria == col.sortKey) ? sf::Color::Yellow : sf::Color::White;
            drawText(col.name, xPos + 5, contentY + 5, 14, headerColor);
            
            // Draw vertical separator
            if (xPos > MARGIN) {
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(xPos, contentY), sf::Color(100, 100, 100)),
                    sf::Vertex(sf::Vector2f(xPos, contentY + 30), sf::Color(100, 100, 100))
                };
                window.draw(line, 2, sf::Lines);
            }
            
            xPos += col.width;
        }
        
        // Process list
        auto processes = sysInfo.getProcesses();
        
        // Calculate visible range based on window size
        int visibleRows = static_cast<int>((window.getSize().y - contentY - 30 - MARGIN) / PROCESS_ROW_HEIGHT);
        int startIdx = std::max(0, std::min(selectedProcess - visibleRows / 2, static_cast<int>(processes.size()) - visibleRows));
        if (startIdx < 0) startIdx = 0;
        
        for (int i = 0; i < visibleRows && i + startIdx < static_cast<int>(processes.size()); i++) {
            const auto& process = processes[i + startIdx];
            float rowY = contentY + 30 + i * PROCESS_ROW_HEIGHT;
            
            // Highlight selected row
            if (i + startIdx == selectedProcess) {
                sf::RectangleShape highlight(sf::Vector2f(contentWidth, PROCESS_ROW_HEIGHT));
                highlight.setPosition(MARGIN, rowY);
                highlight.setFillColor(sf::Color(70, 130, 180, 128));
                window.draw(highlight);
            }
            
            // Alternate row background
            if (i % 2 == 0) {
                sf::RectangleShape rowBg(sf::Vector2f(contentWidth, PROCESS_ROW_HEIGHT));
                rowBg.setPosition(MARGIN, rowY);
                rowBg.setFillColor(sf::Color(40, 40, 40, 128));
                window.draw(rowBg);
            }
            
            // Draw process info
            xPos = MARGIN;
            
            // Name column
            drawText(process.name, xPos + 5, rowY + 4, 12);
            xPos += columns[0].width;
            
            // PID column
            drawText(std::to_string(process.pid), xPos + 5, rowY + 4, 12);
            xPos += columns[1].width;
            
            // CPU usage column
            std::stringstream cpuStr;
            cpuStr << std::fixed << std::setprecision(1) << process.cpuUsage;
            drawText(cpuStr.str(), xPos + 5, rowY + 4, 12);
            xPos += columns[2].width;
            
            // Memory usage column
            std::stringstream memStr;
            if (process.memoryUsage < 1024) {
                memStr << process.memoryUsage << " KB";
            } else if (process.memoryUsage < 1024 * 1024) {
                memStr << std::fixed << std::setprecision(1) << (process.memoryUsage / 1024.0f) << " MB";
            } else {
                memStr << std::fixed << std::setprecision(2) << (process.memoryUsage / (1024.0f * 1024.0f)) << " GB";
            }
            drawText(memStr.str(), xPos + 5, rowY + 4, 12);
            xPos += columns[3].width;
            
            // Status column
            drawText(process.status, xPos + 5, rowY + 4, 12);
        }
        
        // Draw scrollbar if needed
        if (processes.size() > visibleRows) {
            float scrollbarHeight = (static_cast<float>(visibleRows) / processes.size()) * (visibleRows * PROCESS_ROW_HEIGHT);
            float scrollbarY = contentY + 30 + (static_cast<float>(startIdx) / processes.size()) * (visibleRows * PROCESS_ROW_HEIGHT);
            
            sf::RectangleShape scrollbar(sf::Vector2f(8, scrollbarHeight));
            scrollbar.setPosition(window.getSize().x - MARGIN - 8, scrollbarY);
            scrollbar.setFillColor(sf::Color(100, 100, 100));
            window.draw(scrollbar);
        }
        
        // Draw Kill Process button
        float buttonWidth = 140, buttonHeight = 32;
        float buttonX = MARGIN;
        float buttonY = window.getSize().y - buttonHeight - MARGIN;
        sf::RectangleShape killBtn(sf::Vector2f(buttonWidth, buttonHeight));
        killBtn.setPosition(buttonX, buttonY);
        killBtn.setFillColor(selectedProcess >= 0 && selectedProcess < (int)processes.size() ? sf::Color(180,70,70) : sf::Color(90,90,90));
        window.draw(killBtn);
        drawText("Kill Process", buttonX + 20, buttonY + 6, 16, sf::Color::White);
        // Draw kill message if any
        if (!killMessage.empty() && killMessageTimer > 0) {
            drawText(killMessage, buttonX + buttonWidth + 20, buttonY + 8, 16, sf::Color(255,80,80));
        }
    }
    
    void drawSystemTab() {
        float contentY = HEADER_HEIGHT + TAB_HEIGHT + MARGIN;
        
        // System information - this could be expanded with more detailed info
#ifdef _WIN32
        drawText("Operating System: Windows", MARGIN, contentY, 16);
#elif defined(__linux__)
        drawText("Operating System: Linux", MARGIN, contentY, 16);
#elif defined(__unix__)
        drawText("Operating System: Unix", MARGIN, contentY, 16);
#else
        drawText("Operating System: Unknown", MARGIN, contentY, 16);
#endif
        
        contentY += 30;
        
        // CPU info
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        drawText("Processor Cores: " + std::to_string(sysInfo.dwNumberOfProcessors), MARGIN, contentY, 14);
#elif defined(__linux__) || defined(__unix__)
        long numCores = sysconf(_SC_NPROCESSORS_ONLN);
        drawText("Processor Cores: " + std::to_string(numCores), MARGIN, contentY, 14);
#endif
        
        contentY += 25;
        
        // Memory info
        std::stringstream memStr;
        memStr << "Total Physical Memory: " << std::fixed << std::setprecision(1) 
               << (this->sysInfo.getTotalMemory() / (1024.0f * 1024.0f)) << " GB";
        drawText(memStr.str(), MARGIN, contentY, 14);
        
        contentY += 25;
        
        // System uptime
#ifdef _WIN32
        ULONGLONG uptimeMS = GetTickCount64();
        int upSec = static_cast<int>(uptimeMS / 1000);
#elif defined(__linux__) || defined(__unix__)
        struct sysinfo sinfo;
        sysinfo(&sinfo);
        int upSec = static_cast<int>(sinfo.uptime);
#endif
        
        int upDays = upSec / (60 * 60 * 24);
        int upHours = (upSec % (60 * 60 * 24)) / (60 * 60);
        int upMinutes = (upSec % (60 * 60)) / 60;
        
        std::stringstream uptimeStr;
        uptimeStr << "System Uptime: ";
        if (upDays > 0) uptimeStr << upDays << " days, ";
        uptimeStr << upHours << " hours, " << upMinutes << " minutes";
        
        drawText(uptimeStr.str(), MARGIN, contentY, 14);
    }
    
    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                running = false;
            } else if (event.type == sf::Event::Resized) {
                // Update view to match new window size
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
                
                // Resize UI elements
                headerBg.setSize(sf::Vector2f(window.getSize().x, HEADER_HEIGHT));
                tabsBg.setSize(sf::Vector2f(window.getSize().x, TAB_HEIGHT));
                contentBg.setSize(sf::Vector2f(window.getSize().x, window.getSize().y - HEADER_HEIGHT - TAB_HEIGHT));
            } else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    // Check for tab clicks
                    if (event.mouseButton.y >= HEADER_HEIGHT && event.mouseButton.y <= HEADER_HEIGHT + TAB_HEIGHT) {
                        float tabWidth = window.getSize().x / tabs.size();
                        int clickedTab = static_cast<int>(event.mouseButton.x / tabWidth);
                        if (clickedTab >= 0 && clickedTab < static_cast<int>(tabs.size())) {
                            selectedTab = clickedTab;
                        }
                    }
                    
                    // Check for process selection in process tab
                    if (selectedTab == 1 && event.mouseButton.y > HEADER_HEIGHT + TAB_HEIGHT + MARGIN + 30) {
                        float contentY = HEADER_HEIGHT + TAB_HEIGHT + MARGIN + 30;
                        int rowIdx = static_cast<int>((event.mouseButton.y - contentY) / PROCESS_ROW_HEIGHT);
                        
                        auto processes = sysInfo.getProcesses();
                        int visibleRows = static_cast<int>((window.getSize().y - contentY - MARGIN) / PROCESS_ROW_HEIGHT);
                        int startIdx = std::max(0, std::min(selectedProcess - visibleRows / 2, static_cast<int>(processes.size()) - visibleRows));
                        
                        if (rowIdx >= 0 && rowIdx + startIdx < static_cast<int>(processes.size())) {
                            selectedProcess = rowIdx + startIdx;
                        }
                    }
                    
                    // Check for column header clicks to sort
                    if (selectedTab == 1 && 
                        event.mouseButton.y > HEADER_HEIGHT + TAB_HEIGHT + MARGIN && 
                        event.mouseButton.y < HEADER_HEIGHT + TAB_HEIGHT + MARGIN + 30) {
                        
                        float contentWidth = window.getSize().x - 2 * MARGIN;
                        float xPos = event.mouseButton.x - MARGIN;
                        
                        std::vector<std::pair<float, std::string>> columnWidths = {
                            {contentWidth * 0.4f, "name"},
                            {contentWidth * 0.1f, "pid"},
                            {contentWidth * 0.15f, "cpu"},
                            {contentWidth * 0.25f, "memory"}
                        };
                        
                        float accumulatedWidth = 0;
                        for (const auto& col : columnWidths) {
                            accumulatedWidth += col.first;
                            if (xPos < accumulatedWidth) {
                                sortCriteria = col.second;
                                sysInfo.sortProcessesBy(sortCriteria);
                                break;
                            }
                        }
                    }
                    
                    // Check for Kill Process button click
                    auto processes = sysInfo.getProcesses();
                    float buttonWidth = 140, buttonHeight = 32;
                    float buttonX = MARGIN;
                    float buttonY = window.getSize().y - buttonHeight - MARGIN;
                    if (event.mouseButton.x >= buttonX && event.mouseButton.x <= buttonX + buttonWidth &&
                        event.mouseButton.y >= buttonY && event.mouseButton.y <= buttonY + buttonHeight &&
                        selectedProcess >= 0 && selectedProcess < (int)processes.size()) {
                        unsigned long pid = processes[selectedProcess].pid;
                        if (kill(pid, SIGKILL) == 0) {
                            killMessage = "Process killed.";
                        } else {
                            killMessage = "Failed to kill.";
                        }
                        killMessageTimer = 120; // Show message for ~2 seconds
                    }
                }
            } else if (event.type == sf::Event::MouseWheelScrolled) {
                if (selectedTab == 1) {  // Process tab
                    auto processes = sysInfo.getProcesses();
                    if (!processes.empty()) {
                        // Scroll up or down the process list
                        selectedProcess -= static_cast<int>(event.mouseWheelScroll.delta);
                        
                        // Clamp to valid range
                        selectedProcess = std::max(0, std::min(selectedProcess, static_cast<int>(processes.size()) - 1));
                    }
                }
            } else if (event.type == sf::Event::KeyPressed) {
                if (selectedTab == 1) {  // Process tab
                    auto processes = sysInfo.getProcesses();
                    if (!processes.empty()) {
                        if (event.key.code == sf::Keyboard::Up) {
                            selectedProcess = std::max(0, selectedProcess - 1);
                        } else if (event.key.code == sf::Keyboard::Down) {
                            selectedProcess = std::min(static_cast<int>(processes.size()) - 1, selectedProcess + 1);
                        } else if (event.key.code == sf::Keyboard::PageUp) {
                            int visibleRows = static_cast<int>((window.getSize().y - HEADER_HEIGHT - TAB_HEIGHT - MARGIN - 30 - MARGIN) / PROCESS_ROW_HEIGHT);
                            selectedProcess = std::max(0, selectedProcess - visibleRows);
                        } else if (event.key.code == sf::Keyboard::PageDown) {
                            int visibleRows = static_cast<int>((window.getSize().y - HEADER_HEIGHT - TAB_HEIGHT - MARGIN - 30 - MARGIN) / PROCESS_ROW_HEIGHT);
                            selectedProcess = std::min(static_cast<int>(processes.size()) - 1, selectedProcess + visibleRows);
                        } else if (event.key.code == sf::Keyboard::Home) {
                            selectedProcess = 0;
                        } else if (event.key.code == sf::Keyboard::End) {
                            selectedProcess = static_cast<int>(processes.size()) - 1;
                        }
                    }
                }
            }
        }
        
        if (killMessageTimer > 0) --killMessageTimer;
        if (killMessageTimer == 0) killMessage = "";
    }
    
public:
    TaskManager() 
        : window(sf::VideoMode(800, 600), "MaqOS Task Manager"), 
          running(true), 
          selectedTab(0), 
          selectedProcess(0),
          sortCriteria("cpu"),
          threadRunning(false) {
        
        // Set application icon if available
        // (This would require an icon file)
        
        // Initialize tabs
        tabs = {"Performance", "Processes", "System"};
        
        // Initialize UI
        initUI();
        
        // Start data collection thread
        startUpdateThread();
    }
    
    ~TaskManager() {
        // Clean up thread
        threadRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
    
    void run() {
        while (running && window.isOpen()) {
            // Process events
            handleEvents();
            
            // Clear window
            window.clear(sf::Color(20, 20, 20));
            
            // Draw UI
            window.draw(contentBg);
            drawHeader();
            drawTabs();
            
            // Draw selected tab content
            switch (selectedTab) {
                case 0:
                    drawPerformanceTab();
                    break;
                case 1:
                    drawProcessesTab();
                    break;
                case 2:
                    drawSystemTab();
                    break;
            }
            
            // Display the window
            window.display();
            
            // Avoid using 100% CPU
            sf::sleep(sf::milliseconds(16));  // ~60 FPS
        }
    }
};

int main() {
    try {
        TaskManager taskManager;
        taskManager.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}