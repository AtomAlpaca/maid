#include "duel.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <tgbot/tgbot.h>
#include <tgbot/tools/StringTools.h>
#include <utility>
#include <vector>

int main()
{
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rnd(seed);
    std::string token("");
    std::vector<std::string> lazy =
    {
        "我想睡觉",
        "呜",
        "呜呜",
        "呜呜呜",
        "呜呜呜呜",
        "呜呜呜呜呜",
        "好累",
        "好困",
        "肚子疼",
        "哭哭"
    };
    std::vector<TgBot::BotCommand::Ptr> commands;
    TgBot::BotCommand::Ptr cmdArray(new TgBot::BotCommand);
    std::vector<std::pair<std::string, std::string>> vec =
    {
        {"bind", "绑定 Codeforces 帐号"},
        {"unbind", "解绑 Codeforces 帐号"},
        {"name", "获取当前绑定的 Codeforces 帐号"},
        {"random", "获取随机的 Codeforces 题目"},
        {"duel", "向对方发起决斗请求"},
        {"accept", "接受对方的决斗请求"},
        {"reject", "拒绝决斗请求"},
        {"done", "完成决斗"}
    };
    for (auto & [i, j] : vec)
    {
        cmdArray = TgBot::BotCommand::Ptr(new TgBot::BotCommand);
        cmdArray -> command = i;
        cmdArray -> description = j;
        commands.push_back(cmdArray);
    }

    TgBot::Bot bot(token);
    duel::Duel duel;
    bot.getApi().setMyCommands(commands);
    bot.getEvents().onCommand("bind", [&](TgBot::Message::Ptr message) {
        auto vec = StringTools::split(message -> text, ' ');
        if (vec.size() != 2)
        {
            bot.getApi().sendMessage(message -> chat -> id, "用法：/bind <codeforces name>");
            return ;
        }
        if (duel.getCFName(message -> chat -> id).type)
        {
            bot.getApi().sendMessage(message -> chat -> id, "已经绑定了 Codeforces 帐号! 使用 /unbind 解绑");
            return ;
        }
        duel.bind(message -> from -> id, vec[1]);
        bot.getApi().sendMessage(message->chat->id, std::string("已将 Codeforces 帐号") + vec[1] + "绑定到您的 Telegram");
    });

    bot.getEvents().onCommand("unbind", [&](TgBot::Message::Ptr message) {
        auto res = duel.getCFName(message -> chat -> id);
        if (!res.type)
        {
            bot.getApi().sendMessage(message -> chat -> id, "还未绑定 Codeforces 帐号哦");
            return ;
        }
        if (!duel.isFree(duel::User(message -> from -> id, res.result)))
        {
            bot.getApi().sendMessage(message -> chat -> id, "目前不可以解除绑定！请先完成您的决斗并处理你的决斗请求！");
            return ;
        }
        duel.unbind(message -> from -> id);
        bot.getApi().sendMessage(message -> chat -> id, "已解绑您的 Codeforces 帐号");
    });

    bot.getEvents().onCommand("name", [&](TgBot::Message::Ptr message) {
        auto res = duel.getCFName(message -> from -> id);
        if (res.type)
        {
            bot.getApi().sendMessage(message -> chat -> id, res.result);
        }
        else
        {
            bot.getApi().sendMessage(message -> chat -> id, "还未绑定 Codeforces 帐号哦");
        }
    });

    bot.getEvents().onCommand("random", [&](TgBot::Message::Ptr message) {
        auto vec = StringTools::split(message -> text, ' ');
        auto ptr = std::make_shared<TgBot::ReplyParameters>();
        ptr -> messageId = message -> messageId;
        ptr -> chatId    = message -> chat -> id;
        ptr -> allowSendingWithoutReply = true;
        int64_t difficulty;
        if (vec.size() != 2)
        {
            difficulty = std::uniform_int_distribution<int64_t>(8, 35)(rnd) * 100;
        }
        else
        {
            difficulty = std::atoi(vec[1].c_str());
            if (difficulty % 100 != 0 or difficulty / 100 < 8 or difficulty / 100 > 35)
            {
                bot.getApi().sendMessage(message -> chat -> id, "没有这个难度的题目！", nullptr, ptr);
                return ;
            }
        }
        auto res = duel.getRandomProblem(difficulty);
        if (res.type == false)
        {
            bot.getApi().sendMessage(message -> chat -> id, "获取随机题目失败！");
            return ;
        }
        bot.getApi().sendMessage(message -> chat -> id,
                                std::string("https://codeforces.com/problemset/problem/")
                              + std::to_string(res.result.contest) + "/" + res.result.index,
                                nullptr,
                                ptr);
    });

    bot.getEvents().onCommand("duel", [&](TgBot::Message::Ptr message) {
        auto ptr = std::make_shared<TgBot::ReplyParameters>();
        ptr -> messageId = message -> messageId;
        ptr -> chatId    = message -> chat -> id;
        ptr -> allowSendingWithoutReply = true;
        if (message -> replyToMessage == nullptr)
        {
            bot.getApi().sendMessage(message -> chat -> id, "请对你想决斗的用户回复此命令！", nullptr, ptr);
            return ;
        }
        if (message -> from -> id == message -> replyToMessage -> from -> id)
        {
           bot.getApi().sendMessage(message -> chat -> id, "不可以和自己决斗！", nullptr, ptr);
           return ;
        }
        auto fromRes = duel.getCFName(message -> from -> id);
        auto toRes   = duel.getCFName(message -> replyToMessage -> from -> id);
        if (fromRes.type == false)
        {
            bot.getApi().sendMessage(message -> chat -> id, "你还没有绑定 Codeforces 帐号哦", nullptr, ptr);
            return ;
        }
        if (toRes.type == false)
        {
            bot.getApi().sendMessage(message -> chat -> id, "对方还没有绑定 Codeforces 帐号哦", nullptr, ptr);
            return ;
        }
        duel::User from(message -> from -> id, fromRes.result);
        duel::User to  (message -> replyToMessage -> from -> id, toRes.result);
        auto toStatus = duel.pool.getStatus(to);
        if (!toStatus.type or toStatus.result.duelStatus != duel::FREE)
        {
            bot.getApi().sendMessage(message -> chat -> id, "对方当前未处于空闲状态！", nullptr, ptr);
            return ;
        }
        auto vec = StringTools::split(message -> text, ' ');
        int64_t difficulty;
        if (vec.size() == 1)
        {
            difficulty = std::uniform_int_distribution<int64_t>(8, 35)(rnd) * 100;
        }
        else
        {
            difficulty = std::atoi(vec[1].c_str());
            if (difficulty % 100 != 0 or difficulty / 100 < 8 or difficulty / 100 > 35)
            {
                bot.getApi().sendMessage(message -> chat -> id, "没有这个难度的题目！");
                return ;
            }
        }
        auto res = duel.getRandomProblem(difficulty);
        if (res.type == false)
        {
            bot.getApi().sendMessage(message -> chat -> id, "获取随机题目失败！");
            return ;
        }
        bot.getApi().sendMessage(message -> chat -> id, from.cfName + "向" + to.cfName + "发起了决斗邀请！");
        duel.invite(from, to, res.result);
    });

    bot.getEvents().onCommand("accept", [&](TgBot::Message::Ptr message) {
        auto res = duel.getCFName(message -> from -> id);
        auto status = duel.pool.getStatus(duel::User(message -> chat -> id, res.result));
        bot.getApi().sendMessage(message -> chat -> id,
                                 res.result
                               + "接受了"
                               +  status.result.otherSide.cfName
                               + "的决斗申请！\n"
                               + "题目为： "
                               + "https://codeforces.com/problemset/problem/"
                               + std::to_string(status.result.problem.contest) + "/"
                               + status.result.problem.index);
        duel.accept(duel::User(message -> from -> id, res.result));
    });

    bot.getEvents().onCommand("reject", [&](TgBot::Message::Ptr message) {
        auto res = duel.getCFName(message -> from -> id);
        auto status = duel.pool.getStatus(duel::User(message -> chat -> id, res.result));
        bot.getApi().sendMessage(message -> chat -> id,
                                 res.result
                               + "拒绝了"
                               +  status.result.otherSide.cfName
                               + "的决斗申请！\n");
        duel.reject(duel::User(message -> from -> id, res.result));
    });

    bot.getEvents().onCommand("check", [&](TgBot::Message::Ptr message) {
        auto vec = StringTools::split(message -> text, ' ');
        auto ptr = std::make_shared<TgBot::ReplyParameters>();
        ptr -> messageId = message -> messageId;
        ptr -> chatId    = message -> chat -> id;
        ptr -> allowSendingWithoutReply = true;
        if (vec.size() != 4)
        {
            return ;
        }
        if (duel.checkCompleteness(vec[1], duel::Problem(std::atoi(vec[2].c_str()), vec[3])))
        {
            bot.getApi().sendMessage(message -> chat -> id, "Yes", nullptr, ptr);
        }
        else
        {
            bot.getApi().sendMessage(message -> chat -> id,
                                    "No",
                                    nullptr,
                                    ptr);
        }
    });

    bot.getEvents().onCommand("done", [&](TgBot::Message::Ptr message) {
        auto res = duel.getCFName(message -> from -> id);
        auto status = duel.pool.getStatus(duel::User(message -> chat -> id, res.result));
        auto ptr = std::make_shared<TgBot::ReplyParameters>();
        ptr -> messageId = message -> messageId;
        ptr -> chatId    = message -> chat -> id;
        ptr -> allowSendingWithoutReply = true;
        if (!status.type ||  status.result.duelStatus != duel::INDUEL)
        {
            bot.getApi().sendMessage(message -> chat -> id, "你还未参加决斗！", nullptr, ptr);
            return ;
        }
        if (duel.checkCompleteness(res.result, status.result.problem))
        {
            bot.getApi().sendMessage(message -> chat -> id, res.result + " 赢得了与 " + status.result.otherSide.cfName + "的决斗！", nullptr, ptr);
            duel.endDuel(duel::User(message -> from -> id, res.result));
            return ;
        }
        else
        {
            bot.getApi().sendMessage(message -> chat -> id, "你还未通过对应的题目！", nullptr, ptr);
        }
    });

    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
        auto ptr = std::make_shared<TgBot::ReplyParameters>();
        ptr -> messageId = message -> messageId;
        ptr -> chatId    = message -> chat -> id;
        ptr -> allowSendingWithoutReply = true;
        if (!StringTools::startsWith(message->text, "/")) {
            return;
        }
        if (std::uniform_int_distribution<int64_t>(0, 99)(rnd) == 0)
        {
            bot.getApi().sendMessage(message -> chat -> id, lazy[std::uniform_int_distribution<int64_t>(0, lazy.size() - 1)(rnd)]);
        }
        for (auto & [i, j] : vec)
        {
            if (StringTools::startsWith(message->text, std::string("/") + i))
            {
                return ;
            }
        }
        auto v = StringTools::split(message -> text, ' ');
        v[0].erase(v[0].begin());
        if (v.size() == 1)
        {
            if (message -> replyToMessage == nullptr)
            {
                bot.getApi().sendMessage(message -> chat -> id,
                    message -> from -> firstName + message -> from -> lastName + " " + v[0] + "了自己！",
                    nullptr, ptr);
            }
            else
            {
                bot.getApi().sendMessage(message -> chat -> id,
                    message -> from -> firstName + message -> from -> lastName + " " + v[0] + "了 "
                    + message -> replyToMessage -> from -> firstName + message -> replyToMessage -> from -> lastName + "！",
                    nullptr, ptr);
            }
        }
        else
        {
            if (message -> replyToMessage == nullptr)
            {
                bot.getApi().sendMessage(message -> chat -> id,
                    message -> from -> firstName + message -> from -> lastName + " " + v[0] + "自己" + v[1] + "！",
                    nullptr, ptr);
            }
            else
            {
                bot.getApi().sendMessage(message -> chat -> id, message -> from -> firstName + message -> from -> lastName + " " + v[0] +
                    message -> replyToMessage -> from -> firstName + message -> replyToMessage -> from -> lastName + v[1] + "！",
                    nullptr, ptr);
            }
        }
    });

    try
    {
        std::clog << "Bot started!" << std::endl;
        bot.getApi().deleteWebhook();
        TgBot::TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    } catch (std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
