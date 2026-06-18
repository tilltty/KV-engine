#include "protocolParser.h"

std::string protocolParser::ParseAndExecute(const std::string& buffer, size_t& consumed) {
    std::string reply;
    parse_pos_ = 0;
    size_t crl = buffer.find("\r\n", parse_pos_);
    if (crl == std::string::npos) return "";   // 需要更多数据
    std::string c = buffer.substr(parse_pos_, crl - parse_pos_);
    if(c == "get" || c == "set" || c == "del") state_ = State::READ_COMMAND;
    std::cout<<(state_ == State::READ_COMMAND ? "READ_COMMAND\n" : "READ_DATA\n");
    while (true) {
        if (state_ == State::READ_COMMAND) {
            size_t crlf = buffer.find("\r\n", parse_pos_);
            if (crlf == std::string::npos) return "";   // 需要更多数据
            std::string cmd = buffer.substr(parse_pos_, crlf - parse_pos_);
            parse_pos_ = crlf + 2;
            if (cmd == "get" || cmd == "del") {
                // 对于 get/del，还需读取 key 行
                crlf = buffer.find("\r\n", parse_pos_);
                if (crlf == std::string::npos) return "";
                std::string key = buffer.substr(parse_pos_, crlf - parse_pos_);
                parse_pos_ = crlf + 2;
                consumed = parse_pos_;
                NodeInfo owner = hash_ring_->getNode(key);
                if (owner.id != local_node_.id) {
                    state_ = State::READ_COMMAND;
                    return "MOVED " + owner.id + " " + owner.host + ":" + std::to_string(owner.port) + "\r\n";
                }

                if (cmd == "get") {
                    std::string value = storage_->get(key);
                    reply = value.empty() ? "$-1\r\n" : "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
                } else { // del
                    reply = storage_->del(key) ? "OK\r\n" : "-ERROR key not found\r\n";
                }
                state_ = State::READ_COMMAND;
                
                return reply;
            }
            else if (cmd == "set") {
                // 读取 key 行
                crlf = buffer.find("\r\n", parse_pos_);
                if (crlf == std::string::npos) return "";
                current_key_ = buffer.substr(parse_pos_, crlf - parse_pos_);
                parse_pos_ = crlf + 2;

                crlf = buffer.find("\r\n", parse_pos_);
                if (crlf == std::string::npos) return "";
                std::string len_str = buffer.substr(parse_pos_, crlf - parse_pos_);
                parse_pos_ = crlf + 2;
                expected_length_ = std::stoull(len_str);

                data_buffer_.clear();
                data_read_ = 0;
                state_ = State::READ_DATA;
            }
            else {
                reply = "-ERROR unknown command '" + cmd + "'\r\n";
                state_ = State::READ_COMMAND;
                return reply;
            }
        }

        if (state_ == State::READ_DATA) {
            size_t remaining = expected_length_ - data_read_;
            size_t available = buffer.size() - parse_pos_;
            
            if (remaining == 0) {
                if (buffer.size() - parse_pos_ >= 2 && buffer[parse_pos_] == '\r' && buffer[parse_pos_+1] == '\n') {
                    parse_pos_ += 2;
                    consumed = parse_pos_;
                    NodeInfo owner = hash_ring_->getNode(current_key_);
                    if (owner.id != local_node_.id) {
                        state_ = State::READ_COMMAND;
                        return "MOVED " + owner.id + " " + owner.host + ":" + std::to_string(owner.port) + "\r\n";
                    }
                    storage_->set(current_key_, data_buffer_);
                    reply = "$0\r\n";
                    state_ = State::READ_COMMAND;
                    return reply;
                } else {
                    return ""; 
                }
            }

            if (available == 0) return ""; 

            size_t take = std::min(remaining, available);
            data_buffer_.append(buffer, parse_pos_, take); 
            data_read_ += take;
            parse_pos_ += take;

            if (data_read_ == expected_length_) {
                continue;
            }
            return ""; 
        }
    }
    return "";
}

protocolParser::protocolParser(std::shared_ptr<storageEngine> storage,  
    std::shared_ptr<consistentHash> hash_ring, 
    NodeInfo local_node)
    : storage_(std::move(storage)),
    hash_ring_(hash_ring),
    local_node_(local_node){}