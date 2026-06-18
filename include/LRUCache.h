#include <unordered_map>
#include <list>
#include <utility>
#include <optional>

template<typename Key, typename Value>
class LRUCache{
public:
    LRUCache(size_t capacity):capacity_(capacity){}
    std::optional<Value> get(const Key& key){
        auto it = cache_map_.find(key);
        if(it == cache_map_.end()) return std::nullopt;
        LRU_list_.splice(LRU_list_.begin(), LRU_list_, it->second);
        return it->second->second;
    }
    void set(const Key& key, const Value& value){
        auto it = cache_map_.find(key);
        if(it != cache_map_.end()){
            it->second->second = value;
            LRU_list_.splice(LRU_list_.begin(), LRU_list_, it->second);
            return ;
        }
        if(LRU_list_.size() >= capacity_){
            const Key& oldestKey = LRU_list_.back().first;
            cache_map_.erase(oldestKey);
            LRU_list_.pop_back();
        }
        LRU_list_.emplace_front(key, value);
        cache_map_[key] = LRU_list_.begin();
    }
    bool Delete(const Key& key){
        auto it = cache_map_.find(key);
        if(it == cache_map_.end()) return false;
        LRU_list_.erase(it->second);
        cache_map_.erase(it);
        return true;
    }
    size_t size()const {return LRU_list_.size();}
    size_t capacity()const {return capacity_;}
private:
    size_t capacity_;
    std::list<std::pair<Key, Value>> LRU_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
}; 
