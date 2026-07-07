cmake_minimum_required(VERSION 3.10)
project(MiniSearchEngineCore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/trie.cpp
    src/indexer.cpp
    src/bm25.cpp
)

# Header files (for IDE visibility)
set(HEADERS
    src/trie.hpp
    src/indexer.hpp
    src/bm25.hpp
)

# Expose executable binary target
add_executable(search_core ${SOURCES} ${HEADERS})
