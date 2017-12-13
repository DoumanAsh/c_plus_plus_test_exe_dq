#pragma once

#include <memory>
#include <chrono>
#include <algorithm>
#include <random>

#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>

#include <unordered_map>

static std::mt19937 random; // NOLINT

class Message {
    long id;

    public:
        ///Defaul constructor makes message with random ID.
        Message() noexcept {

            this->id = random();
        }
        explicit Message(long id) noexcept : id(id) {}

        long get_id() const {
            return this->id;
        }
};

class Conn {
    //Internal stuff to emulate some request/response
    std::vector<Message*> sent_queue;
    bool sendMessage(Message* requestToSend) {
        this->sent_queue.push_back(new Message(*requestToSend));
        return true;
    }
    Message* receiveMessage() {
        std::shuffle(this->sent_queue.begin(), this->sent_queue.end(), random);
        Message* result = this->sent_queue.back();
        this->sent_queue.pop_back();
        return result;
    }

    static void random_wait() {
        std::uniform_int_distribution<int> rnd(100, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(rnd(random)));
    }

    //Implementation

    //Used to control access to socket.
    std::mutex socket_mtx;

    using Message_p = std::unique_ptr<Message>;
    //Stores messages that are to be sent to other threads.
    //We assume that IDs are unique for simplicity sake.
    std::mutex await_rsps_mtx;
    /**
     * Map of Request ID to promise that resolves into response.
     *
     * It simplifies flow of code and allow to wait for actual response to appear.
     * instead of using mutex and separate condvar to signal when response is available.
     */
    std::unordered_map<long, std::promise<Message_p>> await_rsps;

    /**
     * Retrieves promise with response for Request's ID.
     *
     * If no one is still waiting for it then it is created.
     * Otherwise reference to existing one is returned.
     */
    inline std::promise<Message_p>& get_awaiting_response(long id) {
        std::lock_guard<std::mutex> store_lock(this->await_rsps_mtx);
        return this->await_rsps[id];
    }

    /**
     * Waits until promise with Response for Request's ID is resolved.
     */
    inline Message_p wait_for_message(long id) {
        auto result = this->get_awaiting_response(id).get_future().get();
        {

            std::lock_guard<std::mutex> store_lock(this->await_rsps_mtx);
            this->await_rsps.erase(id);
        }
        return result;
    }

    /**
     * Stores resolved promise for Request's ID.
     */
    inline void store_msg_and_notify(long id, Message_p msg) {
        std::promise<Message_p>& await_rsp = this->get_awaiting_response(id);
        await_rsp.set_value(std::move(msg));
    }

    public:
        bool sendReceive(Message* requestToSend, Message* expectedReply) {
            std::unique_lock<std::mutex> socket_lock(this->socket_mtx);
            auto result = this->sendMessage(requestToSend);
            socket_lock.unlock();

            //Emulate delays in receiving requests
            //for out of order response.
            Conn::random_wait();

            socket_lock.lock();
            Message_p msg(this->receiveMessage());
            socket_lock.unlock();

            auto expected_id = requestToSend->get_id();
            auto msg_id = msg->get_id();
            if (msg_id == expected_id) {
                *expectedReply = *msg;
            }
            else {
                //If we received unexpected response
                //store it and wait for ours.
                this->store_msg_and_notify(msg_id, std::move(msg));
                *expectedReply = *wait_for_message(expected_id);
            }

            return result;
        }
};
