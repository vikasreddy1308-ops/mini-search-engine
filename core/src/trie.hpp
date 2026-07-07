#ifndef TRIE_HPP
#define TRIE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class TrieNode {
public:
    std::unordered_map<char, std::shared_ptr<TrieNode>> children;
    bool isEndOfWord = false;
    std::string word = "";
};

class Trie {
private:
    std::shared_ptr<TrieNode> root;
    void collectWords(std::shared_ptr<TrieNode> node, std::vector<std::string>& results, int limit);

public:
    Trie();
    void insert(const std::string& word);
    std::vector<std::string> getSuggestions(const std::string& prefix, int limit = 5);
};

#endif // TRIE_HPP
