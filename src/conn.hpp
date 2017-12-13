#pragma once

#include <memory>
#include <chrono>
#include <algorithm>
#include <random>

#include <vector>
#include <mutex>
#include <condition_variable>

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

    //Stores messages that are to be sent to other threads.
    //We assume that IDs are unique for simplicity sake.
    std::mutex store_mtx;
    std::unordered_map<long, std::unique_ptr<Message>> msg_store;
    //Stores temp condvars for particular request.
    //If message with such ID is waited then thread is waken up.
    std::unordered_map<long, std::condition_variable> coord_vars;

    ///Adds condvar for thread with request id.
    inline auto add_cond_var(long id) {
        //Work around emplace with no arguments for condvar
        return this->coord_vars.emplace(std::piecewise_construct, std::make_tuple(id), std::make_tuple());
    }

    /**
     * Continuously waits for message with id.
     */
    std::unique_ptr<Message> wait_for_message(long id) {
        std::unique_lock<std::mutex> store_lock(this->store_mtx);
        auto msg = this->msg_store.find(id);

        if (msg == this->msg_store.cend()) {
            //Insert thread's condvar and wait until another thread will notify us.
            auto insert_res = this->add_cond_var(id);
            insert_res.first->second.wait(store_lock);

            msg = this->msg_store.find(id);

            this->coord_vars.erase(insert_res.first);
        }

        std::unique_ptr<Message> result = std::move(msg->second);
        this->msg_store.erase(msg);

        return result;
    }

    /**
     * Stores message and notify, if thread is already waiting for it.
     */
    inline void store_msg_and_notify(long id, std::unique_ptr<Message> msg) {
        std::lock_guard<std::mutex> store_lock(this->store_mtx);
        this->msg_store.emplace(id, std::move(msg));
        auto coord = this->coord_vars.find(id);
        if (coord != this->coord_vars.cend()) {
            coord->second.notify_all();
        }
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
            std::unique_ptr<Message> msg(this->receiveMessage());
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
