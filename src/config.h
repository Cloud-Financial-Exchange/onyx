// Copyright [2023] NYU

#ifndef SRC_CONFIG_H_
#define SRC_CONFIG_H_

#include <cstdint>

namespace CONFIG {
    const int MSG_RATE = 175'000;
    const bool BURSTY = false;
    const int BURST_FACTOR = 1;
    const bool LOGGING = true;

    const bool ENFORCE_SOCKET_SENDER = false;
    const bool USING_EBPF_AT_SENDER = ENFORCE_SOCKET_SENDER && false;

    const bool SENDER_PROXY_GENERATES_MESSAGES = true;

    const bool ENFORCE_SOCKET_RECEIVER = false;  // set to true when multiple logical receivers are required

    const bool XDP_BASED_DEDUP = ENFORCE_SOCKET_RECEIVER && false;

    // Storage related configs
    // const char* S3_STATS_BUCKET = "expresults";  // aws
    const char* S3_STATS_BUCKET = "exp-results-nyu-systems-multicast";  // gcp
    const char* CLOUD = "gcp";
    // const char* CLOUD = "aws";

    const int CRITICAL_PATH_SEEN_IDS_MAP_CLEARING_THRESH = MSG_RATE;

    const bool SIMULATE_TRADING = false;

namespace LOSS_EXPERIMENT {
    const bool EXP = true;
    const bool LOGGING = true;
    const bool SIMULATE_LOSSES = false;
}

namespace REQUEST_DUPLICATION {
    const int FACTOR = 1;  // means a total of `x` requests will be sent (i.e., a total of x-1 new copies)
}

namespace HEDGING {
    const bool DYNAMIC_RELATIONSHIPS = false;  // only works with H=0,1,2

    // Not being used rn
    const bool MSG_HISTORY = false;  // REMEMBER TO INCREASE MSG SIZE FOR FAIR SIMULATTION!!!

    const bool SIBLING_BASED_HEDGING = false;  // safe to set if false, w/o changing intensity
    const int SIBLING_HEDGING_INTENSITY = 2;  // value of x means that x siblings will be hedged by a proxy
    // Hedging related config
    enum HEDGING_MODE_TYPE {
        RANDOM,
        FIXED_RANDOM,
        VARIANCE,
        NONE
    };

    const bool INJECT_FAULT = false;
    const int FAULTY_NODE_LOW = 2;
    const int FAULTY_NODE_HIGH = 9;
    const int FAULTY_SLEEP_MICROSECONDS = 300;

    const bool PARITY_BASED_EXPERIMENT = false;
    const int PARITY = 2;  // msg_id%PARITY == 0 (i.e. even message ids) will not be hedged
    const HEDGING_MODE_TYPE HEDGING_MODE = HEDGING_MODE_TYPE::NONE;

    const bool MULTIPLE_HEDGING_MODES = false;

    // Even numbered hedge nodes will be hedging_mode_1
    const HEDGING_MODE_TYPE HEDGING_MODE_1 = HEDGING_MODE_TYPE::RANDOM;

    // Odd numbered hedge nodes will be hedging_mode_2
    const HEDGING_MODE_TYPE HEDGING_MODE_2 = HEDGING_MODE_TYPE::VARIANCE;

    const int REQUEST_REORDERED_DOWNSTREAMS_AFTER_MESSAGES = 1 * MSG_RATE;
    const int REORDER_DOWNSTREAMS_AFTER_MILLISECONDS = 1000;
    const int HINTS_FREQUENCY_IN_MILLISECONDS = 500;

    int MAX_DOWNSTREAMS_FACTOR = 2;  // MAX_DOWNSTREAMS_FACTOR * BRANCHING_FACTOR will be total downstreams
                                           // Set to -1 for using MAX_DOWNSTREAMS_FACTOR = BRANCHING_FACTOR
}  // namespace HEDGING

namespace HOLDRELEASE {
    int DEFAULT_OWD = 50;  // microseconds
    int EXTRA_HOLD_ADDED_TO_DEADLINE = 25;  // microseconds
    double TARGET_PERCENTILE_FOR_OWD_ESTIMATE = 97.0;
    int WINDOW_SIZE_MILLISECONDS = 10;
    int SENDING_ESTIMATE_FREQUENCY_IN_MILLISECONDS = 15;

    enum HOLDRELEASE_MODE_TYPE {
        LEVEL_BY_LEVEL,
        END_TO_END,
        NONE
    };

    const HOLDRELEASE_MODE_TYPE HOLDRELEASE_MODE = HOLDRELEASE_MODE_TYPE::NONE;
}  // namespace HOLDRELEASE

// Only works for sockets mode right now
namespace ORDERS_SUBMISSION {
    const bool LOGGING = true;
    const bool STRESS_TEST = true;
    const int COLLECTIVE_RATE = 100'000;  // of all the traders, in orders/second
    const int SEQUENCER_DELAY = 400;  // microseconds
    const bool ENABLE_DELAYMONITOR = false;
    const uint64_t LOGS_FLUSH_THRESHOLD = 20 * 1'000'000;  // Logs will be flushed to a file after x microseocnds
    const char* LATENCY_LOGS  = "slogs.csv";  // for use during stress test
    const char* LOGS_FILE  = "mlogs.csv";  // matching order logs
    const char* ALOGS_FILE = "alogs.csv";  // all logs
    const char* QLOGS_FILE = "qlogs.csv";  // all logs
    const int FIXED_PACING_VAL = 0;  // is only used when DYNAMIC_PACING is false
    const bool DYNAMIC_PACING = false;
    bool BURST_MODE = true;
    const int BURST_FACTOR = 20;
    const int QTYPE = 0;  // 0 for simplepq, 1 for fancy(i.e., LOQv1), 2 for LOQv2
    const int MID_PRICE = 7;
    const int ACTION_WINDOW = 0;
    const int OWD_THRESH = 70;
    const bool PROXY_SEQUENCE = false;  // do we want the proxies to do sequencing?
    const bool ALWAYS_USE_SINGLE_PUBLISHER = true;
    const int THRESH_FOR_DUMMY_GENERATION = 1'000;  // microseconds
    const bool DUMMY_GENERATION = true;
}  // namespace ORDERS_SUBMISSION

}  // namespace CONFIG

#endif  // SRC_CONFIG_H_
