//
// Created by luca on 09/08/22.
//
#include "Codes.hpp"
#include "DecodingSimulator.hpp"
#include "UFDecoder.hpp"

#include <bitset>
#include <fstream>
#include <gtest/gtest.h>
using json = nlohmann::json;
class DecodingSimulatorTest : public testing::TestWithParam<std::string> {
};

TEST(DecodingSimulatorTest, TestRuntimeSim) {
    std::string       rawOut          = "./testRawFile";
    std::string       testOut         = "./testStatFile";
    const double      physicalErrRate = 0.01;
    std::size_t       nrRuns          = 1;
    std::size_t       nrSamples       = 1;
    const std::string codePath        = "./resources/codes/inCodes";
    auto              code            = SteaneXCode();
    try {
        UFDecoder decoder;
        decoder.setCode(code);
        DecodingSimulator::simulateAverageRuntime(rawOut, testOut, physicalErrRate, nrRuns, codePath, nrSamples, DecoderType::UF_HEURISTIC);
    } catch (QeccException& e) {
        std::cerr << "Exception caught " << e.getMessage();
        EXPECT_TRUE(false);
    }
    EXPECT_TRUE(true);
}

TEST(DecodingSimulatorTest, TestPerformanceSim) {
    std::string rawOut      = "./testRawFile";
    std::string testOut     = "./testStatFile";
    double      minErate    = 0.01;
    double      maxErate    = 0.03;
    double      stepSize    = 0.01;
    std::size_t runsPerRate = 2;
    auto        code        = SteaneCode();
    try {
        DecodingSimulator::simulateWER(rawOut, testOut, minErate, maxErate, runsPerRate, code, stepSize, DecoderType::UF_DECODER);
    } catch (QeccException& e) {
        std::cerr << "Exception caught " << e.getMessage();
        EXPECT_TRUE(false);
    }
    EXPECT_TRUE(true);
}
