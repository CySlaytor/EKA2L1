#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>

class NgageCoverageLogger {
private:
    inline static std::mutex mtx;
    inline static std::ofstream out;
    inline static bool initialized = false;
    inline static std::unordered_set<std::string> seen;

public:
    static void Log(const char *file, const char *func) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!initialized) {
            // 1. Read existing log to populate 'seen' set to prevent duplicates across runs
            std::ifstream in("ngage_coverage_log.txt");
            if (in.is_open()) {
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty()) {
                        if (line.back() == '\\r') line.pop_back(); // Handle Windows CRLF
                        seen.insert(line);
                    }
                }
                in.close();
            }

            // 2. Open file in APPEND mode so we don't wipe previous sessions
            out.open("ngage_coverage_log.txt", std::ios::out | std::ios::app);
            initialized = true;
        }

        std::string f(file);
        size_t pos = f.find_last_of("/\\\\");
        if (pos != std::string::npos)
            f = f.substr(pos + 1);
        std::string entry = f + " | " + std::string(func);

        // 3. Only write to the file if we haven't seen this function yet
        if (seen.find(entry) == seen.end()) {
            seen.insert(entry);
            if (out.is_open()) {
                out << entry << "\n"; // Fixed newline bug
                out.flush();
            }
        }
    }
};

#define NGAGE_COVERAGE_LOG() NgageCoverageLogger::Log(__FILE__, __func__)