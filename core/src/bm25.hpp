#ifndef BM25_HPP
#define BM25_HPP

#include "indexer.hpp"
#include <string>
#include <vector>

struct SearchResult {
    int docId;
    std::string title;
    std::string path;
    double score = 0.0;
    std::string snippet = "";
};

class SearchEngine {
private:
    Indexer& indexer;
    // BM25 Hyperparameters
    const double k1 = 1.2;
    const double b = 0.75;

    // Computes Inverse Document Frequency (IDF)
    double calculateIDF(int df);
    
    // Generates a dynamic search snippet showing matching terms
    std::string generateSnippet(const std::string& content, const std::vector<std::string>& queryTerms);

    // Sub-search methods
    std::vector<SearchResult> searchKeyword(const std::vector<std::string>& queryTerms);
    std::vector<SearchResult> searchPhrase(const std::string& phraseQuery);
    std::vector<SearchResult> searchBoolean(const std::string& booleanQuery);

public:
    SearchEngine(Indexer& idx);
    
    // Main search interface
    std::vector<SearchResult> executeQuery(const std::string& query, const std::string& searchType = "keyword");
};

#endif // BM25_HPP
