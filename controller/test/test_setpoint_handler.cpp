#include <gtest/gtest.h>

#include "SetpointHandler.h"

enum class Output { O, L, M, H };

class TestHandler : public SetpointHandler<Output, float> {
    using SetpointHandler::SetpointHandler;
};

TEST(SetpointHandlerTest, TransitionsCorrectly) {
    // Based on some plausible temperature cutoff values
    TestHandler::Cutoff cutoffs[] = {
        TestHandler::Cutoff{Output::O, -0.5}, TestHandler::Cutoff{Output::L, 0.5},
        TestHandler::Cutoff{Output::M, 2}, TestHandler::Cutoff{Output::H, 5}};

    float deltas[5] = {-1.0, 0.0, 1.0, 3.0, 6.0};
    Output outputs[5][4] = {
        {Output::O, Output::O, Output::O, Output::O}, {Output::O, Output::L, Output::L, Output::L},
        {Output::L, Output::L, Output::M, Output::M}, {Output::M, Output::M, Output::M, Output::H},
        {Output::H, Output::H, Output::H, Output::H},
    };

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            TestHandler handler(cutoffs, 4);
            TestHandler::Cutoff *start_cutoff = &cutoffs[c];

            std::string msg = "row: " + std::to_string(r + 1) + " col: " + std::to_string(c + 1);

            // Set and check initial condition
            EXPECT_EQ(start_cutoff->output, handler.update(start_cutoff->relative_threshold + 0.01))
                << msg;
            EXPECT_EQ(outputs[r][c], handler.update(deltas[r])) << msg;
        }
    }
};
