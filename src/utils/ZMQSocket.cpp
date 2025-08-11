// Copyright [2024] NYU

#include "ZMQSocket.hpp"  // NOLINT

#include <stdexcept>
#include <tuple>
#include <iostream>
#include <ostream>

typedef std::initializer_list<std::string> str_list;

static std::string concatenate(const std::initializer_list<std::string>& strings) {
    std::ostringstream stream;
    for (const auto& str : strings)
        stream << str;

    return stream.str();
}

std::string Zmqsocket::get_ip(void) {
    return std::string{this->ip};
}

std::string Zmqsocket::get_port(void) {
    return std::string{this->port};
}

std::string Zmqsocket::get_format(void) {
    return std::string{this->format};
}

void Zmqsocket::set_log(Zmqsocket::LOG_LEVEL level) {
    this->_level = level;
}

void Zmqsocket::set_name(std::string name) {
    this->_name = name;
}

void Zmqsocket::log(const std::initializer_list<std::string>& strings) {
    return;  // Override log
    if (this->_level == Zmqsocket::LOG_LEVEL::NONE)
        return;

    std::string data{concatenate(strings)};
    std::cout << "ZMQLOG::"
              << this->_name
              << " "
              << data
              << std::endl;
}

void Zmqsocket::log_term(const std::initializer_list<std::string>& strings, Zmqsocket::COLOR color) {
    if (this->_level == Zmqsocket::LOG_LEVEL::NONE)
        return;

    std::string data{concatenate(strings)};
    std::cout << color
              << "ZMQLOG::"
              << this->_name
              << " "
              << data
              << std::endl;
}

void Zmqsocket::log_err(const std::initializer_list<std::string>& strings, const zmq::error_t& e) {
    if (this->_level == Zmqsocket::LOG_LEVEL::NONE) {
        throw(e);
        return;
    }

    std::string data{concatenate(strings)};
    std::cerr << "LOGERR: " << data << std::endl;
    std::cerr << "\tERRNO: " << e.num() << " => " << e.what() << std::endl;
}

Zmqsocket::~Zmqsocket(void) {
    this->log({"CLOSED:  PORT=", this->port, " IP=", this->ip});
    this->socket.close();
}

Zmqsocket::Zmqsocket(std::string PROTOCOL,
                    std::string IP,
                    std::string PORT,
                    zmq::socket_type TYPE,
                    std::string NAME,
                    Zmqsocket::LOG_LEVEL LEVEL) :
    _level(LEVEL),
    _name(NAME) {
    this->ip        = IP;
    this->port      = PORT;
    this->format    = std::string{PROTOCOL + "://" + IP + ":" + PORT};

    this->context   = zmq::context_t{1};
    this->socket    = zmq::socket_t{this->context, (this->type = TYPE)};
    this->log({"OPENED"});
}


void Zmqsocket::connect(void) {
    this->socket.connect(this->format);
    this->log({"CONNECTED IP=", this->ip, " PORT=", this->port});
}

void Zmqsocket::connect(std::string PROTOCOL, std::string IP, std::string PORT) {
    std::string format{PROTOCOL + "://" + IP + ":" + PORT};
    this->socket.connect(format);
    this->log({"CONNECTED IP=", IP, " PORT=", PORT});
}

int Zmqsocket::connect_peer(void) {
    std::string endpoint{this->format};
    void* _socket = reinterpret_cast<void*>(this->socket.handle());
    uint32_t routing_id = zmq_connect_peer(_socket, endpoint.c_str());
    this->log({"CONNECTED IP=", this->ip, " PORT=", this->port});
    return routing_id;
}

int Zmqsocket::connect_peer(std::string PROTOCOL, std::string IP, std::string PORT) {
    std::string endpoint{PROTOCOL + "://" + IP + ":" + PORT};
    void* _socket = reinterpret_cast<void*>(this->socket.handle());
    uint32_t routing_id = zmq_connect_peer(_socket, endpoint.c_str());
    this->log({"CONNECTED IP=", IP, " PORT=", PORT});
    return routing_id;
}

void Zmqsocket::bind(void) {
    this->log({"ATTEMPT IP=", this->ip, " PORT=", this->port});
    this->socket.bind(this->format);
    this->log({"BOUND IP=", this->ip, " PORT=", this->port});
}

int Zmqsocket::send(zmq::message_t& msg, zmq::send_flags flags) {
    auto res = this->socket.send(msg, flags);
    if (res.has_value()) {
        this->log({"SENT DATA"});
        return res.value();
    }

    throw(std::runtime_error("Failed to send data"));
}

int Zmqsocket::recv(zmq::message_t& msg, zmq::recv_flags flags) {
    this->log({"RECV WAIT"});
    try {
        auto res = this->socket.recv(msg, flags);
        if (res.has_value()) {
            this->log({"RECV DATA"});
            return res.value();
        } else {
            this->log({"RECV EMPTY DATA"});
            return 0;
        }
    } catch (const zmq::error_t& e) {
        this->log_err({"ERROR ON SOCKET RECV"}, e);
        exit(EXIT_FAILURE);
    }
}

int Zmqsocket::send_message(ManagementMsg msg) {
    std::string data;
    msg.SerializeToString(&data);
    zmq::message_t zmqmsg(data);
    this->log({"SENT MESSAGE TYPE: ", std::to_string(msg.msg_type())});
    return this->send(zmqmsg);
}

int Zmqsocket::send_message(ManagementMsg msg, int message_type) {
    msg.set_msg_type(message_type);
    return this->send_message(msg);
}

int Zmqsocket::send_message_to(ManagementMsg msg, int routing_id) {
    std::string data;
    msg.SerializeToString(&data);
    zmq::message_t zmqmsg{data};
    zmqmsg.set_routing_id(routing_id);
    std::string msgtype{MessageType_Name(MessageType(msg.msg_type()))};
    this->log({"SENT MESSAGE TYPE: ", msgtype});
    return this->send(zmqmsg);
}

int Zmqsocket::send_message_to(ManagementMsg msg, int message_type, int routing_id) {
    msg.set_msg_type(message_type);
    return this->send_message_to(msg, routing_id);
}

int Zmqsocket::recv_message(ManagementMsg& msg) {
    zmq::message_t zmqmsg;
    int n = this->recv(zmqmsg);
    if (n > 0)
        msg.ParseFromArray(reinterpret_cast<char*>(zmqmsg.data()), n);

    std::string msgtype{MessageType_Name(MessageType(msg.msg_type()))};
    this->log({"RECV MESSAGE TYPE: ", msgtype});
    return n;
}

int Zmqsocket::recv_message_from(ManagementMsg& msg, int& id) {
    zmq::message_t zmqmsg;
    int n = this->recv(zmqmsg);
    id = zmqmsg.routing_id();
    if (n > 0)
        msg.ParseFromArray(reinterpret_cast<char*>(zmqmsg.data()), n);

    std::string msgtype{MessageType_Name(MessageType(msg.msg_type()))};
    this->log({"RECV MESSAGE TYPE: ", msgtype});
    return n;
}

std::tuple<int, int> Zmqsocket::expect_message(ManagementMsg& msg, MessageType exp) {
    int n = this->recv_message(msg);
    int match = 0;

    std::string msgtype{MessageType_Name(MessageType(msg.msg_type()))};
    std::string exptype{MessageType_Name(exp)};

    match = (msgtype == exptype);
    if (!match)
        this->log({"ERR: INCORRECT MESSAGE TYPE RECEIVED: ", msgtype});

    return std::make_tuple(n, match);
}

std::tuple<int, int> Zmqsocket::expect_message_from(ManagementMsg& msg, MessageType exp, int& id) {
    int n = this->recv_message_from(msg, id);
    int match = 0;

    std::string msgtype{MessageType_Name(MessageType(msg.msg_type()))};
    std::string exptype{MessageType_Name(exp)};

    match = (msgtype == exptype);
    if (!match)
        this->log({"ERR: INCORRECT MESSAGE TYPE RECEIVED: ", msgtype});

    return std::make_tuple(n, match);
}
