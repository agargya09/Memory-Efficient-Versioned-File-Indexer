// CS253 Assignment 1
// Memory-Efficient Versioned File Indexer
// 240234_Avtansh.cpp

#include <bits/stdc++.h>
using namespace std;

// A generic map that tracks how many times each key has been seen.
// Templated so it works for any key/value type combination.
template <typename K, typename V>
class FrequencyMap {
private:
    unordered_map<K, V> data_;

public:
    void increment(const K& key, V amount = static_cast<V>(1)) {
        data_[key] += amount;
    }

    V get(const K& key) const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : V{};
    }

    const unordered_map<K, V>& getAll() const { return data_; }
    size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }
};

// Opens a file and reads it in fixed-size chunks.
// The whole file is never in memory at once — only one buffer's worth at a time.
class BufferedFileReader {
private:
    ifstream file_;
    char*    buffer_;
    size_t   bufferSize_;
    size_t   bytesRead_;
    bool     eof_;
    string   filename_;

public:
    BufferedFileReader(const string& filename, size_t bufferSizeKB)
        : bufferSize_(bufferSizeKB * 1024), bytesRead_(0), eof_(false),
          filename_(filename)
    {
        if (bufferSizeKB < 256 || bufferSizeKB > 1024) {
            throw invalid_argument(
                "Buffer size must be between 256 and 1024 KB. Got: "
                + to_string(bufferSizeKB) + " KB");
        }

        buffer_ = new char[bufferSize_];

        file_.open(filename_, ios::binary);
        if (!file_.is_open()) {
            delete[] buffer_;
            throw runtime_error("Cannot open file: " + filename_);
        }
    }

    ~BufferedFileReader() {
        delete[] buffer_;
        if (file_.is_open()) file_.close();
    }

    BufferedFileReader(const BufferedFileReader&)            = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    bool fillBuffer() {
        if (eof_) return false;
        file_.read(buffer_, static_cast<streamsize>(bufferSize_));
        bytesRead_ = static_cast<size_t>(file_.gcount());
        if (file_.eof()) eof_ = true;
        return bytesRead_ > 0;
    }

    const char*   buffer()     const { return buffer_;     }
    size_t        bytesRead()  const { return bytesRead_;  }
    bool          atEOF()      const { return eof_;        }
    size_t        bufferSize() const { return bufferSize_; }
    const string& filename()   const { return filename_;   }
};

// Pulls out lowercase alphanumeric words from a character stream.
// The leftover_ string handles the case where a word gets cut off
// at the end of one buffer and continues in the next.
class Tokenizer {
private:
    string leftover_;

    static bool isWordChar(unsigned char c) { return isalnum(c) != 0; }
    static char toLow(unsigned char c)      { return static_cast<char>(tolower(c)); }

public:
    Tokenizer() = default;
    void reset() { leftover_.clear(); }

    // Works on a raw buffer — calls callback for each complete word found
    template <typename Callback>
    void tokenize(const char* buf, size_t len, Callback callback) {
        string current = move(leftover_);
        leftover_.clear();

        for (size_t i = 0; i < len; ++i) {
            unsigned char ch = static_cast<unsigned char>(buf[i]);
            if (isWordChar(ch)) {
                current += toLow(ch);
            } else {
                if (!current.empty()) {
                    callback(current);
                    current.clear();
                }
            }
        }
        leftover_ = move(current);
    }

    // Works on a plain string — returns a vector of tokens.
    // Used to normalise the word passed in on the command line.
    vector<string> tokenize(const string& text) {
        vector<string> tokens;
        string cur;
        for (unsigned char ch : text) {
            if (isWordChar(ch)) {
                cur += toLow(ch);
            } else if (!cur.empty()) {
                tokens.push_back(move(cur));
                cur.clear();
            }
        }
        if (!cur.empty()) tokens.push_back(move(cur));
        return tokens;
    }

    // Flush whatever partial word is left after the last buffer read
    template <typename Callback>
    void flush(Callback callback) {
        if (!leftover_.empty()) {
            callback(leftover_);
            leftover_.clear();
        }
    }
};

// Holds one FrequencyMap per named version and drives the
// read -> tokenise -> count pipeline for each file.
class VersionedIndex {
private:
    unordered_map<string, FrequencyMap<string, long long>> versions_;

public:
    void addVersion(const string& versionName,
                    const string& filepath,
                    size_t bufferSizeKB)
    {
        if (versions_.count(versionName))
            throw runtime_error("Version already exists: " + versionName);

        BufferedFileReader reader(filepath, bufferSizeKB);
        Tokenizer          tokenizer;
        auto&              freq = versions_[versionName];

        auto addWord = [&](const string& w) { freq.increment(w); };

        while (reader.fillBuffer())
            tokenizer.tokenize(reader.buffer(), reader.bytesRead(), addWord);
        tokenizer.flush(addWord);
    }

    bool hasVersion(const string& name) const {
        return versions_.count(name) > 0;
    }

    const FrequencyMap<string, long long>& getIndex(const string& name) const {
        auto it = versions_.find(name);
        if (it == versions_.end())
            throw runtime_error("Version not found: " + name);
        return it->second;
    }

    long long getWordCount(const string& version, const string& word) const {
        return getIndex(version).get(word);
    }

    vector<pair<string, long long>> getTopK(const string& version, int k) const {
        const auto& map = getIndex(version).getAll();
        vector<pair<string, long long>> entries(map.begin(), map.end());

        sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
                return (a.second != b.second) ? (a.second > b.second)
                                              : (a.first < b.first);
            });

        if (k > static_cast<int>(entries.size()))
            k = static_cast<int>(entries.size());

        return {entries.begin(), entries.begin() + k};
    }
};

// Base class for all query types.
// Each derived class just implements execute() differently.
class Query {
public:
    virtual ~Query() = default;
    virtual void   execute(const VersionedIndex& index) const = 0;
    virtual string typeName() const = 0;
};

class WordQuery : public Query {
private:
    string version_;
    string word_;

public:
    WordQuery(const string& version, const string& word)
        : version_(version), word_(word) {}

    void execute(const VersionedIndex& index) const override {
        long long count = index.getWordCount(version_, word_);
        cout << "+-------------------------------------+\n"
             << "|           WORD QUERY RESULT         |\n"
             << "+-------------------------------------+\n"
             << "  Version : " << version_ << "\n"
             << "  Word    : " << word_    << "\n"
             << "  Count   : " << count    << "\n";
    }

    string typeName() const override { return "word"; }
};

class TopKQuery : public Query {
private:
    string version_;
    int    k_;

public:
    TopKQuery(const string& version, int k) : version_(version), k_(k) {}

    void execute(const VersionedIndex& index) const override {
        auto results = index.getTopK(version_, k_);
        cout << "+-------------------------------------+\n"
             << "|          TOP-K QUERY RESULT         |\n"
             << "+-------------------------------------+\n"
             << "  Version : " << version_ << "\n"
             << "  K       : " << k_       << "\n\n";

        cout << left
             << setw(6)  << "Rank"
             << setw(30) << "Word"
             << "Count\n"
             << string(50, '-') << "\n";

        int rank = 1;
        for (const auto& [word, cnt] : results) {
            cout << left
                 << setw(6)  << rank++
                 << setw(30) << word
                 << cnt << "\n";
        }
    }

    string typeName() const override { return "top"; }
};

class DiffQuery : public Query {
private:
    string version1_;
    string version2_;
    string word_;

public:
    DiffQuery(const string& v1, const string& v2, const string& word)
        : version1_(v1), version2_(v2), word_(word) {}

    void execute(const VersionedIndex& index) const override {
        long long c1   = index.getWordCount(version1_, word_);
        long long c2   = index.getWordCount(version2_, word_);
        long long diff = c2 - c1;

        cout << "+-------------------------------------+\n"
             << "|           DIFF QUERY RESULT         |\n"
             << "+-------------------------------------+\n"
             << "  Version 1  : " << version1_ << "\n"
             << "  Version 2  : " << version2_ << "\n"
             << "  Word       : " << word_     << "\n"
             << "  Count (v1) : " << c1        << "\n"
             << "  Count (v2) : " << c2        << "\n"
             << "  Difference : " << (diff >= 0 ? "+" : "") << diff << "\n";
    }

    string typeName() const override { return "diff"; }
};

// Runs a query against the index. Overloaded to accept either
// a reference or a pointer to a Query object.
class QueryProcessor {
private:
    const VersionedIndex& index_;

public:
    explicit QueryProcessor(const VersionedIndex& index) : index_(index) {}

    void process(const Query& query) const {
        query.execute(index_);
    }

    void process(const Query* query) const {
        if (!query)
            throw invalid_argument("Null query pointer passed to processor");
        query->execute(index_);
    }
};

struct Args {
    string file, file1, file2;
    string version, version1, version2;
    size_t bufferKB = 512;
    string query;
    string word;
    int    top = 10;
};

static Args parseArgs(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if      (a == "--file"     && i+1 < argc) args.file     = argv[++i];
        else if (a == "--file1"    && i+1 < argc) args.file1    = argv[++i];
        else if (a == "--file2"    && i+1 < argc) args.file2    = argv[++i];
        else if (a == "--version"  && i+1 < argc) args.version  = argv[++i];
        else if (a == "--version1" && i+1 < argc) args.version1 = argv[++i];
        else if (a == "--version2" && i+1 < argc) args.version2 = argv[++i];
        else if (a == "--buffer"   && i+1 < argc) {
            long raw = stol(argv[++i]);
            if (raw < 256 || raw > 1024)
                throw invalid_argument("--buffer must be between 256 and 1024 KB");
            args.bufferKB = static_cast<size_t>(raw);
        }
        else if (a == "--query" && i+1 < argc) args.query = argv[++i];
        else if (a == "--word"  && i+1 < argc) args.word  = argv[++i];
        else if (a == "--top"   && i+1 < argc) {
            args.top = stoi(argv[++i]);
            if (args.top <= 0)
                throw invalid_argument("--top must be a positive integer");
        }
        else throw invalid_argument("Unknown argument: " + a);
    }
    return args;
}

static string lower(const string& s) {
    string out = s;
    for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return out;
}

int main(int argc, char* argv[]) {
    using Clock = chrono::high_resolution_clock;
    auto t0 = Clock::now();

    try {
        if (argc < 2) {
            cerr << "Usage:\n"
                 << "  ./analyzer --file <file> --version <name> --buffer <kb> --query word --word <w>\n"
                 << "  ./analyzer --file <file> --version <name> --buffer <kb> --query top  --top <k>\n"
                 << "  ./analyzer --file1 <f1> --version1 <n1> --file2 <f2> --version2 <n2> "
                    "--buffer <kb> --query diff --word <w>\n";
            return 1;
        }

        Args args = parseArgs(argc, argv);

        if (args.query.empty())
            throw invalid_argument("--query is required (word | top | diff)");

        cout << "\n==========================================\n"
             << "  Memory-Efficient Versioned File Indexer\n"
             << "==========================================\n"
             << "  Buffer size : " << args.bufferKB << " KB\n"
             << "  Query type  : " << args.query    << "\n\n";

        VersionedIndex index;
        QueryProcessor processor(index);

        if (args.query == "diff") {
            if (args.file1.empty() || args.version1.empty() ||
                args.file2.empty() || args.version2.empty())
                throw invalid_argument(
                    "diff query requires --file1 --version1 --file2 --version2");
            if (args.word.empty())
                throw invalid_argument("diff query requires --word");

            cout << "  Indexing \"" << args.file1 << "\" -> version '" << args.version1 << "' ...\n";
            index.addVersion(args.version1, args.file1, args.bufferKB);
            cout << "  Unique words: " << index.getIndex(args.version1).size() << "\n";

            cout << "  Indexing \"" << args.file2 << "\" -> version '" << args.version2 << "' ...\n";
            index.addVersion(args.version2, args.file2, args.bufferKB);
            cout << "  Unique words: " << index.getIndex(args.version2).size() << "\n\n";

            DiffQuery q(args.version1, args.version2, lower(args.word));
            processor.process(q);

        } else {
            if (args.file.empty() || args.version.empty())
                throw invalid_argument("This query requires --file and --version");

            cout << "  Indexing \"" << args.file << "\" -> version '" << args.version << "' ...\n";
            index.addVersion(args.version, args.file, args.bufferKB);
            cout << "  Unique words: " << index.getIndex(args.version).size() << "\n\n";

            if (args.query == "word") {
                if (args.word.empty())
                    throw invalid_argument("word query requires --word");
                WordQuery q(args.version, lower(args.word));
                processor.process(&q);

            } else if (args.query == "top") {
                TopKQuery q(args.version, args.top);
                processor.process(q);

            } else {
                throw invalid_argument(
                    "Unknown query type: \"" + args.query + "\". Use word | top | diff");
            }
        }

    } catch (const invalid_argument& ex) {
        cerr << "\n[Error] " << ex.what() << "\n";
        return 2;
    } catch (const runtime_error& ex) {
        cerr << "\n[Error] " << ex.what() << "\n";
        return 3;
    } catch (const exception& ex) {
        cerr << "\n[Error] " << ex.what() << "\n";
        return 1;
    }

    auto t1 = Clock::now();
    double secs = chrono::duration<double>(t1 - t0).count();

    cout << "\n==========================================\n"
         << "  Execution time : "
         << fixed << setprecision(6) << secs << " seconds\n"
         << "==========================================\n\n";

    return 0;
}
