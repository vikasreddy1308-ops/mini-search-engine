# High-Performance Mini Search Engine

A full-stack, high-performance information retrieval system featuring a native C++17 indexing core, a TypeScript/Node.js REST API layer, and an interactive React web frontend.

## Architecture & Features

The project is structured into three main modules:
*   **Core (C++17)**: The indexing and query evaluation engine.
*   **Backend (Node.js/Express/TypeScript)**: The API gateway bridging the web frontend and C++ core.
*   **Frontend (React/TypeScript)**: An intuitive, responsive search interface.

### Technical Highlights
1.  **Inverted Index**: Native C++ core mapping tokens to posting lists for sub-millisecond retrieval.
2.  **BM25 Relevance Ranking**: Implements the industry-standard Okapi BM25 scoring algorithm to rank document matches by relevance.
3.  **Trie-based Autocomplete**: A Prefix Trie data structure in the C++ core for fast prefix-matching and search suggestions.
4.  **Boolean Query Parser**: Supports complex query evaluation (`AND`, `OR`, `NOT`) using optimized sorted-list intersection algorithms.
5.  **Index Persistence**: Persistent binary serialization using raw byte streams to write and load the index, bypassing redundant file indexing on startup.
6.  **Recursive File Indexer**: Dynamically traverses local directories, tokenizing and parsing documents.

---

## Folder Structure

```
mini-search-engine/
├── core/             # C++17 Search Engine Core
│   ├── src/          # Inverted Index, BM25, Trie, and Main logic
│   └── CMakeLists.txt
├── backend/          # Node.js Express API (TypeScript)
│   └── src/          # Express router and C++ bindings/execution
├── frontend/         # React User Interface (TypeScript)
└── .gitignore
```

---

## Setup & Running Locally

### Prerequisites
*   **C++17 Compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
*   **CMake** 3.15+
*   **Node.js** v16+ & **npm**

### 1. Build the C++ Core
Navigate to the `core/` directory and build the executable:
```bash
cd core
mkdir build && cd build
cmake ..
cmake --build .
```
This compiles the executable which parses files and accepts command-line queries or acts as the subprocess backend.

### 2. Start the Backend API
Navigate to the `backend/` directory, install dependencies, and start the server:
```bash
cd ../../backend
npm install
npm run dev
```
The server will run on `http://localhost:5000` (or specified PORT) and communicates search queries to the C++ core.

### 3. Start the Frontend App
Navigate to the `frontend/` directory, install dependencies, and start the development server:
```bash
cd ../frontend
npm install
npm run dev
```
Open `http://localhost:5173` in your browser to interact with the search UI.

---

## Technologies Used
*   **Core**: C++17, Standard Template Library (STL), CMake
*   **Backend**: Node.js, Express.js, TypeScript
*   **Frontend**: React, TypeScript, HTML5, CSS3 / Tailwind CSS
*   **Developer Tools**: Git, GitHub, CMake, GCC/Clang
