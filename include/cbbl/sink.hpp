#pragma once
#include <string>
#include <memory>
#include <set>
#include <mutex>
#include <sqlite3.h>

namespace cbbl {
class Sink {
    public:
    virtual void writeTile(int res, int z, int x, int y, const std::string& buf) = 0;
    virtual ~Sink() {};
};

std::unique_ptr<Sink> CreateSink(const std::string &s);

class FileSink : public Sink {
    public:
    FileSink(const std::string &path);
    ~FileSink() {};
    void writeTile(int res, int z, int x, int y, const std::string& buf) override;

    private:
    std::string mOutput;
    std::set<std::string> mCreatedDirs;
    std::mutex mCreatedDirsMutex;
};

class MbtilesSink : public Sink {
    public:
    MbtilesSink(const std::string &path);
    ~MbtilesSink();
    void writeTile(int res, int z, int x, int y, const std::string& buf) override;

    private:
    std::string mOutput;
    sqlite3 * mDb;
    sqlite3_stmt * mStmt;
};

}