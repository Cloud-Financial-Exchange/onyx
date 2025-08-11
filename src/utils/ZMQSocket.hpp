// Copyright [2024] NYU

#ifndef SRC_UTILS_ZMQSOCKET_HPP_
#define SRC_UTILS_ZMQSOCKET_HPP_

#ifndef ZMQ_BUILD_DRAFT_API
#define ZMQ_BUILD_DRAFT_API
#endif  // ZMQ_BUILD_DRAFT_API

#include <iostream>
#include <vector>
#include <string>
#include <tuple>

#include <zmq.hpp>
#include "./../build/message.pb.h"

class Zmqsocket {
 public:
    typedef enum {
        RED = 0,
        GREEN = 1,
        CLEAR = 2
    } COLOR;

    typedef enum {
        NONE = 0,
        INFO = 1,
        WARNING = 2,
        ERR = 3
    } LOG_LEVEL;

    Zmqsocket(std::string PROTOCOL,
              std::string IP,
              std::string PORT,
              zmq::socket_type TYPE,
              std::string NAME,
              LOG_LEVEL LEVEL = LOG_LEVEL::INFO);

    ~Zmqsocket();

    zmq::socket_t               socket;

    void connect(void);
    void connect(std::string PROTOCOL, std::string IP, std::string PORT);

    int  connect_peer(void);
    int  connect_peer(std::string PROTOCOL, std::string IP, std::string PORT);

    void bind(void);

    int send(zmq::message_t& msg, zmq::send_flags flags = zmq::send_flags::none);
    int recv(zmq::message_t& msg, zmq::recv_flags flags = zmq::recv_flags::none);

    int send_message(ManagementMsg msg);
    int send_message(ManagementMsg msg, int message_type);
    int send_message_to(ManagementMsg msg, int routing_id);
    int send_message_to(ManagementMsg msg, int message_type, int routing_id);

    int recv_message(ManagementMsg& msg);
    int recv_message_from(ManagementMsg& msg, int& id);

    std::tuple<int, int> expect_message(ManagementMsg& msg, MessageType exp);
    std::tuple<int, int> expect_message_from(ManagementMsg& msg, MessageType exp, int& id);

    void set_log(LOG_LEVEL verbosity);
    void set_name(std::string name);

    void log(const std::initializer_list<std::string>& strings);
    void log_term(const std::initializer_list<std::string>& strings, COLOR color = COLOR::CLEAR);
    void log_err(const std::initializer_list<std::string>& strings, const zmq::error_t& e);

    std::string get_ip(void);
    std::string get_port(void);
    std::string get_format(void);

 private:
    std::string                 ip;
    std::string                 port;
    std::string                 format;

    zmq::context_t              context;
    zmq::socket_type            type;

    const std::string red{"\033[31m"};
    const std::string green{"\033[32m"};
    const std::string clear{"\033[0m"};

    LOG_LEVEL           _level;
    std::string         _name;
};

#endif  // SRC_UTILS_ZMQSOCKET_HPP_
