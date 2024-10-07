#include "duel.h"
#include "base.h"
#include "lib/json.hpp"
#include <cstdint>
#include <string>
#include <tgbot/net/HttpClient.h>
#include <tgbot/net/Url.h>
#include <tgbot/types/Update.h>

using duel::Duel;

duel::Problem::Problem() {}

bool duel::Problem::operator==(Problem problem)
{
    return problem.contest == this -> contest
        && problem.index   == this -> index;
}

duel::User::User() {}

bool duel::User::operator<(const User user) const
{
    return this -> tgID < user.tgID;
}

bool duel::User::operator==(const User user) const
{
    return this -> tgID == user.tgID;
}

duel::Status::Status() {}

duel::Status::Status(DuelStatus duelStatus)
{
    this -> duelStatus = duelStatus;
}

base::Result<duel::Status> duel::Pool::getStatus(std::int64_t tgID)
{
    if (!mp.count(User(tgID,"")))
    {
        return base::Result<duel::Status> (false, Status(FREE));
    }
    return base::Result<duel::Status> (true, mp[User(tgID, "")]);
}

base::Result<duel::Status> duel::Pool::getStatus(User user)
{
    if (!mp.count(user))
    {
        return base::Result<duel::Status> (false, Status(FREE));
    }
    return base::Result<duel::Status> (true, mp[user]);
}

void duel::Pool::deleteUser(User user)
{
    if (mp.count(user))
    {
        mp.erase(user);
    }
}

void duel::Pool::updateUser(User user, Status status)
{
    mp[user] = status;
}

duel::ProblemSet::ProblemSet()
{
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        std::cerr <<  "Unable to open problemset data base" << std::endl;
        exit(0);
    }
}

duel::ProblemSet::ProblemSet(std::string str)
{
    dbPath = str;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        std::cerr <<  "Unable to open problemset data base" << std::endl;
        exit(0);
    }
}

duel::ProblemSet::~ProblemSet()
{
    sqlite3_close(db);
}

base::Result<duel::Problem> duel::ProblemSet::getRandomProblem(int64_t difficulty)
{
    std::clog << "Getting problem with difficulty " << difficulty << std::endl;
    std::string table = std::string("RATE") + std::to_string(difficulty);
    std::string command = std::string("SELECT * FROM ") + table
                                    + " LIMIT 1 OFFSET ABS(RANDOM()) % ("
                                    + "SELECT COUNT(*) FROM " + table + ");";
    base::Result<duel::Problem> res(false, Problem());
    if (   sqlite3_prepare_v2(db, command.c_str(), -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW)
    {
        res.type = true;
        res.result.contest = sqlite3_column_int(stmt, 0);
        res.result.index   = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }
    else
    {
        std::clog << "Can not find problem with difficulty "
                  << difficulty
                  << std::endl;
    }
    sqlite3_finalize(stmt);
    return res;
}

Duel::Duel()
{
    if (sqlite3_open_v2("./id.db", &cfID, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
        std::cerr << "Can not open codeforces user ID database: "
                  << sqlite3_errmsg(cfID)
                  << std::endl;
        exit(0);
    }
}

Duel::~Duel()
{
    sqlite3_close(cfID);
}

void Duel::bind(std::int64_t tgID, std::string cfName)
{
    std::clog << "Binding " << tgID << " to " << cfName << std::endl;
    std::string command = std::string("INSERT INTO ID (TGID, CFNAME) ")
                                    + "VALUES ("
                                    + std::to_string(tgID)
                                    + ", \""
                                    + cfName
                                    + "\");";
    if (sqlite3_prepare_v2(cfID, command.c_str(), -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_step(stmt);
    }
    else
    {
        std::cerr << "Can not bind codeforces user ID: "
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Duel::unbind(std::int64_t tgID)
{
    std::clog << "Unbinding " << tgID << std::endl;
    std::string command = std::string("DELETE FROM ID WHERE TGID=")
                                    + std::to_string(tgID)
                                    + ";";
    if (sqlite3_prepare_v2(cfID, command.c_str(), -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_step(stmt);
    }
    else
    {
        std::cerr << "Can not unbind codeforces user ID"
                  << std::endl;
    }
    sqlite3_finalize(stmt);
}

base::Result<std::string> Duel::getCFName(std::int64_t tgID)
{
    std::string command = "SELECT CFNAME FROM ID WHERE TGID =" + std::to_string(tgID) + ";";
    base::Result<std::string> res(false, std::string(""));
    if (   sqlite3_prepare_v2(cfID, command.c_str(), -1, &stmt, NULL) == SQLITE_OK
        && sqlite3_step(stmt) == SQLITE_ROW)
    {
        res.type = true;
        res.result = reinterpret_cast<const char * >(sqlite3_column_text(stmt, 0));
    }
    else
    {
        std::clog << "Can not find codeforces user ID of telegram user "
                  << tgID
                  << std::endl;
    }
    sqlite3_finalize(stmt);
    return res;
}

bool Duel::isFree(std::int64_t tgID)
{
    auto res = pool.getStatus(tgID);
    return res.type == false || res.result.duelStatus == FREE;
}

bool Duel::isFree(User user)
{
    auto res = pool.getStatus(user);
    return res.type == false || res.result.duelStatus == FREE;
}

void Duel::invite(User from, User to, Problem problem)
{
    std::clog << "Starting duel between " << from.cfName << " to " << to.cfName << std::endl;
    pool.updateUser(from, Status(INVITING, to, problem));
    pool.updateUser(to,   Status(INVITED, from, problem));
}

void Duel::accept(User user)
{
    auto res = pool.getStatus(user);
    if (!res.type || res.result.duelStatus != INVITED)
    {
        std::clog << "User " << user.tgID << " has not been invited!" << std::endl;
        return ;
    }
    res.result.duelStatus = INDUEL;
    auto other = res.result;
    other.otherSide = user;
    pool.updateUser(user, res.result);
    pool.updateUser(res.result.otherSide, other);
}

void Duel::reject(User user)
{
    auto res = pool.getStatus(user);
    if (!res.type || res.result.duelStatus != INVITED)
    {
        std::clog << "User " << user.tgID << " has not been invited!" << std::endl;
        return ;
    }
    pool.updateUser(user, Status(FREE));
    pool.updateUser(res.result.otherSide, Status(FREE));
}

void Duel::endDuel(User user)
{
    auto res = pool.getStatus(user);
    if (!res.type || res.result.duelStatus == FREE)
    {
        return ;
    }
    pool.updateUser(user, Status(FREE));
    pool.updateUser(res.result.otherSide, Status(FREE));
}

base::Result<duel::Problem> Duel::getRandomProblem(std::int64_t difficulty)
{
    return problemSet.getRandomProblem(difficulty);
}

bool Duel::checkCompleteness(std::string cfName, Problem problem)
{
    std::clog << "Checking " << cfName << " " << problem.contest << problem.index << std::endl;
    TgBot::CurlHttpClient client;
    const TgBot::Url url("https://codeforces.com/api/user.status?handle=AtomAlpaca&from=1&count=1");
    std::string res = client.makeRequest(url, std::vector<TgBot::HttpReqArg>());
    std::clog << res << std::endl << url.protocol << url.host << url.path << url.query;
    nlohmann::json j = nlohmann::json::parse(res);
    if (j["status"] == "OK" &&
        j["result"][0]["problem"]["contestId"] == problem.contest &&
        j["result"][0]["problem"]["index"]     == problem.index &&
        j["result"][0]["verdict"] == "OK")
    {
        return true;
    }
    else
    {
        return false;
    }
}
