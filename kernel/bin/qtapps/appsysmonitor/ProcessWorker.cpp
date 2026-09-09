#include "ProcessWorker.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>

namespace fs = std::filesystem;
const long ticksPerSec = sysconf(_SC_CLK_TCK);

std::vector<CpuTicks> ProcessWorker::getGlobalCpuTicks()
{
    std::ifstream file("/proc/stat");
    std::string line;
    std::vector<CpuTicks> result;

    while(std::getline(file, line)) 
    {
        if(line.rfind("cpu", 0) != 0) break; // Stop when 'cpu' lines end

        std::stringstream ss(line);
        CpuTicks t;

#ifdef __laylaos__
        // some fields are missing in LaylaOS
        if(ss >> t.id >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq)
#else
        if(ss >> t.id >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal)
#endif
        {
            result.push_back(t);
        }
    }

    return result;
}

std::string ProcessWorker::getProcessUser(unsigned int uid)
{
    passwd* pwd = getpwuid(uid);
    return (pwd) ? pwd->pw_name : std::to_string(uid);
}

void ProcessWorker::parseProcessIo(int pid, unsigned long long& readBytes, unsigned long long& writeBytes)
{
    std::ifstream file("/proc/" + std::to_string(pid) + "/io");
    std::string line;

    while(std::getline(file, line)) 
    {
        if(line.rfind("read_bytes:", 0) == 0) 
        {
            readBytes = std::stoull(line.substr(11));
        }
        else if(line.rfind("write_bytes:", 0) == 0) 
        {
            writeBytes = std::stoull(line.substr(12));
        }
    }
}

void ProcessWorker::getGlobalMemoryPercent(double& outMemPercent)
{
    std::ifstream file("/proc/meminfo");
    std::string token;
    unsigned long long memTotal = 0, memAvailable = 0;

    while(file >> token) 
    {
        if(token == "MemTotal:") file >> memTotal;
        else if(token == "MemAvailable:") file >> memAvailable;
    }

    if(memTotal > 0) 
    {
        outMemPercent = (static_cast<double>(memTotal - memAvailable) / memTotal) * 100.0;
    }
    else
    {
        outMemPercent = 0.0;
    }
}

std::vector<ProcessData> ProcessWorker::gatherFrameData(
    std::vector<CpuTicks>& prevGlobal, 
    std::unordered_map<int, unsigned long long>& prevProcTicks, 
    double elapsedSec, 
    std::vector<double>& outCpuPercents,
    double& outGlobalMemPercent) 
{
    // Global CPU
    std::vector<CpuTicks> currGlobal = getGlobalCpuTicks();
    outCpuPercents.clear();

    // Loop through each discovered core (including aggregate at index 0)
    for(size_t i = 0; i < currGlobal.size(); ++i) 
    {
        if(i >= prevGlobal.size()) { outCpuPercents.push_back(0.0); continue; }

        unsigned long long prevIdle = prevGlobal[i].idle + prevGlobal[i].iowait;
        unsigned long long currIdle = currGlobal[i].idle + currGlobal[i].iowait;
        unsigned long long prevTotal = prevIdle + prevGlobal[i].user + prevGlobal[i].nice + prevGlobal[i].system + prevGlobal[i].irq + prevGlobal[i].softirq + prevGlobal[i].steal;
        unsigned long long currTotal = currIdle + currGlobal[i].user + currGlobal[i].nice + currGlobal[i].system + currGlobal[i].irq + currGlobal[i].softirq + currGlobal[i].steal;
        
        double pct = 0.0;
        if(currTotal > prevTotal) 
        {
            pct = ((double)((currTotal - prevTotal) - (currIdle - prevIdle)) / (currTotal - prevTotal)) * 100.0;
        }
        outCpuPercents.push_back(pct);
    }
    prevGlobal = currGlobal;

    // Global Memory
    getGlobalMemoryPercent(outGlobalMemPercent);

    std::vector<ProcessData> frameProcesses;

    for(const auto& entry : fs::directory_iterator("/proc")) 
    {
        if(!entry.is_directory()) continue;
        std::string pidStr = entry.path().filename().string();
        if(!std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) continue;

        int pid = std::stoi(pidStr);
        ProcessData proc;
        proc.pid = pid;

        struct stat st;
        if(stat(entry.path().c_str(), &st) == 0) 
        {
            proc.user = getProcessUser(st.st_uid);
        }

        std::ifstream statFile(entry.path() / "stat");
        if(!statFile.is_open()) continue;
        
        std::string s;
        std::vector<std::string> parts;
        while(statFile >> s) { parts.push_back(s); }
        if(parts.size() < 22) continue;

        size_t closeParenIdx = 1;
        for(size_t i = 1; i < parts.size(); ++i) 
        {
            if(parts[i].back() == ')') { closeParenIdx = i; break; }
        }

        if(!parts.empty()) 
        {
            proc.name = parts[1]; // First item containing the open parenthesis like "(bash"
        }

        for(size_t i = 2; i <= closeParenIdx; ++i) 
        {
            proc.name += " " + parts[i]; // Safely stitch back paths with spaces
        }

        if(!proc.name.empty() && proc.name.front() == '(') proc.name.erase(0, 1);
        if(!proc.name.empty() && proc.name.back() == ')') proc.name.pop_back();

        size_t offset = closeParenIdx;

#ifdef __laylaos__
        // there is an extra tgid field in LaylaOS
        proc.nice = std::stoi(parts[18 + offset]);
        unsigned long long utime = std::stoull(parts[13 + offset]);
        unsigned long long stime = std::stoull(parts[14 + offset]);
#else
        proc.nice = std::stoi(parts[17 + offset]);
        unsigned long long utime = std::stoull(parts[12 + offset]);
        unsigned long long stime = std::stoull(parts[13 + offset]);
#endif

        proc.totalTicksSinceBoot = utime + stime;

        if(prevProcTicks.find(pid) != prevProcTicks.end() && elapsedSec > 0.0) 
        {
            unsigned long long tickDelta = proc.totalTicksSinceBoot - prevProcTicks[pid];

            // Convert jiffies delta to raw CPU percentage utilization
            proc.cpuPercent = ((static_cast<double>(tickDelta) / ticksPerSec) / elapsedSec) * 100.0;
        }
        else
        {
            proc.cpuPercent = 0.0;
        }

        prevProcTicks[pid] = proc.totalTicksSinceBoot;

        std::ifstream statusFile(entry.path() / "status");
        std::string line;

        while(std::getline(statusFile, line)) 
        {
            if(line.rfind("VmRSS:", 0) == 0) 
            {
                std::stringstream ss(line.substr(6));
                unsigned long long kb = 0;
                ss >> kb;
                proc.rssMemoryBytes = kb * 1024;
                break;
            }
        }

        parseProcessIo(pid, proc.bytesRead, proc.bytesWritten);
        frameProcesses.push_back(proc);
    }

    std::unordered_map<int, unsigned long long> nextProcTicks;
    for(const auto& proc : frameProcesses) 
    {
        nextProcTicks[proc.pid] = prevProcTicks[proc.pid];
    }
    prevProcTicks = std::move(nextProcTicks);

    return frameProcesses;
}

