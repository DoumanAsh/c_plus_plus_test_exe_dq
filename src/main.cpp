#include <iostream>

#include <thread>
#include <cassert>

#include "conn.hpp"

int main(int /*argc*/, char** /*argv*/) {
    Conn conn;

    std::mutex stream_mtx;
    std::vector<std::thread> jobs;

    auto fn = [&conn, &stream_mtx]() {
        Message sentMsg, recvMsg;
        conn.sendReceive(&sentMsg, &recvMsg);
        assert(sentMsg.get_id() == recvMsg.get_id());
        {
            std::lock_guard<std::mutex> lock(stream_mtx);
            std::cout << "Request(id=" << sentMsg.get_id() << ") -->\n"
                      << "<-- Response(id=" << recvMsg.get_id() << ")\n";
        }
    };

    for (size_t idx = 100; idx != 0; idx--) {
        jobs.emplace_back(fn);
    }

    for (auto& th : jobs) {
        th.join();
    }

    return 0;
}
