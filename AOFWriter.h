#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <string>
#include <cstdio>

enum class AOFSyncMode {
    ALWAYS,   // 每条命令后立即 fsync
    EVERYSEC, // 每秒 fsync 一次
    NO        // 不主动 fsync
};

class AOFWriter {
public:
    AOFWriter(const std::string& filename, AOFSyncMode mode = AOFSyncMode::EVERYSEC);
    ~AOFWriter();

    AOFWriter(const AOFWriter&) = delete;
    AOFWriter& operator=(const AOFWriter&) = delete;

    // 主线程调用：将命令加入队列（非阻塞）
    void appendCommand(const std::string& cmd);

    // 停止后台线程并等待结束
    void stop();

private:
    void backgroundThread();          
    void flushAndFsync();     
    
    void fsync();

    std::string aof_path_;
    AOFSyncMode sync_mode_;
    std::atomic<bool> stop_{false};
    std::thread worker_;

    std::queue<std::string> pending_cmds_;   
    std::mutex mtx_;
    std::condition_variable cv_;

    FILE* aof_file_ = nullptr;
    std::chrono::steady_clock::time_point last_fsync_;
};