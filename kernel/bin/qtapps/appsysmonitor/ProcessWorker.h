#ifndef PROCESS_WORKER_H
#define PROCESS_WORKER_H

#include <string>
#include <vector>
#include <unordered_map>

struct CpuTicks
{
    std::string id; // e.g., "cpu", "cpu0", "cpu1"
    unsigned long long user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
};

struct ProcessData
{
    int pid = 0;
    std::string name;
    std::string user;
    int nice = 0;
    double cpuPercent = 0.0;
    unsigned long long rssMemoryBytes = 0;
    unsigned long long bytesRead = 0;
    unsigned long long bytesWritten = 0;
    unsigned long long totalTicksSinceBoot = 0;
};

class ProcessWorker
{
public:
    static std::vector<CpuTicks> getGlobalCpuTicks();
    static std::string getProcessUser(unsigned int uid);
    static void parseProcessIo(int pid, unsigned long long& readBytes, unsigned long long& writeBytes);
    static void getGlobalMemoryPercent(double& outMemPercent);

    static std::vector<ProcessData> gatherFrameData(
    std::vector<CpuTicks>& prevGlobal,
    std::unordered_map<int, unsigned long long>& prevProcTicks, 
    double elapsedSec, 
    std::vector<double>& outCpuPercents,
    double& outGlobalMemPercent
);
};

#endif      /* PROCESS_WORKER_H */
