#include "storageEngine.h"



std::string storageEngine::set(const std::string& key, const std::string& value){
    cache_.set(key, value);
    aof_writer_->appendCommand("set " + key + " " + value);
    return "ok\r\n";
}

std::string storageEngine::get(const std::string& key){
    auto opt_val = cache_.get(key);
    if(!opt_val.has_value()){
        return "";
    }
    else return *opt_val;
}



bool storageEngine::del(const std::string& key){
    if(cache_.Delete(key)){
        aof_writer_->appendCommand("del " + key);
        return true;
    }
    return false;
}

void storageEngine::loadFromAOF(){
    //std::cout<<"loadFromAOF path: "<<aof_path_<<std::endl;
    std::ifstream file(aof_path_);
    if(!file.is_open()){
        //std::cout<<"file not open\n";
        return;
    }
    std::string line;
    while(std::getline(file, line)){
        if(!line.empty() && line.back() == '\n'){
            line.pop_back();
        }

        if(line.compare(0, 4, "set ") == 0){
            size_t space1 = line.find(" ", 4);
            if(space1!=std::string::npos){
                std::string key = line.substr(4, space1 - 4);
                std::string value = line.substr(space1 + 1);
                cache_.set(key, value);
            }
        }
        else if(line.compare(0, 4, "del ") == 0){
            std::string key = line.substr(4);
            cache_.Delete(key);
        }
    }
    printf("loaded AOF, current size: %zu\n", cache_.size());
}
