#pragma once

namespace base {
    template<class T>
    class Result
    {
        public:
        bool type;
        T result;
        Result(bool type, T result) : type(type), result(result) {};
    };
}
