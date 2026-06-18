#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <algorithm>
#include <sstream>

using boost::asio::ip::tcp;

// ---------- Configuration ----------
struct Config {
    std::string host = "127.0.0.1";
    std::string port = "6001";
    int threads = 10;
    int64_t total_requests = 100000;
    double read_ratio = 1.0;
    int value_size = 256;
    int key_range = 10000;
    int warmup_seconds = 5;
    int max_redirects = 3;          // Max MOVED redirect attempts per request
};

// ---------- Statistics (thread-safe) ----------
class Stats {
public:
    void record_latency(int64_t us) {
        std::lock_guard<std::mutex> lock(mtx_);
        latencies_.push_back(us);
        total_latency_us_ += us;
        success_count_++;
        total_responses_++;
    }
    void record_error() {
        std::lock_guard<std::mutex> lock(mtx_);
        error_count_++;
        total_responses_++;
    }
    void inc_sent() {
        std::lock_guard<std::mutex> lock(mtx_);
        sent_count_++;
    }
    int64_t get_sent() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return sent_count_;
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        latencies_.clear();
        total_latency_us_ = 0;
        success_count_ = 0;
        error_count_ = 0;
        sent_count_ = 0;
        total_responses_ = 0;
        duration_sec_ = 0;
    }
    void set_duration(double sec) { duration_sec_ = sec; }
    void print() const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (success_count_ == 0) {
            std::cout << "No successful requests.\n";
            return;
        }
        std::vector<int64_t> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        double avg = total_latency_us_ / (double)success_count_;
        auto percentile = [&](double p) -> int64_t {
            size_t idx = (size_t)(p * sorted.size());
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            return sorted[idx];
        };
        std::cout << "\n========== Benchmark Results ==========\n";
        std::cout << "Total requests       : " << total_responses_ << "\n";
        std::cout << "Successful           : " << success_count_ << "\n";
        std::cout << "Errors               : " << error_count_ << "\n";
        std::cout << "Duration (sec)       : " << std::fixed << std::setprecision(2) << duration_sec_ << "\n";
        std::cout << "QPS                  : " << static_cast<int64_t>(total_responses_ / duration_sec_) << "\n";
        std::cout << "Avg latency (us)     : " << static_cast<int64_t>(avg) << "\n";
        std::cout << "P50 latency (us)     : " << percentile(0.50) << "\n";
        std::cout << "P95 latency (us)     : " << percentile(0.95) << "\n";
        std::cout << "P99 latency (us)     : " << percentile(0.99) << "\n";
        std::cout << "Max latency (us)     : " << sorted.back() << "\n";
    }
private:
    mutable std::mutex mtx_;
    std::vector<int64_t> latencies_;
    int64_t total_latency_us_ = 0;
    int64_t success_count_ = 0;
    int64_t error_count_ = 0;
    int64_t sent_count_ = 0;
    int64_t total_responses_ = 0;
    double duration_sec_ = 0;
};

// ---------- Helper: send command and read response (with possible MOVED) ----------
// Returns latency in microseconds, or -1 on error.
// On MOVED, it will reconnect to the new host:port and retry (up to max_redirects).
int64_t send_one_request_with_redirect(tcp::socket& sock,
                                        boost::asio::io_context& io,
                                        const std::string& cmd,
                                        bool is_read,
                                        const Config& cfg,
                                        std::string& current_host,
                                        std::string& current_port) {
    int redirects = 0;
    while (redirects <= cfg.max_redirects) {
        auto start = std::chrono::steady_clock::now();

        // Write command
        boost::system::error_code ec;
        boost::asio::write(sock, boost::asio::buffer(cmd), ec);
        if (ec) return -1;

        // Read first line (response header)
        boost::asio::streambuf buf;
        size_t n = boost::asio::read_until(sock, buf, "\r\n", ec);
        if (ec) return -1;

        std::istream is(&buf);
        std::string first_line;
        std::getline(is, first_line, '\r');
        is.get(); // consume '\n'

        // Check for MOVED response
        if (first_line.find("MOVED") == 0) {
            // Format: "MOVED node_id host port"
            std::stringstream ss(first_line);
            std::string moved, node_id, addr;
            ss >> moved >> node_id >> addr;
            size_t colon = addr.find(':');
            if (colon != std::string::npos) {
                std::string new_host = addr.substr(0, colon);
                std::string new_port = addr.substr(colon + 1);
                // Reconnect to new node
                boost::system::error_code ec2;
                sock.close(ec2);
                tcp::resolver resolver(io);
                tcp::resolver::results_type endpoints = resolver.resolve(new_host, new_port, ec2);
                if (ec2) return -1;
                boost::asio::connect(sock, endpoints, ec2);
                if (ec2) return -1;
                current_host = new_host;
                current_port = new_port;
                redirects++;
                continue; // retry the same command
            } else {
                return -1; // malformed MOVED
            }
        }

        // Not MOVED, parse normal RESP response
        if (is_read) {
            // GET response
            if (first_line.empty() || first_line[0] != '$') return -1;
            if (first_line == "$-1") {
                // key not found, success
                auto end = std::chrono::steady_clock::now();
                return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            }
            int len = std::stoi(first_line.substr(1));
            if (len > 0) {
                boost::asio::read(sock, buf, boost::asio::transfer_exactly(len + 2), ec);
                if (ec) return -1;
                // data consumed, no need to store
            }
        } else {
            // SET response: should be "$0" or "+OK"
            if (first_line != "$0" && first_line != "+OK") return -1;
        }

        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    return -1; // too many redirects
}

// ---------- Worker thread function ----------
void worker(int id, const Config& cfg, Stats& stats, std::atomic<int64_t>& global_sent, int64_t target) {
    try {
        boost::asio::io_context io;
        tcp::socket sock(io);
        tcp::resolver resolver(io);

        // Initial connection
        std::string current_host = cfg.host;
        std::string current_port = cfg.port;
        auto endpoints = resolver.resolve(current_host, current_port);
        boost::asio::connect(sock, endpoints);

        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> key_dist(0, cfg.key_range - 1);
        std::string value_data(cfg.value_size, 'x');

        int64_t sent = 0;
        while (sent < target) {
            bool is_read = (rng() % 100) < (cfg.read_ratio * 100);
            std::string key = std::to_string(key_dist(rng));
            std::string cmd;
            if (is_read) {
                // GET command: get\r\nkey\r\n
                cmd = "get\r\n" + key + "\r\n";
            } else {
                // SET command: set\r\nkey\r\nlen\r\ndata\r\n
                cmd = "set\r\n" + key + "\r\n" + std::to_string(value_data.size()) + "\r\n" + value_data + "\r\n";
            }

            int64_t latency = send_one_request_with_redirect(sock, io, cmd, is_read, cfg, current_host, current_port);
            if (latency >= 0) {
                stats.record_latency(latency);
            } else {
                stats.record_error();
            }
            sent++;
            global_sent++;
            stats.inc_sent();
        }
    } catch (std::exception& e) {
        std::cerr << "Worker " << id << " exception: " << e.what() << std::endl;
        stats.record_error();
    }
}

// ---------- Warmup phase ----------
void run_warmup(const Config& cfg) {
    std::cout << "Warming up for " << cfg.warmup_seconds << " seconds...\n";
    std::vector<std::thread> warmup_threads;
    std::atomic<bool> stop_flag(false);
    for (int i = 0; i < cfg.threads; ++i) {
        warmup_threads.emplace_back([&cfg, &stop_flag, i]() {
            try {
                boost::asio::io_context io;
                tcp::socket sock(io);
                tcp::resolver resolver(io);
                auto endpoints = resolver.resolve(cfg.host, cfg.port);
                boost::asio::connect(sock, endpoints);
                std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> key_dist(0, cfg.key_range - 1);
                std::string value_data(cfg.value_size, 'x');
                while (!stop_flag) {
                    bool is_read = (rng() % 100) < (cfg.read_ratio * 100);
                    std::string key = std::to_string(key_dist(rng));
                    std::string cmd;
                    if (is_read) {
                        cmd = "get\r\n" + key + "\r\n";
                    } else {
                        cmd = "set\r\n" + key + "\r\n" + std::to_string(value_data.size()) + "\r\n" + value_data + "\r\n";
                    }
                    boost::system::error_code ec;
                    boost::asio::write(sock, boost::asio::buffer(cmd), ec);
                    if (ec) break;
                    boost::asio::streambuf buf;
                    boost::asio::read_until(sock, buf, "\r\n", ec);
                    if (ec) break;
                    // For GET, we may need to read extra data, but ignoring for warmup
                }
            } catch (...) {}
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(cfg.warmup_seconds));
    stop_flag = true;
    for (auto& t : warmup_threads) t.join();
}

// ---------- Main ----------
int main(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i+1 < argc) cfg.host = argv[++i];
        else if (arg == "-p" && i+1 < argc) cfg.port = argv[++i];
        else if (arg == "-t" && i+1 < argc) cfg.threads = std::stoi(argv[++i]);
        else if (arg == "-n" && i+1 < argc) cfg.total_requests = std::stoll(argv[++i]);
        else if (arg == "-r" && i+1 < argc) cfg.read_ratio = std::stod(argv[++i]);
        else if (arg == "-vsize" && i+1 < argc) cfg.value_size = std::stoi(argv[++i]);
        else if (arg == "-keys" && i+1 < argc) cfg.key_range = std::stoi(argv[++i]);
        else if (arg == "-warmup" && i+1 < argc) cfg.warmup_seconds = std::stoi(argv[++i]);
        else if (arg == "-max-redirect" && i+1 < argc) cfg.max_redirects = std::stoi(argv[++i]);
        else {
            std::cout << "Usage: " << argv[0] << " [-h host] [-p port] [-t threads] [-n total] [-r read_ratio] "
                      << "[-vsize size] [-keys range] [-warmup sec] [-max-redirect num]\n";
            return 0;
        }
    }

    std::cout << "Benchmark Configuration:\n"
              << "  Server: " << cfg.host << ":" << cfg.port << "\n"
              << "  Threads: " << cfg.threads << "\n"
              << "  Total requests: " << cfg.total_requests << "\n"
              << "  Read ratio: " << cfg.read_ratio << "\n"
              << "  Value size: " << cfg.value_size << " bytes\n"
              << "  Key range: " << cfg.key_range << "\n"
              << "  Warmup seconds: " << cfg.warmup_seconds << "\n"
              << "  Max redirects: " << cfg.max_redirects << "\n";

    Stats stats;
    std::atomic<int64_t> global_sent(0);
    int64_t per_thread = cfg.total_requests / cfg.threads;
    int64_t remainder = cfg.total_requests % cfg.threads;

    // Warmup
    run_warmup(cfg);

    // Formal test
    std::cout << "Starting formal test...\n";
    stats.reset();
    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < cfg.threads; ++i) {
        int64_t target = per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, i, std::cref(cfg), std::ref(stats), std::ref(global_sent), target);
    }
    for (auto& t : threads) t.join();

    auto end_time = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(end_time - start_time).count();
    stats.set_duration(duration);
    stats.print();

    return 0;
}
