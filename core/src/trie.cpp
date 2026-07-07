#include "trie.hpp"
#include <iostream>

Trie::Trie() {
    root = std::make_shared<TrieNode>();
}

// Inserts a word into the Trie
void Trie::insert(const std::string& word) {
    if (word.empty()) return;
    
    auto current = root;
    for (char ch : word) {
        // Convert to lowercase for normalization
        char lowerCh = std::tolower(ch);
        if (current->children.find(lowerCh) == current->children.end()) {
            current->children[lowerCh] = std::make_shared<TrieNode>();
        }
        current = current->children[lowerCh];
    }
    current->isEndOfWord = true;
    current->word = word;
}

// DFS helper to collect up to 'limit' words matching the prefix
void Trie::collectWords(std::shared_ptr<TrieNode> node, std::vector<std::string>& results, int limit) {
    if (!node || results.size() >= static_cast<size_t>(limit)) return;
    
    if (node->isEndOfWord) {
        results.push_back(node->word);
    }
    
    // Recurse through all child nodes (alphabetically or order of map keys)
    for (const auto& pair : node->children) {
        collectWords(pair.second, results, limit);
        if (results.size() >= static_cast<size_t>(limit)) return;
    }
}

// Returns matching search queries for the typed prefix
std::vector<std::string> Trie::getSuggestions(const std::string& prefix, int limit) {
    std::vector<std::string> results;
    if (prefix.empty()) return results;
    
    auto current = root;
    for (char ch : prefix) {
        char lowerCh = std::tolower(ch);
        if (current->children.find(lowerCh) == current->children.end()) {
            return results; // No matches found
        }
        current = current->children[lowerCh];
    }
    
    collectWords(current, results, limit);
    return results;
}
