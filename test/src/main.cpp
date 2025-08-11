// #include <gtest/gtest.h>
// #include <vector>
// #include <string>
// #include <unordered_set>

// #include "main.hpp"

// struct DownstreamTestParams {
//     int eid;
//     int branching;
//     int redundancy;
//     int total_proxies_without_redundant_nodes;
// };

// std::ostream& operator<<(std::ostream& os, const DownstreamTestParams& params) {
//     os << "ENTITY ID=" << params.eid << ", "
//        << "BRANCHING=" << params.branching << ", "
//        << "REDUNDANCY=" << params.redundancy << ", "
//        << "PROXIES W/O RED. NODES=" << params.total_proxies_without_redundant_nodes << std::endl;

//     return os;
// }

// class DownstreamTestSuite : public ::testing::TestWithParam<DownstreamTestParams> {
// public:
//     static std::string print_vector(const std::vector<std::string>& vec) {
//         std::stringstream ss;
//         ss << "{" << std::endl;
//         for(size_t i = 0; i < vec.size(); ++i) {
//             ss << "\t" << i << ": " << vec[i];
//             if (i < vec.size() - 1) {
//                 ss << ",";
//             }
//             ss << std::endl;
//         }
//         ss << "}";
//         return ss.str();
//     }
// protected:

// };

// TEST_P(DownstreamTestSuite, DownstreamTest) {
//     DownstreamTestParams params = GetParam();
//     auto downstream_addrs = get_management_downstreams(
//         params.eid, 
//         params.branching, 
//         params.redundancy, 
//         params.total_proxies_without_redundant_nodes
//     );

//     std::unordered_set<std::string> set(downstream_addrs.begin(), downstream_addrs.end());
//     bool duplicates = (set.size() != downstream_addrs.size());

//     EXPECT_FALSE(duplicates)    << "DOWNSTREAM ADDRESSES CONTAIN DUPLICATES:" << std::endl
//                                 << DownstreamTestSuite::print_vector(downstream_addrs);
// }

// INSTANTIATE_TEST_SUITE_P(
//     DownstreamTests,
//     DownstreamTestSuite,
//     ::testing::Values(
//         DownstreamTestParams{0, 2, 0, 7},
//         DownstreamTestParams{1, 2, 0, 7},
//         DownstreamTestParams{2, 2, 0, 7},
//         DownstreamTestParams{3, 2, 0, 7},
//         DownstreamTestParams{4, 2, 0, 7},
//         DownstreamTestParams{5, 2, 0, 7},
//         DownstreamTestParams{6, 2, 0, 7}
//     ),
//     [](const testing::TestParamInfo<DownstreamTestSuite::ParamType>& info) {
//         std::stringstream ss;
//         ss << info.index;
//         return ss.str();
//     }
// );

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
