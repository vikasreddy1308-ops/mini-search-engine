#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "indexer.hpp"
#include "bm25.hpp"

// Utility to parse string values from a simple flat JSON line
std::string getJSONString(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";

    size_t quoteStart = json.find("\"", colonPos);
    if (quoteStart == std::string::npos) return "";

    // Parse string content considering escaped backslash quotes
    std::string value = "";
    size_t i = quoteStart + 1;
    while (i < json.length()) {
        if (json[i] == '\\' && i + 1 < json.length()) {
            char next = json[i+1];
            if (next == 'n') value += '\n';
            else if (next == 't') value += '\t';
            else if (next == '"') value += '"';
            else if (next == '\\') value += '\\';
            else value += next;
            i += 2;
        } else if (json[i] == '"') {
            break; // Closing quote
        } else {
            value += json[i];
            i++;
        }
    }
    return value;
}

// Utility to parse numeric values from a simple flat JSON line
int getJSONInt(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return 0;

    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return 0;

    size_t valStart = json.find_first_of("-0123456789", colonPos);
    if (valStart == std::string::npos) return 0;

    size_t valEnd = json.find_first_not_of("0123456789", valStart);
    std::string valStr = json.substr(valStart, valEnd - valStart);
    try {
        return std::stoi(valStr);
    } catch (...) {
        return 0;
    }
}

// Utility to escape quotes and newlines when producing C++ JSON outputs
std::string escapeJSONString(const std::string& input) {
    std::string output = "";
    for (char c : input) {
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\t') output += "\\t";
        else if (c == '\r') output += "\\r";
        else output += c;
    }
    return output;
}

int main() {
    // Disable standard C++ sync buffers for speed
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    Indexer indexer;
    SearchEngine searchEngine(indexer);

    std::string line;
    // Process input lines from Node.js Express process
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::string command = getJSONString(line, "command");

        if (command == "add_doc") {
            int id = getJSONInt(line, "id");
            std::string title = getJSONString(line, "title");
            std::string path = getJSONString(line, "path");
            std::string content = getJSONString(line, "content");

            indexer.addDocument(id, title, path, content);

            // Respond success
            std::cout << "{\"success\":true,\"docId\":" << id << "}\n" << std::flush;
        } 
        else if (command == "search") {
            std::string query = getJSONString(line, "query");
            std::string type = getJSONString(line, "type"); // "keyword", "phrase", "boolean"

            auto hits = searchEngine.executeQuery(query, type);

            // Construct JSON array response manually
            std::cout << "{\"results\":[";
            for (size_t i = 0; i < hits.size(); ++i) {
                std::cout << "{"
                          << "\"docId\":" << hits[i].docId << ","
                          << "\"title\":\"" << escapeJSONString(hits[i].title) << "\","
                          << "\"path\":\"" << escapeJSONString(hits[i].path) << "\","
                          << "\"score\":" << hits[i].score << ","
                          << "\"snippet\":\"" << escapeJSONString(hits[i].snippet) << "\""
                          << "}";
                if (i + 1 < hits.size()) std::cout << ",";
            }
            std::cout << "],\"count\":" << hits.size() << "}\n" << std::flush;
        } 
        else if (command == "autocomplete") {
            std::string prefix = getJSONString(line, "prefix");
            int limit = getJSONInt(line, "limit");
            if (limit <= 0) limit = 5;

            auto suggestions = indexer.autocompleteTrie.getSuggestions(prefix, limit);

            // Respond list of strings
            std::cout << "{\"suggestions\":[";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                std::cout << "\"" << escapeJSONString(suggestions[i]) << "\"";
                if (i + 1 < suggestions.size()) std::cout << ",";
            }
            std::cout << "]}\n" << std::flush;
        }
        else if (command == "clear") {
            indexer.clear();
            std::cout << "{\"success\":true}\n" << std::flush;
        }
        else {
            std::cout << "{\"error\":\"Unknown command: " << escapeJSONString(command) << "\"}\n" << std::flush;
        }
    }

    return 0;
}
