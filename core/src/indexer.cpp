#include "indexer.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

Indexer::Indexer() {
    loadStopWords();
}

// Fills stopWords set with standard English stop words
void Indexer::loadStopWords() {
    const std::vector<std::string> words = {
        "a", "about", "above", "after", "again", "against", "all", "am", "an", "and", "any", "are", 
        "as", "at", "be", "because", "been", "before", "being", "below", "between", "both", "but", 
        "by", "could", "did", "do", "does", "doing", "down", "during", "each", "few", "for", "from", 
        "further", "had", "has", "have", "having", "he", "her", "here", "hers", "him", "himself", 
        "his", "how", "i", "if", "in", "into", "is", "it", "its", "itself", "me", "more", "most", 
        "my", "myself", "no", "nor", "not", "of", "off", "on", "once", "only", "or", "other", "ought", 
        "our", "ours", "ourselves", "out", "over", "own", "same", "she", "should", "so", "some", "such", 
        "than", "that", "the", "their", "theirs", "them", "themselves", "then", "there", "these", 
        "they", "this", "those", "through", "to", "too", "under", "until", "up", "very", "was", "we", 
        "were", "what", "when", "where", "which", "while", "who", "whom", "why", "with", "would", 
        "you", "your", "yours", "yourself", "yourselves"
    };
    for (const auto& w : words) {
        stopWords.insert(w);
    }
}

// Tokenization, lowercasing, and stop-word filtering
std::vector<std::string> Indexer::preprocess(const std::string& text) {
    std::vector<std::string> tokens;
    std::string word = "";
    
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            word += std::tolower(static_cast<unsigned char>(ch));
        } else {
            if (!word.empty()) {
                if (stopWords.find(word) == stopWords.end()) {
                    tokens.push_back(word);
                }
                word = "";
            }
        }
    }
    if (!word.empty() && stopWords.find(word) == stopWords.end()) {
        tokens.push_back(word);
    }
    
    return tokens;
}

// Preprocessing that tracks word positions (necessary for exact phrase matching)
std::vector<std::pair<std::string, int>> Indexer::preprocessWithPositions(const std::string& text) {
    std::vector<std::pair<std::string, int>> tokens;
    std::string word = "";
    int position = 0;
    
    for (size_t i = 0; i < text.length(); ++i) {
        char ch = text[i];
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            word += std::tolower(static_cast<unsigned char>(ch));
        } else {
            if (!word.empty()) {
                if (stopWords.find(word) == stopWords.end()) {
                    tokens.push_back({word, position});
                }
                word = "";
                position++;
            }
        }
    }
    if (!word.empty() && stopWords.find(word) == stopWords.end()) {
        tokens.push_back({word, position});
    }
    
    return tokens;
}

// Adds document to the index structure
void Indexer::addDocument(int docId, const std::string& title, const std::string& path, const std::string& content) {
    Document doc;
    doc.id = docId;
    doc.title = title;
    doc.path = path;
    doc.content = content;
    
    auto tokens = preprocessWithPositions(content);
    doc.length = tokens.size();
    documents[docId] = doc;
    totalDocsCount++;

    // Track term frequencies locally for this document to avoid adding duplicate postings
    std::unordered_map<std::string, std::pair<int, std::vector<int>>> localFreq;
    for (const auto& tokenPair : tokens) {
        const std::string& term = tokenPair.first;
        int pos = tokenPair.second;
        
        localFreq[term].first++;
        localFreq[term].second.push_back(pos);
        
        // Add terms to autocompletion Trie
        autocompleteTrie.insert(term);
    }

    // Insert local frequency tallies into inverted index postings
    for (const auto& pair : localFreq) {
        const std::string& term = pair.first;
        int tf = pair.second.first;
        const auto& positions = pair.second.second;

        Posting posting;
        posting.docId = docId;
        posting.termFrequency = tf;
        posting.positions = positions;

        invertedIndex[term].push_back(posting);
    }

    calculateGlobalStats();
}

// Re-computes document statistics
void Indexer::calculateGlobalStats() {
    if (totalDocsCount == 0) {
        averageDocLength = 0;
        return;
    }
    
    long long totalLength = 0;
    for (const auto& pair : documents) {
        totalLength += pair.second.length;
    }
    averageDocLength = static_cast<double>(totalLength) / totalDocsCount;
}

void Indexer::clear() {
    invertedIndex.clear();
    documents.clear();
    totalDocsCount = 0;
    averageDocLength = 0.0;
    autocompleteTrie = Trie();
}
