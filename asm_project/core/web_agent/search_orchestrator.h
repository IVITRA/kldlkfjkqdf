#pragma once
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <memory>
#include <chrono>
#include <algorithm>
#include <optional>
#include "../context_protocol.h"

namespace asm_web {

struct SearchQuery {
    std::string original_question;
    std::vector<std::string> extracted_keywords;
    std::string domain_hint; // "programming", "history", "religion", etc.
    bool requires_realtime;  // Does it need current news/prices?
};

struct WebResult {
    std::string url;
    std::string title;
    std::string snippet;
    float credibility_score; // 0.0 - 1.0 (source credibility rating)
    std::string content_type; // "article", "documentation", "forum", "video_transcript"
    std::chrono::system_clock::time_point fetch_time;
};

// Simple LRU Cache for search results
template<typename K, typename V>
class LRUCache {
private:
    struct Entry {
        K key;
        V value;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::vector<Entry> entries;
    size_t max_size;
    std::mutex cache_mutex;
    
public:
    LRUCache(size_t size) : max_size(size) {}
    
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        for (const auto& entry : entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return std::nullopt;
    }
    
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        // Remove if already exists
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&key](const Entry& e) { return e.key == key; }),
            entries.end()
        );
        
        // Add new entry
        entries.push_back({key, value, std::chrono::system_clock::now()});
        
        // Remove oldest if over capacity
        if (entries.size() > max_size) {
            entries.erase(entries.begin());
        }
    }
    
    void clear_old_entries(int days) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(days * 24);
        
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [cutoff](const Entry& e) { return e.timestamp < cutoff; }),
            entries.end()
        );
    }
};

class CuriousSearchAgent {
private:
    // Multiple search engines for maximum coverage
    std::vector<std::string> search_engines = {
        "https://duckduckgo.com/html/?q=",  // Privacy-focused
        "https://search.brave.com/search?q=" // No tracking
    };
    
    // Cache for recent searches (don't search same thing twice in a day)
    LRUCache<std::string, std::vector<WebResult>> search_cache{1000};
    
    // Extract keywords from query
    std::vector<std::string> extract_keywords(const std::string& query) {
        std::vector<std::string> keywords;
        std::string current_word;
        
        for (char c : query) {
            if (std::isalnum(c) || c == ' ') {
                if (c == ' ' && !current_word.empty()) {
                    if (current_word.length() > 2) { // Skip short words
                        keywords.push_back(current_word);
                    }
                    current_word.clear();
                } else if (c != ' ') {
                    current_word += c;
                }
            }
        }
        
        if (!current_word.empty() && current_word.length() > 2) {
            keywords.push_back(current_word);
        }
        
        return keywords;
    }
    
    // Check if query contains temporal markers
    bool contains_temporal_markers(const std::string& text) {
        std::vector<std::string> markers = {
            "today", "yesterday", "latest", "recent", "new", "current",
            "2026", "2025", "this year", "this month", "now",
            "اليوم", "الآن", "الأخير", "الجديد", "الحالي"
        };
        
        std::string lower_text = text;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        
        for (const auto& marker : markers) {
            if (lower_text.find(marker) != std::string::npos) {
                return true;
            }
        }
        
        return false;
    }
    
    // Query single search engine
    std::vector<WebResult> query_single_engine(const std::string& engine_url, 
                                               const SearchQuery& query) {
        std::vector<WebResult> results;
        
        // In production, use libcurl to fetch results
        // For now, return empty (would require actual HTTP implementation)
        
        // Example implementation with curl would be:
        // CURL* curl = curl_easy_init();
        // std::string url = engine_url + encode_query(query.original_question);
        // curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // ... perform request and parse results
        
        return results;
    }

public:
    CuriousSearchAgent() : search_cache(1000) {}
    
    // Decide whether to search online
    bool should_search_online(const asm_core::ThoughtPassport& passport, 
                             const std::string& original_query,
                             float confidence_threshold = 0.6f) {
        if (passport.confidence < confidence_threshold) return true;
        
        // If question contains recent temporal markers
        if (contains_temporal_markers(original_query)) return true;
        
        return false;
    }
    
    // Parallel search across multiple engines
    std::vector<WebResult> search_parallel(const SearchQuery& query) {
        // Check cache first
        auto cached = search_cache.get(query.original_question);
        if (cached.has_value()) {
            return cached.value();
        }
        
        std::vector<std::future<std::vector<WebResult>>> futures;
        
        for (const auto& engine : search_engines) {
            futures.push_back(std::async(std::launch::async, [&]() {
                return query_single_engine(engine, query);
            }));
        }
        
        // Merge results and remove duplicates
        std::vector<WebResult> all_results;
        for (auto& f : futures) {
            auto res = f.get();
            all_results.insert(all_results.end(), res.begin(), res.end());
        }
        
        // Sort by credibility
        std::sort(all_results.begin(), all_results.end(), 
                 [](const auto& a, const auto& b) {
                     return a.credibility_score > b.credibility_score;
                 });
        
        search_cache.put(query.original_question, all_results);
        return all_results;
    }
    
    // Get cache statistics
    size_t get_cache_size() const {
        return 1000; // Simplified
    }
    
    // Clear cache
    void clear_cache() {
        search_cache.clear_old_entries(0); // Clear all
    }
};

} // namespace asm_web
