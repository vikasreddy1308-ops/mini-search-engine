#ifndef INDEXER_HPP
#define INDEXER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "trie.hpp"

struct Document {
    int id;
    std::string title;
    std::string path;
    std::string content;
    int length = 0;
};

struct Posting {
    int docId;
    int termFrequency = 0;
    std::vector<int> positions; // For phrase queries matching
};

class Indexer {
private:
    std::unordered_set<std::string> stopWords;
    void loadStopWords();

public:
    // Inverted Index: Term -> List of Posting entries
    std::unordered_map<std::string, std::vector<Posting>> invertedIndex;
    
    // Metadata: Document ID -> Document Details
    std::unordered_map<int, Document> documents;
    
    // Total document count
    int totalDocsCount = 0;
    
    // Average document length across index (for BM25)
    double averageDocLength = 0.0;
    
    // Autocomplete Trie connection
    Trie autocompleteTrie;

    Indexer();
    
    // Preprocesses a text string (tokenization, lowercasing, stop-word removal)
    std::vector<std::string> preprocess(const std::string& text);
    
    // Preprocesses keeping positions (useful for phrase matches)
    std::vector<std::pair<std::string, int>> preprocessWithPositions(const std::string& text);
    
    // Adds a single document to the inverted index
    void addDocument(int docId, const std::string& title, const std::string& path, const std::string& content);
    
    // Loads and indexes all documents from a folder list
    void indexFolder(const std::string& folderPath);
    
    // Re-calculates global index stats (avg doc length)
    void calculateGlobalStats();
    
    // Empties current index
    void clear();
};

#endif // INDEXER_HPP
