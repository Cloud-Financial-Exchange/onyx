# Documentation
## 1. Jasper on GCP
### 1.1 HugePage Allocation
Changes to `grub.cfg`:
```cfg
GRUB_CMDLINE_LINUX="hugepagesz=1G hugepages=4 hugepagesz=2M hugepages=5120"
```

## 2. ZeroMQ and MAC Collection

As a result of the [MAC collection ticket](https://github.com/HaseebLUMS/dom-tenant-service/issues/71), we created a helper class that could 
abstract away the minutia of _unix_ sockets to enable a more scalable and robust manner to distribute the MAC addresses needed by various nodes 
across our tree.

The issue was mainly that the way in which the MAC addresses were collected/distributed before _runtime_ was not scalable, since 
the `client node` had to notify all `proxy` and `receiver` nodes of the collection process, receive their `MAC`s and then process to distribute them 
correctly down to each node with it's needed downstream `MAC` addresses. This is not necessarily scalable as the number of nodes increase, the number of `TCP` sockets to open and manage from a single client can become unbearable.

__Initial Solution__: The `Client` only talks to `Proxies` and asks them to collect their needed downstream `MACs` on their own. This reduces the number of sockets needed by the `Client` and then distributes the amount of communication through the proxies. Each `Proxy` will find out who are 
the downstream nodes that they need to talk to, which might be either other `Proxies` or `Receivers`, through function called `get_downstream(...)`. They will connect to every single `IP` listed in the vector returned by that function, request the endpoint's `MAC` address and wait on collection. When all needed `MACs` are acquired, `Proxy` then replies back to the `Client` letting it know it has finished.

This is how the new mac collection communication scheme is implemented:

- `C`: Client
- `P`: Proxy
- `D`: Downstream Node, could be either another `Proxy` or a `Receiver`
```
C  -----------------------> P ------------------> D0
    REQ_MAC_COLLECTION         REQ_MAC_ADDRESS
                            P <------------------ D0
                               ACK_MAC_ADDRESS

                            P ------------------> D1
                            P <------------------ D1

                            P ------------------> D2
                            P <------------------ D2
                                     . 
                                     . 
                                     .
                            P ------------------> DN
                            P <------------------ DN
C  <----------------------- P
    ACK_MAC_COLLECTION

```

To perform this in a scalable, efficient and easy to extend manner, we chose to use the [ZeroMQ](https://zeromq.org/) library, which is a known 
and well-established messaging library. Also, we chose to use it's most used `C++` wrapper [cppzmq](https://github.com/zeromq/cppzmq) for ease of use. Finally, for encapsulation and re-utilization purposes we have chosen to implement a small wrapper class around _ZeroMQ_ sockets to facilitate it's usage across the code-base. This class is called `ZMQSocket` and is shown later in the document.

### 1.1 ZeroMQ

This is a quick introduction to _ZeroMQ_. It implements a solid layer of abstraction over several protocols and creates different types of sockets 
that employ different communication patterns. Links to the API, documentation and codebase are available below and are the better source of information without a doubt. Given the communication pattern needed above, the chosen socket type for our use-case was `ZMQ_PEER` as it allows to both passively wait on connections, as well as actively connect to other nodes in the network. In addition to that, it also has a feature to store `IDs` of connections as a manner to later send messages to specific peer's in the network, which is very helpful.


>ZeroMQ (also spelled ØMQ, 0MQ or ZMQ) is a high-performance asynchronous messaging library, aimed at use in distributed or concurrent applications. It provides a message queue, but unlike message-oriented middleware, a ZeroMQ system can run without a dedicated message broker.
>ZeroMQ supports common messaging patterns (pub/sub, request/reply, client/server and others) over a variety of transports (TCP, in-process, inter-process, multicast, WebSocket and more), making inter-process messaging as simple as inter-thread messaging. This keeps your code clear, modular and extremely easy to scale.

<div align="center">
 <table>
 <tr> <th>Communication Patterns</th> <th>ZeroMQ Socket Type(s)</th> </tr>

 <tr>
     <td> Request x Reply</td> <td> ZMQ_REQ, ZMQ_REP, ZMQ_ROUTER, ZMQ_DEALER </td> 
 </tr>
 <tr>
     <td> Peer x Peer</td> <td> ZMQ_PEER </td> 
 </tr>
 <tr>
     <td> Client x Server</td> <td> ZMQ_CLIENT, ZMQ_SERVER </td> 
 </tr>
     <td> Radio x Dish</td> <td> ZMQ_RADIO, ZMQ_DISH </td> 
 </tr>
     <td> Pub x Sub</td> <td> ZMQ_PUB, ZMQ_SUB, ZMQ_XPUB, ZMQ_XSUB </td> 
 </tr>
     <td> Pub x Sub</td> <td> ZMQ_PUB, ZMQ_SUB, ZMQ_XPUB, ZMQ_XSUB </td> 
 </tr>
 </tr>
     <td> Pipeline </td> <td> ZMQ_PUSH, ZMQ_PULL</td> 
 </tr>
 </tr>
     <td> Exclusive Pair / Inter-Thread </td> <td> ZMQ_PAIR</td> 
 </tr>
 </tr>
     <td> Channel </td> <td> ZMQ_CHANNEL</td> 
 </tr>
 </tr>
     <td> Native / TCP </td> <td> ZMQ_STREAM</td> 
 </tr>

 </table> 
</div>

- [zmq](https://zeromq.org/)
- [libzmq](https://github.com/zeromq/libzmq)
- [cppzmq](https://github.com/zeromq/cppzmq)
- [zmq socket types](https://libzmq.readthedocs.io/en/latest/zmq_socket.html)
- [zmq API](https://libzmq.readthedocs.io/en/latest/)

### 1.1 ZMQSocket Class

The added `MessageType` enum used in `message.proto` and the `ZMQSocket` class in question:

- Message Types:
```cpp 
enum MessageType {
    REQ_UNKNOWN = 0;
    ACK_UNKNOWN = 1;
    REQ_MAC_COLLECTION = 2;
    ACK_MAC_COLLECTION = 3;
    REQ_MAC_ADDRESS = 4;
    ACK_MAC_ADDRESS = 5;
    REQ_STATS_COLLECTION = 6;
    ACK_STATS_COLLECTION = 7;
}

```

- ZMQSocket Class:
```cpp 
class ZMQSocket {
public:
    // helpful enums

    ZMQSocket(std::string PROTOCOL,
              std::string IP,
              std::string PORT,
              zmq::socket_type TYPE, 
              std::string NAME, 
              LOG_LEVEL LEVEL=LOG_LEVEL::INFO);

    ~ZMQSocket();

    zmq::socket_t               socket;

    void connect(void);
    void connect(std::string PROTOCOL, std::string IP, std::string PORT);

    int  connect_peer(void);
    int  connect_peer(std::string PROTOCOL, std::string IP, std::string PORT);

    void bind(void);

    int send(zmq::message_t& msg, zmq::send_flags flags=zmq::send_flags::none);
    int recv(zmq::message_t& msg, zmq::recv_flags flags=zmq::recv_flags::none);
    
    int send_message(ManagementMsg msg);
    int send_message(ManagementMsg msg, int message_type);
    int send_message_to(ManagementMsg msg, int routing_id);
    int send_message_to(ManagementMsg msg, int message_type, int routing_id);

    int recv_message(ManagementMsg& msg);
    int recv_message_from(ManagementMsg& msg, int& id);

    std::tuple<int, int> expect_message(ManagementMsg& msg, MessageType exp);
    std::tuple<int, int> expect_message_from(ManagementMsg& msg, MessageType exp, int& id);

    // logging

    // getters

private:
    // private data structures
};


```

## 3. Testing via GTest
A unit-testing folder was added to the project as well under `test`. It simply uses `CMake` and 
`GoogleTest` to compile a testing binary that inspects the behavior of specific functions that 
initially caused issues with the new `MAC` collection system. Mostly to do with the `get_downstream(...)` 
function. It will also be added to the merge request as it could be useful for future use.
