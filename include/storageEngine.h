#include <unordered_map>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <memory>
#include <string>
#include "LRUCache.h"
#include "AOFWriter.h"
#include <iostream>
//namespace fs = std::filesystem;

class storageEngine{
public:
    storageEngine(size_t max_keys = 1024, const std::string& aof_path = "appendonly.aof") 
    : cache_(max_keys), aof_writer_(std::make_unique<AOFWriter>(aof_path)),aof_path_(aof_path) {
        loadFromAOF();
    }
    ~storageEngine(){
        if(aof_writer_) aof_writer_->stop();
    }
    //std::string Store(const std::string& key, const std::string& data);
    //std::string Retrieve(const std::string& key);
    std::string set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    //size_t FileSize(const std::string& key);
    bool del(const std::string& key);
    //void SetBaseDir(const std::string& dir);
private:

    void loadFromAOF();
    //std::string sanitize(const std::string& key);
    LRUCache<std::string, std::string> cache_; 
    std::unique_ptr<AOFWriter> aof_writer_;
    std::string aof_path_;
    //std::string base_dir_ = "./data";
};


