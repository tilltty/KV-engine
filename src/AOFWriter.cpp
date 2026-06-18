#include "AOFWriter.h"
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <stdexcept>

AOFWriter::AOFWriter(const std::string& filename, AOFSyncMode mode)
    : aof_path_(filename), sync_mode_(mode){
    aof_file_ = fopen(aof_path_.c_str(), "a");
    if(!aof_file_){
        throw std::runtime_error("Failed to open AOF file: " + aof_path_);
    }
    setbuf(aof_file_, nullptr);
    last_fsync_ = std::chrono::steady_clock::now();
    worker_ = std::thread(&AOFWriter::backgroundThread, this);
}

AOFWriter::~AOFWriter(){
    stop();
}

void AOFWriter::stop(){
    if(stop_) return;
    stop_ = true;
    cv_.notify_one();
    if(worker_.joinable()) worker_.join();
    if(aof_file_){
        fclose(aof_file_);
        aof_file_ = nullptr;
    }
}
void AOFWriter::appendCommand(const std::string& cmd){
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_cmds_.push(cmd);
    }
    cv_.notify_one();
}

void AOFWriter::backgroundThread(){
    while(!stop_){
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::seconds(1), [this]{
            return stop_ || !pending_cmds_.empty();
        });

        std::queue<std::string> cmds;
        cmds.swap(pending_cmds_);
        lock.unlock();

        if(!cmds.empty()){
            while(!cmds.empty()){
                const std::string& cmd = cmds.front();
                size_t written = fwrite(cmd.c_str(), 1, cmd.size(), aof_file_);
                fwrite("\n", 1, 1, aof_file_);
                cmds.pop();
            }
            fflush(aof_file_);

            if(sync_mode_ == AOFSyncMode::ALWAYS){
                fsync();
                last_fsync_ = std::chrono::steady_clock::now();
            }
            else if(sync_mode_ == AOFSyncMode::EVERYSEC){
                auto now = std::chrono::steady_clock::now();
                if(now - last_fsync_ >= std::chrono::seconds(1)){
                    fsync();
                    last_fsync_ = now;
                }
            }
            else if(sync_mode_ == AOFSyncMode::NO){//总是要fsync，防止数据长期未落盘
                auto now = std::chrono::steady_clock::now();
                if (now - last_fsync_ >= std::chrono::seconds(1)) {
                    fflush(aof_file_);
                    fsync();
                    last_fsync_ = now;
                }
            }
        }
    }
}


void AOFWriter::fsync(){
#ifdef _WIN32
    _commit(fileno(aof_file_));
#else 
    ::fsync(fileno(aof_file_));
#endif
}
