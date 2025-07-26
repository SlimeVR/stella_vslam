#ifndef STELLA_VSLAM_BENCHMARK_TIMER_H
#define STELLA_VSLAM_BENCHMARK_TIMER_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

namespace stella_vslam {
namespace benchmark {

class timer {
public:
    using clock_type = std::chrono::high_resolution_clock;
    using time_point = clock_type::time_point;
    using duration = std::chrono::duration<double, std::milli>;

    timer() : start_time_(clock_type::now()) {}

    void start() {
        start_time_ = clock_type::now();
    }

    double elapsed_ms() const {
        auto end_time = clock_type::now();
        return duration(end_time - start_time_).count();
    }

    double lap_ms() {
        auto end_time = clock_type::now();
        double elapsed = duration(end_time - start_time_).count();
        start_time_ = end_time;
        return elapsed;
    }

private:
    time_point start_time_;
};

struct timing_stats {
    std::string name;
    double total_time_ms = 0.0;
    double min_time_ms = std::numeric_limits<double>::max();
    double max_time_ms = 0.0;
    double avg_time_ms = 0.0;
    size_t call_count = 0;
    std::vector<double> samples;

    void add_sample(double time_ms) {
        total_time_ms += time_ms;
        min_time_ms = std::min(min_time_ms, time_ms);
        max_time_ms = std::max(max_time_ms, time_ms);
        call_count++;
        avg_time_ms = total_time_ms / call_count;
        samples.push_back(time_ms);
    }

    double get_percentile(double p) const {
        if (samples.empty()) return 0.0;
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    }

    void reset() {
        total_time_ms = 0.0;
        min_time_ms = std::numeric_limits<double>::max();
        max_time_ms = 0.0;
        avg_time_ms = 0.0;
        call_count = 0;
        samples.clear();
    }
};

class benchmark_manager {
public:
    static benchmark_manager& get_instance() {
        static benchmark_manager instance;
        return instance;
    }

    void record_time(const std::string& module, const std::string& function, double time_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = module + "::" + function;
        if (stats_.find(key) == stats_.end()) {
            stats_[key].name = key;
        }
        stats_[key].add_sample(time_ms);
    }

    timing_stats get_stats(const std::string& module, const std::string& function) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = module + "::" + function;
        auto it = stats_.find(key);
        return (it != stats_.end()) ? it->second : timing_stats{};
    }

    std::unordered_map<std::string, timing_stats> get_all_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void print_summary() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, timing_stats>> sorted_stats(stats_.begin(), stats_.end());
        std::sort(sorted_stats.begin(), sorted_stats.end(), 
                  [](const auto& a, const auto& b) { return a.second.total_time_ms > b.second.total_time_ms; });
        
        size_t max_name_length = 20;
        for (const auto& [key, stat] : sorted_stats) {
            max_name_length = std::max(max_name_length, key.length());
        }
        max_name_length = std::min(max_name_length, size_t(50));
        
        size_t total_width = max_name_length + 6*12 + 5;
        
        std::cout << "\n" << std::string(total_width, '=') << std::endl;
        std::cout << "STELLA-VSLAM BENCHMARK SUMMARY" << std::endl;
        std::cout << std::string(total_width, '=') << std::endl;

        std::cout << std::left << std::setw(max_name_length) << "Module::Function"
                  << std::right << std::setw(10) << "Calls"
                  << std::setw(12) << "Total(ms)"
                  << std::setw(12) << "Avg(ms)"
                  << std::setw(12) << "Min(ms)"
                  << std::setw(12) << "Max(ms)"
                  << std::setw(12) << "P95(ms)" << std::endl;
        std::cout << std::string(total_width, '-') << std::endl;

        for (const auto& [key, stat] : sorted_stats) {
            // Truncate long function names
            std::string display_name = key;
            if (display_name.length() > max_name_length) {
                display_name = display_name.substr(0, max_name_length - 3) + "...";
            }
            
            std::cout << std::left << std::setw(max_name_length) << display_name
                      << std::right << std::setw(10) << stat.call_count
                      << std::setw(12) << std::fixed << std::setprecision(2) << stat.total_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(2) << stat.avg_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(2) << stat.min_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(2) << stat.max_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(2) << stat.get_percentile(0.95)
                      << std::endl;
        }
        std::cout << std::string(total_width, '=') << std::endl;
        
        double total_processing_time = 0.0;
        size_t total_calls = 0;
        for (const auto& [key, stat] : stats_) {
            total_processing_time += stat.total_time_ms;
            total_calls += stat.call_count;
        }
        
        std::cout << "SUMMARY: " << sorted_stats.size() << " functions tracked, " 
                  << total_calls << " total calls, " 
                  << std::fixed << std::setprecision(2) << total_processing_time << " ms total time" << std::endl;
        std::cout << std::string(total_width, '=') << std::endl;
    }

    void save_to_csv(const std::string& filename) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << filename << " for writing" << std::endl;
            return;
        }

        file << "Module,Function,CallCount,TotalTime_ms,AvgTime_ms,MinTime_ms,MaxTime_ms,P50_ms,P95_ms,P99_ms\n";

        for (const auto& [key, stat] : stats_) {
            size_t pos = key.find("::");
            std::string module = (pos != std::string::npos) ? key.substr(0, pos) : key;
            std::string function = (pos != std::string::npos) ? key.substr(pos + 2) : "";

            file << module << ","
                 << function << ","
                 << stat.call_count << ","
                 << std::fixed << std::setprecision(3) << stat.total_time_ms << ","
                 << std::fixed << std::setprecision(3) << stat.avg_time_ms << ","
                 << std::fixed << std::setprecision(3) << stat.min_time_ms << ","
                 << std::fixed << std::setprecision(3) << stat.max_time_ms << ","
                 << std::fixed << std::setprecision(3) << stat.get_percentile(0.50) << ","
                 << std::fixed << std::setprecision(3) << stat.get_percentile(0.95) << ","
                 << std::fixed << std::setprecision(3) << stat.get_percentile(0.99) << "\n";
        }

        std::cout << "Benchmark results saved to " << filename << std::endl;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.clear();
    }

    void enable(bool enabled = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = enabled;
    }

    bool is_enabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, timing_stats> stats_;
    bool enabled_ = true;

    benchmark_manager() = default;
};

//! RAII helper class for automatic timing
class scoped_benchmark_timer {
public:
    scoped_benchmark_timer(const std::string& module, const std::string& function)
        : module_(module), function_(function), timer_() {}
    
    ~scoped_benchmark_timer() {
        if (benchmark_manager::get_instance().is_enabled()) {
            benchmark_manager::get_instance().record_time(module_, function_, timer_.elapsed_ms());
        }
    }

private:
    std::string module_;
    std::string function_;
    timer timer_;
};

#define STELLA_BENCHMARK_TIMER(module, function) \
    stella_vslam::benchmark::scoped_benchmark_timer __benchmark_timer(module, function);

} // namespace benchmark
} // namespace stella_vslam

#endif // STELLA_VSLAM_BENCHMARK_TIMER_H