#include "../core/router.h"
#include <queue>
#include <cmath>
#include <iostream>
#include <limits>

namespace asm_core {

void HierarchicalRouter::build_graph(const std::vector<std::array<float, 128>>& expert_centroids) {
    num_experts = expert_centroids.size();
    nodes.resize(num_experts);
    
    if (num_experts == 0) {
        std::cerr << "No experts to build graph with" << std::endl;
        return;
    }
    
    // Copy centroids to linear array for SIMD
    for (size_t i = 0; i < num_experts; ++i) {
        nodes[i].expert_id = static_cast<uint32_t>(i);
        nodes[i].centroid = expert_centroids[i];
        std::memcpy(centroids_linear[i], expert_centroids[i].data(), 
                   sizeof(float) * 128);
        
        // Random layer assignment (exponential distribution)
        double r = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        nodes[i].max_layer = static_cast<uint8_t>(std::min(3, static_cast<int>(-std::log(r))));
    }
    
    std::cout << "Building HNSW graph with " << num_experts << " experts..." << std::endl;
    
    // Build HNSW graph layer by layer (from top to bottom)
    for (int layer = 3; layer >= 0; --layer) {
        std::vector<uint32_t> nodes_at_layer;
        for (uint32_t i = 0; i < num_experts; ++i) {
            if (nodes[i].max_layer >= static_cast<uint8_t>(layer)) {
                nodes_at_layer.push_back(i);
            }
        }
        
        if (nodes_at_layer.empty()) continue;
        
        // For each node, find nearest neighbors and add connections
        for (uint32_t node_id : nodes_at_layer) {
            // Simple approach: connect to M nearest neighbors at this layer
            std::vector<std::pair<float, uint32_t>> distances;
            
            for (uint32_t other_id : nodes_at_layer) {
                if (node_id == other_id) continue;
                
                float dist = avx2_distance(centroids_linear[node_id], 
                                          centroids_linear[other_id]);
                distances.emplace_back(dist, other_id);
            }
            
            // Sort by distance
            std::sort(distances.begin(), distances.end());
            
            // Connect to M nearest
            int connections = std::min(M, static_cast<int>(distances.size()));
            for (int i = 0; i < connections; ++i) {
                nodes[node_id].neighbors[layer].push_back(distances[i].second);
                // Add reverse connection
                nodes[distances[i].second].neighbors[layer].push_back(node_id);
            }
        }
    }
    
    // Set entry point (node with highest layer)
    entry_point = 0;
    for (uint32_t i = 1; i < num_experts; ++i) {
        if (nodes[i].max_layer > nodes[entry_point].max_layer) {
            entry_point = i;
        }
    }
    
    std::cout << "HNSW graph built successfully. Entry point: " << entry_point << std::endl;
}

std::vector<uint32_t> HierarchicalRouter::search_nearest(const float* query_vec, int top_k, 
                                                          float* confidence) {
    if (num_experts == 0) {
        if (confidence) *confidence = 0.0f;
        return {};
    }
    
    // Start from entry point and go down layer by layer
    uint32_t current_node = entry_point;
    
    // Greedy search from top layer to bottom
    for (int layer = 3; layer >= 0; --layer) {
        bool changed = true;
        while (changed) {
            changed = false;
            float current_dist = avx2_distance(query_vec, centroids_linear[current_node]);
            
            // Check all neighbors at this layer
            for (uint32_t neighbor_id : nodes[current_node].neighbors[layer]) {
                float neighbor_dist = avx2_distance(query_vec, centroids_linear[neighbor_id]);
                
                if (neighbor_dist < current_dist) {
                    current_node = neighbor_id;
                    current_dist = neighbor_dist;
                    changed = true;
                }
            }
        }
    }
    
    // Now at layer 0, do expanded search with ef_search
    std::vector<uint32_t> candidates = search_layer(query_vec, current_node, ef_search, 0);
    
    // Calculate confidence based on distance to nearest
    float min_dist = std::numeric_limits<float>::max();
    if (!candidates.empty()) {
        min_dist = avx2_distance(query_vec, centroids_linear[candidates[0]]);
    }
    
    // Convert distance to confidence (closer = higher confidence)
    float conf = 1.0f / (1.0f + min_dist);
    if (confidence) {
        *confidence = conf;
    }
    
    // Return top_k closest
    std::vector<uint32_t> result;
    if (static_cast<int>(candidates.size()) <= top_k) {
        result = candidates;
    } else {
        // Sort candidates by distance and return top_k
        std::vector<std::pair<float, uint32_t>> dist_pairs;
        for (uint32_t cand_id : candidates) {
            float dist = avx2_distance(query_vec, centroids_linear[cand_id]);
            dist_pairs.emplace_back(dist, cand_id);
        }
        
        std::sort(dist_pairs.begin(), dist_pairs.end());
        
        for (int i = 0; i < top_k && i < static_cast<int>(dist_pairs.size()); ++i) {
            result.push_back(dist_pairs[i].second);
        }
    }
    
    // Fallback: if confidence is low and we have a fallback expert, add it
    if (has_fallback && conf < 0.6f) {
        // Add fallback expert to results
        result.push_back(fallback_expert_id);
    }
    
    return result;
}

void HierarchicalRouter::add_expert(uint32_t expert_id, const std::array<float, 128>& centroid) {
    if (expert_id >= 50000) {
        std::cerr << "Expert ID out of range: " << expert_id << std::endl;
        return;
    }
    
    nodes[expert_id].expert_id = expert_id;
    nodes[expert_id].centroid = centroid;
    std::memcpy(centroids_linear[expert_id], centroid.data(), sizeof(float) * 128);
    num_experts = std::max(num_experts, static_cast<size_t>(expert_id) + 1);
}

std::vector<uint32_t> HierarchicalRouter::search_layer(const float* query, uint32_t entry, 
                                                        int ef, int layer) {
    // Priority queue for candidates (min-heap by distance)
    using Candidate = std::pair<float, uint32_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> candidates;
    
    // Visited set
    std::vector<bool> visited(num_experts, false);
    
    // Result set (kept sorted)
    std::vector<Candidate> result;
    
    float entry_dist = avx2_distance(query, centroids_linear[entry]);
    candidates.emplace(entry_dist, entry);
    result.emplace_back(entry_dist, entry);
    visited[entry] = true;
    
    while (!candidates.empty()) {
        auto [worst_dist, worst_id] = candidates.top();
        
        // If worst in candidates is farther than best in result, we can stop
        if (worst_dist > result.back().first && result.size() >= static_cast<size_t>(ef)) {
            break;
        }
        
        candidates.pop();
        
        // Explore neighbors
        for (uint32_t neighbor_id : nodes[worst_id].neighbors[layer]) {
            if (visited[neighbor_id]) continue;
            visited[neighbor_id] = true;
            
            float dist = avx2_distance(query, centroids_linear[neighbor_id]);
            
            // Add to candidates
            candidates.emplace(dist, neighbor_id);
            
            // Add to result if better than worst
            if (result.size() < static_cast<size_t>(ef)) {
                result.emplace_back(dist, neighbor_id);
                std::sort(result.begin(), result.end());
            } else if (dist < result.back().first) {
                result.back() = {dist, neighbor_id};
                std::sort(result.begin(), result.end());
            }
        }
    }
    
    // Extract IDs
    std::vector<uint32_t> result_ids;
    for (const auto& [dist, id] : result) {
        result_ids.push_back(id);
    }
    
    return result_ids;
}

std::vector<uint32_t> HierarchicalRouter::select_neighbors(const std::vector<uint32_t>& candidates, int m) {
    // Simple heuristic: select m closest candidates
    if (static_cast<int>(candidates.size()) <= m) {
        return candidates;
    }
    
    // This is a simplified version - in production, use heuristics from HNSW paper
    return std::vector<uint32_t>(candidates.begin(), candidates.begin() + m);
}

float HierarchicalRouter::distance_between(uint32_t id1, uint32_t id2) {
    if (id1 >= num_experts || id2 >= num_experts) {
        return std::numeric_limits<float>::max();
    }
    
    return avx2_distance(centroids_linear[id1], centroids_linear[id2]);
}

} // namespace asm_core
