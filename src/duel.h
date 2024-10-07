#pragma once

#include "base.h"
#include <boost/asio/thread_pool.hpp>
#include <tgbot/net/CurlHttpClient.h>
#include <cstdint>
#include <map>
#include <sqlite3.h>
#include <iostream>
#include <tgbot/types/User.h>
#include "lib/json.hpp"

namespace duel
{
    struct Problem
    {
        std::int64_t contest;
        std::string index;
        bool operator==(Problem problem);
        Problem();
        Problem(std::int64_t contest, std::string index) : contest(contest), index(index) {};
    };

    struct User
    {
        std::int64_t tgID;
        std::string  cfName;
        bool operator< (const User user) const ;
        bool operator==(const User user) const ;
        User();
        User(int64_t tgID, std::string cfName) : tgID(tgID), cfName(cfName) {};
    };

    enum DuelStatus
    {
        FREE,
        INVITING,
        INVITED,
        INDUEL
    };

    struct Status
    {
        DuelStatus duelStatus;
        User otherSide;
        Problem problem;
        Status();
        Status(DuelStatus duelStatus);
        Status(DuelStatus duelStatus, User otherSide, Problem problem) : duelStatus(duelStatus), otherSide(otherSide), problem(problem) {};
    };

    class Pool
    {
        private:
        std::map <User, Status> mp;
        public:
        void deleteUser(User user);
        void updateUser(User user, Status status);
        base::Result<Status> getStatus(std::int64_t tgID);
        base::Result<Status> getStatus(User user);
    };

    class ProblemSet
    {
        private:
        std::string dbPath = "./problem.db";
        sqlite3 * db = nullptr;
        sqlite3_stmt * stmt = nullptr;
        public:
        ProblemSet();
        ProblemSet(std::string);
        ~ProblemSet();
        base::Result<Problem> getRandomProblem(std::int64_t difficulty);
    };

    class Duel
    {
        private:
        sqlite3 * cfID = nullptr;
        sqlite3_stmt * stmt = nullptr;
        public:
        Pool pool;
        ProblemSet problemSet;
        Duel();
        ~Duel();
        void bind(std::int64_t tgID, std::string cfName);
        void unbind(std::int64_t tgID);
        base::Result<std::string> getCFName(std::int64_t tgID);
        bool isFree(std::int64_t tgID);
        bool isFree(User user);
        void invite(User from, User to, Problem problem);
        void reject(User user);
        void accept(User user);
        void endDuel(User user);
        base::Result<Problem> getRandomProblem(std::int64_t difficulty);
        bool checkCompleteness(std::string cfName, Problem problem);
    };
}
