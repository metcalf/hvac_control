#include <gmock/gmock.h>
#include <gtest/gtest.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
extern "C" void app_main() {
    Serial.begin(115200);
    ESP_LOGE("NOT IMPLEMENTED");

    // ::testing::InitGoogleMock();

    // // Run tests
    // if (RUN_ALL_TESTS())
    //     ;

    // // sleep for 1 sec
    // delay(1000);
}

#else
int main(int argc, char **argv) {
    ::testing::InitGoogleMock(&argc, argv);

    if (RUN_ALL_TESTS())
        ;

    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}
#endif
