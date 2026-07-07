#include "bm25.hpp"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <iostream>

SearchEngine::SearchEngine(Indexer& idx) : indexer(idx) {}

// IDF Calculation using standard BM25 variant
double SearchEngine::calculateIDF(int df) {
    int N = indexer.totalDocsCount;
    // Add 0.5 to avoid division by zero and log of negative numbers
    double idf = std::log(1.0 + (N - df + 0.5) / (df + 0.5));
    return idf > 0.0 ? idf : 0.0001; // Avoid negative IDFs for extremely common words
}

// Main query director
std::vector<SearchResult> SearchEngine::executeQuery(const std::string& query, const std::string& searchType) {
    if (query.empty() || indexer.totalDocsCount == 0) {
        return std::vector<SearchResult>();
    }

    if (searchType == "phrase") {
        return searchPhrase(query);
    } else if (searchType == "boolean") {
        return searchBoolean(query);
    } else {
        // Standard BM25 keyword search
        auto queryTerms = indexer.preprocess(query);
        return searchKeyword(queryTerms);
    }
}

// BM25 Keyword Search
std::vector<SearchResult> SearchEngine::searchKeyword(const std::vector<std::string>& queryTerms) {
    std::unordered_map<int, double> docScores;

    for (const auto& term : queryTerms) {
        if (indexer.invertedIndex.find(term) == indexer.invertedIndex.end()) {
            continue; // Term not in vocabulary
        }

        const auto& postings = indexer.invertedIndex[term];
        int df = postings.size();
        double idf = calculateIDF(df);

        for (const auto& posting : postings) {
            int docId = posting.docId;
            int tf = posting.termFrequency;
            int docLen = indexer.documents[docId].length;
            double avgLen = indexer.averageDocLength;

            // BM25 Formula component
            double numerator = tf * (k1 + 1.0);
            double denominator = tf + k1 * (1.0 - b + b * (static_cast<double>(docLen) / (avgLen > 0 ? avgLen : 1.0)));
            double score = idf * (numerator / denominator);

            docScores[docId] += score;
        }
    }

    // Convert docScores map to sorted results vector
    std::vector<SearchResult> results;
    for (const auto& pair : docScores) {
        int docId = pair.first;
        double score = pair.second;
        const auto& doc = indexer.documents[docId];

        SearchResult res;
        res.docId = docId;
        res.title = doc.title;
        res.path = doc.path;
        res.score = score;
        res.snippet = generateSnippet(doc.content, queryTerms);
        results.push_back(res);
    }

    // Sort descending by score
    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    return results;
}

// Exact Phrase Search (using position check)
std::vector<SearchResult> SearchEngine::searchPhrase(const std::string& phraseQuery) {
    // Process query terms (retain punctuation or keep order)
    auto queryTerms = indexer.preprocess(phraseQuery);
    if (queryTerms.empty()) return std::vector<SearchResult>();

    std::vector<SearchResult> results;
    std::unordered_map<int, std::vector<int>> matchingDocs; // Doc ID -> List of matching start positions

    // Start with postings of the first term
    std::string firstTerm = queryTerms[0];
    if (indexer.invertedIndex.find(firstTerm) == indexer.invertedIndex.end()) {
        return results;
    }

    // Initialize document candidates from the first term
    for (const auto& posting : indexer.invertedIndex[firstTerm]) {
        matchingDocs[posting.docId] = posting.positions;
    }

    // Refine candidate match positions using subsequent phrase terms
    for (size_t i = 1; i < queryTerms.size(); ++i) {
        std::string term = queryTerms[i];
        if (indexer.invertedIndex.find(term) == indexer.invertedIndex.end()) {
            return results; // If any phrase term is missing, no matches possible
        }

        const auto& postings = indexer.invertedIndex[term];
        std::unordered_map<int, std::vector<int>> nextMatchingDocs;

        for (const auto& posting : postings) {
            int docId = posting.docId;
            if (matchingDocs.find(docId) == matchingDocs.end()) continue;

            const auto& prevPositions = matchingDocs[docId];
            const auto& currPositions = posting.positions;

            // Check if currPosition == prevPosition + i
            std::vector<int> validStarts;
            for (int startPos : prevPositions) {
                // If the next word is exactly at startPos + i
                if (std::find(currPositions.begin(), currPositions.end(), startPos + static_cast<int>(i)) != currPositions.end()) {
                    validStarts.push_back(startPos);
                }
            }

            if (!validStarts.empty()) {
                nextMatchingDocs[docId] = validStarts;
            }
        }
        matchingDocs = nextMatchingDocs;
    }

    // Calculate score (BM25 score sum of words) for phrase matching documents
    std::unordered_map<int, double> docScores;
    for (const auto& pair : matchingDocs) {
        int docId = pair.first;
        double score = 0.0;
        
        for (const auto& term : queryTerms) {
            const auto& postings = indexer.invertedIndex[term];
            int df = postings.size();
            double idf = calculateIDF(df);

            for (const auto& p : postings) {
                if (p.docId == docId) {
                    int tf = p.termFrequency;
                    int docLen = indexer.documents[docId].length;
                    double avgLen = indexer.averageDocLength;

                    double numerator = tf * (k1 + 1.0);
                    double denominator = tf + k1 * (1.0 - b + b * (static_cast<double>(docLen) / (avgLen > 0 ? avgLen : 1.0)));
                    score += idf * (numerator / denominator);
                    break;
                }
            }
        }
        // Give phrase matches a 2.0x score boost for prominence
        docScores[docId] = score * 2.0;
    }

    // Assemble results
    for (const auto& pair : docScores) {
        int docId = pair.first;
        const auto& doc = indexer.documents[docId];

        SearchResult res;
        res.docId = docId;
        res.title = doc.title;
        res.path = doc.path;
        res.score = pair.second;
        res.snippet = generateSnippet(doc.content, queryTerms);
        results.push_back(res);
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    return results;
}

// Boolean logic search (AND, OR, NOT)
std::vector<SearchResult> SearchEngine::searchBoolean(const std::string& booleanQuery) {
    std::stringstream ss(booleanQuery);
    std::string token;
    
    std::vector<std::string> terms;
    std::vector<std::string> operators; // "AND", "OR", "NOT"

    while (ss >> token) {
        if (token == "AND" || token == "OR" || token == "NOT") {
            operators.push_back(token);
        } else {
            // Clean term
            auto cleanTerms = indexer.preprocess(token);
            if (!cleanTerms.empty()) {
                terms.push_back(cleanTerms[0]);
            }
        }
    }

    if (terms.empty()) return std::vector<SearchResult>();

    // Set of document IDs matching the boolean criteria
    std::unordered_set<int> resultSet;

    // Start with the first term's document matches
    std::string firstTerm = terms[0];
    if (indexer.invertedIndex.find(firstTerm) != indexer.invertedIndex.end()) {
        for (const auto& p : indexer.invertedIndex[firstTerm]) {
            resultSet.insert(p.docId);
        }
    }

    // Process operators sequentially
    size_t opIdx = 0;
    for (size_t i = 1; i < terms.size(); ++i) {
        std::string term = terms[i];
        std::string op = (opIdx < operators.size()) ? operators[opIdx++] : "AND";

        std::unordered_set<int> termDocs;
        if (indexer.invertedIndex.find(term) != indexer.invertedIndex.end()) {
            for (const auto& p : indexer.invertedIndex[term]) {
                termDocs.insert(p.docId);
            }
        }

        if (op == "AND") {
            std::unordered_set<int> intersect;
            for (int id : resultSet) {
                if (termDocs.find(id) != termDocs.end()) {
                    intersect.insert(id);
                }
            }
            resultSet = intersect;
        } else if (op == "OR") {
            for (int id : termDocs) {
                resultSet.insert(id);
            }
        } else if (op == "NOT") {
            for (int id : termDocs) {
                resultSet.erase(id);
            }
        }
    }

    // Compute BM25 scores for matching doc IDs
    std::vector<SearchResult> results;
    for (int docId : resultSet) {
        double score = 0.0;
        for (const auto& term : terms) {
            if (indexer.invertedIndex.find(term) == indexer.invertedIndex.end()) continue;
            
            const auto& postings = indexer.invertedIndex[term];
            for (const auto& p : postings) {
                if (p.docId == docId) {
                    int tf = p.termFrequency;
                    int docLen = indexer.documents[docId].length;
                    double avgLen = indexer.averageDocLength;

                    double numerator = tf * (k1 + 1.0);
                    double denominator = tf + k1 * (1.0 - b + b * (static_cast<double>(docLen) / (avgLen > 0 ? avgLen : 1.0)));
                    score += calculateIDF(postings.size()) * (numerator / denominator);
                    break;
                }
            }
        }

        const auto& doc = indexer.documents[docId];
        SearchResult res;
        res.docId = docId;
        res.title = doc.title;
        res.path = doc.path;
        res.score = score;
        res.snippet = generateSnippet(doc.content, terms);
        results.push_back(res);
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });

    return results;
}

// Snippet generation showing matching terms
std::string SearchEngine::generateSnippet(const std::string& content, const std::vector<std::string>& queryTerms) {
    if (content.empty()) return "";

    // Find the first query term match position in text
    size_t firstMatchPos = std::string::npos;
    std::string lowerContent = content;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    for (const auto& term : queryTerms) {
        size_t pos = lowerContent.find(term);
        if (pos != std::string::npos && (firstMatchPos == std::string::npos || pos < firstMatchPos)) {
            firstMatchPos = pos;
        }
    }

    // Default to start of content if no matches found
    if (firstMatchPos == std::string::npos) {
        firstMatchPos = 0;
    }

    // Extract window around match position
    int start = std::max(0, static_cast<int>(firstMatchPos) - 40);
    int length = 160;
    
    std::string snippet = content.substr(start, length);
    if (start > 0) snippet = "..." + snippet;
    if (start + length < static_cast<int>(content.length())) snippet = snippet + "...";
    
    return snippet;
}
