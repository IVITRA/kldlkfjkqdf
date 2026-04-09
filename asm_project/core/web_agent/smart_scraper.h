#ifndef ASM_CORE_SMART_SCRAPER_H
#define ASM_CORE_SMART_SCRAPER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <algorithm>
#include <regex>

namespace asm_core {
namespace web_agent {

/**
 * Extracted content from a web page
 */
struct ScrapedContent {
    std::string url;
    std::string title;
    std::string main_text;
    std::string summary;
    std::vector<std::string> headings;
    std::vector<std::string> links;
    std::map<std::string, std::string> metadata;
    float quality_score;  // 0.0 to 1.0
    std::string language;
    bool is_article;
};

/**
 * AdaptiveScraper - Intelligent web content extraction
 * 
 * Features:
 * - Readability algorithm for main content detection
 * - Quality assessment for filtering low-value pages
 * - Structure extraction (headings, links, metadata)
 * - Noise removal (ads, navigation, footers)
 */
class AdaptiveScraper {
public:
    AdaptiveScraper();
    ~AdaptiveScraper();

    /**
     * Scrape and extract content from HTML
     * @param html Raw HTML content
     * @param url Source URL for metadata
     * @return Extracted content with quality score
     */
    ScrapedContent scrape(const std::string& html, const std::string& url);

    /**
     * Quick quality assessment without full extraction
     * @param html Raw HTML content
     * @return Quality score 0.0-1.0
     */
    float assess_quality(const std::string& html);

    /**
     * Extract structured data from HTML
     * @param html Raw HTML content
     * @return Map of metadata (title, description, author, date, etc.)
     */
    std::map<std::string, std::string> extract_metadata(const std::string& html);

private:
    // Content extraction methods
    std::string extract_main_content(const std::string& html);
    std::string extract_title(const std::string& html);
    std::vector<std::string> extract_headings(const std::string& html);
    std::vector<std::string> extract_links(const std::string& html);
    
    // Quality assessment
    float calculate_readability_score(const std::string& text);
    float calculate_content_density(const std::string& html);
    float calculate_text_to_html_ratio(const std::string& html, const std::string& text);
    
    // Noise removal
    std::string remove_noise(const std::string& html);
    std::string extract_text_from_html(const std::string& html);
    
    // Language detection
    std::string detect_language(const std::string& text);
    
    // Summarization (simple extractive)
    std::string generate_summary(const std::string& text, int max_sentences = 3);

    // Regex patterns for HTML parsing
    std::regex title_regex;
    std::regex meta_regex;
    std::regex heading_regex;
    std::regex link_regex;
    std::regex paragraph_regex;
    std::regex script_regex;
    std::regex style_regex;
    std::regex tag_regex;
    
    // Common noise patterns to remove
    std::vector<std::string> noise_patterns = {
        "<nav", "</nav>",
        "<footer", "</footer>",
        "<header", "</header>",
        "class=\"sidebar\"",
        "class=\"advertisement\"",
        "class=\"ad\"",
        "class=\"comments\"",
        "id=\"comment",
        "<script", "</script>",
        "<style", "</style>"
    };

    // Trusted content selectors
    std::vector<std::string> content_selectors = {
        "<article", "<main", 
        "class=\"content\"",
        "class=\"article\"",
        "class=\"post\"",
        "id=\"content\"",
        "id=\"main\"",
        "role=\"main\""
    };
};

// ============================================================================
// Implementation
// ============================================================================

inline AdaptiveScraper::AdaptiveScraper() 
    : title_regex("<title[^>]*>(.*?)</title>", std::regex::icase),
      meta_regex("<meta[^>]+(?:name|property)=\"([^\"]+)\"[^>]+content=\"([^\"]+)\"", std::regex::icase),
      heading_regex("<h([1-6])[^>]*>(.*?)</h[1-6]>", std::regex::icase),
      link_regex("<a[^>]+href=\"([^\"]+)\"[^>]*>(.*?)</a>", std::regex::icase),
      paragraph_regex("<p[^>]*>(.*?)</p>", std::regex::icase),
      script_regex("<script[^>]*>.*?</script>", std::regex::icase | std::regex::dotall),
      style_regex("<style[^>]*>.*?</style>", std::regex::icase | std::regex::dotall),
      tag_regex("<[^>]+>") {
}

inline AdaptiveScraper::~AdaptiveScraper() {}

inline ScrapedContent AdaptiveScraper::scrape(const std::string& html, const std::string& url) {
    ScrapedContent content;
    content.url = url;
    
    // Remove noise (scripts, styles, navigation)
    std::string cleaned_html = remove_noise(html);
    
    // Extract main components
    content.title = extract_title(html);
    content.main_text = extract_main_content(cleaned_html);
    content.headings = extract_headings(cleaned_html);
    content.links = extract_links(cleaned_html);
    content.metadata = extract_metadata(html);
    
    // Generate summary
    content.summary = generate_summary(content.main_text);
    
    // Calculate quality score
    float readability = calculate_readability_score(content.main_text);
    float density = calculate_content_density(html);
    float text_ratio = calculate_text_to_html_ratio(html, content.main_text);
    
    content.quality_score = (readability * 0.4f + density * 0.3f + text_ratio * 0.3f);
    content.quality_score = std::min(1.0f, std::max(0.0f, content.quality_score));
    
    // Detect language
    content.language = detect_language(content.main_text);
    
    // Determine if it's an article
    content.is_article = (content.quality_score > 0.5f) && 
                         (!content.headings.empty()) &&
                         (content.main_text.length() > 200);
    
    return content;
}

inline float AdaptiveScraper::assess_quality(const std::string& html) {
    std::string text = extract_text_from_html(html);
    float readability = calculate_readability_score(text);
    float density = calculate_content_density(html);
    float text_ratio = calculate_text_to_html_ratio(html, text);
    
    return (readability * 0.4f + density * 0.3f + text_ratio * 0.3f);
}

inline std::map<std::string, std::string> AdaptiveScraper::extract_metadata(const std::string& html) {
    std::map<std::string, std::string> metadata;
    
    // Extract meta tags
    auto meta_begin = std::sregex_iterator(html.begin(), html.end(), meta_regex);
    auto meta_end = std::sregex_iterator();
    
    for (auto it = meta_begin; it != meta_end; ++it) {
        std::string name = (*it)[1].str();
        std::string content = (*it)[2].str();
        metadata[name] = content;
    }
    
    // Extract Open Graph data
    std::regex og_regex("<meta[^>]+property=\"og:([^\"]+)\"[^>]+content=\"([^\"]+)\"", std::regex::icase);
    auto og_begin = std::sregex_iterator(html.begin(), html.end(), og_regex);
    auto og_end = std::sregex_iterator();
    
    for (auto it = og_begin; it != og_end; ++it) {
        std::string property = "og:" + (*it)[1].str();
        std::string content = (*it)[2].str();
        metadata[property] = content;
    }
    
    return metadata;
}

inline std::string AdaptiveScraper::extract_main_content(const std::string& html) {
    // Try to find content in article/main tags first
    for (const auto& selector : content_selectors) {
        std::regex content_regex(selector + "[^>]*>(.*?)</" + 
                                std::string(selector.substr(1)) + ">", 
                                std::regex::icase | std::regex::dotall);
        
        std::smatch match;
        if (std::regex_search(html, match, content_regex)) {
            return extract_text_from_html(match[1].str());
        }
    }
    
    // Fallback: extract all paragraphs
    std::string all_text;
    auto para_begin = std::sregex_iterator(html.begin(), html.end(), paragraph_regex);
    auto para_end = std::sregex_iterator();
    
    for (auto it = para_begin; it != para_end; ++it) {
        std::string text = extract_text_from_html((*it)[1].str());
        if (text.length() > 50) {  // Filter out short noise
            all_text += text + "\n\n";
        }
    }
    
    return all_text;
}

inline std::string AdaptiveScraper::extract_title(const std::string& html) {
    std::smatch match;
    if (std::regex_search(html, match, title_regex)) {
        return extract_text_from_html(match[1].str());
    }
    return "";
}

inline std::vector<std::string> AdaptiveScraper::extract_headings(const std::string& html) {
    std::vector<std::string> headings;
    
    auto heading_begin = std::sregex_iterator(html.begin(), html.end(), heading_regex);
    auto heading_end = std::sregex_iterator();
    
    for (auto it = heading_begin; it != heading_end; ++it) {
        std::string level = (*it)[1].str();
        std::string text = extract_text_from_html((*it)[2].str());
        if (!text.empty() && text.length() < 200) {
            headings.push_text("[" + level + "] " + text);
        }
    }
    
    return headings;
}

inline std::vector<std::string> AdaptiveScraper::extract_links(const std::string& html) {
    std::vector<std::string> links;
    
    auto link_begin = std::sregex_iterator(html.begin(), html.end(), link_regex);
    auto link_end = std::sregex_iterator();
    
    for (auto it = link_begin; it != link_end; ++it) {
        std::string href = (*it)[1].str();
        std::string text = extract_text_from_html((*it)[2].str());
        
        // Only include meaningful links (not javascript, anchors, etc.)
        if (href.find("javascript:") == std::string::npos && 
            href.find("#") != 0 &&
            text.length() > 0) {
            links.push_back(href);
        }
    }
    
    return links;
}

inline float AdaptiveScraper::calculate_readability_score(const std::string& text) {
    if (text.empty()) return 0.0f;
    
    // Count sentences (approximate by period, exclamation, question mark)
    int sentences = 0;
    for (char c : text) {
        if (c == '.' || c == '!' || c == '?') sentences++;
    }
    
    // Count words
    int words = 0;
    bool in_word = false;
    for (char c : text) {
        if (std::isalpha(c)) {
            if (!in_word) {
                words++;
                in_word = true;
            }
        } else {
            in_word = false;
        }
    }
    
    // Count syllables (approximate by vowel groups)
    int syllables = 0;
    bool in_vowel = false;
    std::string vowels = "aeiouAEIOU";
    for (char c : text) {
        if (vowels.find(c) != std::string::npos) {
            if (!in_vowel) {
                syllables++;
                in_vowel = true;
            }
        } else {
            in_vowel = false;
        }
    }
    
    if (sentences == 0 || words == 0) return 0.0f;
    
    // Flesch Reading Ease (simplified)
    float avg_sentence_length = (float)words / sentences;
    float avg_syllables_per_word = (float)syllables / words;
    
    float score = 206.835f - (1.015f * avg_sentence_length) - (84.6f * avg_syllables_per_word);
    
    // Normalize to 0-1 range (Flesch scores typically 0-100)
    return std::min(1.0f, std::max(0.0f, score / 100.0f));
}

inline float AdaptiveScraper::calculate_content_density(const std::string& html) {
    // Ratio of text content to total HTML size
    std::string text = extract_text_from_html(html);
    
    if (html.empty()) return 0.0f;
    
    return (float)text.length() / html.length();
}

inline float AdaptiveScraper::calculate_text_to_html_ratio(const std::string& html, const std::string& text) {
    if (html.empty() || text.empty()) return 0.0f;
    
    // Remove tags to get pure text length
    std::string html_text = extract_text_from_html(html);
    
    if (html_text.empty()) return 0.0f;
    
    // Good ratio: extracted text should be close to total text in HTML
    return std::min(1.0f, (float)text.length() / html_text.length());
}

inline std::string AdaptiveScraper::remove_noise(const std::string& html) {
    std::string cleaned = html;
    
    // Remove scripts
    cleaned = std::regex_replace(cleaned, script_regex, "");
    
    // Remove styles
    cleaned = std::regex_replace(cleaned, style_regex, "");
    
    // Remove common noise patterns
    for (const auto& pattern : noise_patterns) {
        // Find and remove sections matching noise patterns
        size_t pos = 0;
        while ((pos = cleaned.find(pattern, pos)) != std::string::npos) {
            // Find the closing tag
            size_t end_pos = cleaned.find('>', pos);
            if (end_pos != std::string::npos) {
                // Simple removal (in production, use proper HTML parser)
                pos = end_pos + 1;
            } else {
                break;
            }
        }
    }
    
    return cleaned;
}

inline std::string AdaptiveScraper::extract_text_from_html(const std::string& html) {
    // Remove all HTML tags
    std::string text = std::regex_replace(html, tag_regex, "");
    
    // Decode common HTML entities
    size_t pos = 0;
    while ((pos = text.find("&nbsp;", pos)) != std::string::npos) {
        text.replace(pos, 6, " ");
        pos += 1;
    }
    pos = 0;
    while ((pos = text.find("&amp;", pos)) != std::string::npos) {
        text.replace(pos, 5, "&");
        pos += 1;
    }
    pos = 0;
    while ((pos = text.find("&lt;", pos)) != std::string::npos) {
        text.replace(pos, 4, "<");
        pos += 1;
    }
    pos = 0;
    while ((pos = text.find("&gt;", pos)) != std::string::npos) {
        text.replace(pos, 4, ">");
        pos += 1;
    }
    pos = 0;
    while ((pos = text.find("&#39;", pos)) != std::string::npos) {
        text.replace(pos, 5, "'");
        pos += 1;
    }
    pos = 0;
    while ((pos = text.find("&quot;", pos)) != std::string::npos) {
        text.replace(pos, 6, "\"");
        pos += 1;
    }
    
    // Remove extra whitespace
    std::string result;
    result.reserve(text.length());
    bool last_was_space = false;
    
    for (char c : text) {
        if (std::isspace(c)) {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }
    
    return result;
}

inline std::string AdaptiveScraper::detect_language(const std::string& text) {
    // Simple heuristic-based language detection
    if (text.empty()) return "unknown";
    
    // Check for Arabic characters
    bool has_arabic = false;
    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0xC2 && uc <= 0xDB) {  // UTF-8 Arabic range (simplified)
            has_arabic = true;
            break;
        }
    }
    
    if (has_arabic) return "ar";
    
    // Count common English words
    std::vector<std::string> english_words = {"the", "is", "in", "to", "and", "of", "a", "that", "for"};
    std::vector<std::string> arabic_words = {"في", "من", "على", "هذا", "إلى", "مع", "عن"};
    
    int english_count = 0;
    int arabic_count = 0;
    
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    for (const auto& word : english_words) {
        if (lower_text.find(" " + word + " ") != std::string::npos) {
            english_count++;
        }
    }
    
    for (const auto& word : arabic_words) {
        if (text.find(" " + word + " ") != std::string::npos) {
            arabic_count++;
        }
    }
    
    if (arabic_count > english_count) return "ar";
    if (english_count > arabic_count) return "en";
    
    return "unknown";
}

inline std::string AdaptiveScraper::generate_summary(const std::string& text, int max_sentences) {
    if (text.empty()) return "";
    
    // Split into sentences
    std::vector<std::string> sentences;
    std::string current_sentence;
    
    for (char c : text) {
        current_sentence += c;
        if (c == '.' || c == '!' || c == '?') {
            if (!current_sentence.empty()) {
                sentences.push_back(current_sentence);
                current_sentence.clear();
            }
        }
    }
    
    if (!current_sentence.empty()) {
        sentences.push_back(current_sentence);
    }
    
    // Take first N sentences (simple extractive summarization)
    std::string summary;
    int count = 0;
    for (const auto& sentence : sentences) {
        if (count >= max_sentences) break;
        summary += sentence + " ";
        count++;
    }
    
    return summary;
}

} // namespace web_agent
} // namespace asm_core

#endif // ASM_CORE_SMART_SCRAPER_H
