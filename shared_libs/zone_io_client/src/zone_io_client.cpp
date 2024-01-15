#include "zone_io_client.h"

#if defined(ESP_PLATFORM)

#include "driver/uart.h"
#include "esp_log.h"

#define ZIO_UART_NUM UART_NUM_1
#define ZIO_RXD 35

#define BUF_SIZE (1024)

static const char *TAG = "ZIO";

static SemaphoreHandle_t mutex;
static uint8_t zio_buf_[BUF_SIZE];
static InputState last_input_state_;

struct Bits {
    unsigned b0 : 1, b1 : 1, b2 : 1, b3 : 1, b4 : 1, b5 : 1, b6 : 1, b7 : 1;
};
union BitByte {
    struct Bits bits;
    unsigned char byte;
};

void zone_io_log_state(InputState s) {
    ESP_LOGI(
        TAG, "FC:%d%d|%d%d|%d%d|%d%d V:%c%c|%c%c|%c%c|%c%c SW: %d",
        // Fancoils
        s.fc[0].v, s.fc[0].ob, s.fc[1].v, s.fc[1].ob, s.fc[2].v, s.fc[2].ob, s.fc[3].v, s.fc[3].ob,
        // Thermostats
        s.ts[0].w ? 'w' : '_', s.ts[0].y ? 'y' : '_', s.ts[1].w ? 'w' : '_', s.ts[1].y ? 'y' : '_',
        s.ts[2].w ? 'w' : '_', s.ts[2].y ? 'y' : '_', s.ts[3].w ? 'w' : '_', s.ts[3].y ? 'y' : '_',
        // Valve SW
        static_cast<int>(s.valve_sw));
}

bool zone_io_state_eq(InputState s1, InputState s2) {
    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        if (s1.fc[i].v != s2.fc[i].v || s1.fc[i].ob != s2.fc[i].ob) {
            return false;
        }
    }

    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        if (s1.ts[i].w != s2.ts[i].w || s1.ts[i].y != s2.ts[i].y) {
            return false;
        }
    }

    return s1.valve_sw == s2.valve_sw;
}

void zone_io_init() {
    mutex = xSemaphoreCreateMutex();

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_driver_install(ZIO_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ZIO_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ZIO_UART_NUM, UART_PIN_NO_CHANGE, ZIO_RXD, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
}

void zone_io_task(void *) {
    while (1) {
        int rx_bytes = uart_read_bytes(ZIO_UART_NUM, zio_buf_, BUF_SIZE, portMAX_DELAY);
        ESP_LOGD(TAG, "Read %d bytes", rx_bytes);

        if (rx_bytes < 0) {
            // TODO: Report error
            continue;
        }

        // Apply each update byte, modifying the last state
        InputState input_state = last_input_state_;
        for (int i = 0; i < rx_bytes; i++) {
            BitByte byte = {.byte = zio_buf_[i]};
            if (byte.bits.b7) {
                input_state.ts[2] = {.w = byte.bits.b0, .y = byte.bits.b1};
                input_state.ts[3] = {.w = byte.bits.b2, .y = byte.bits.b3};
                input_state.valve_sw = static_cast<ValveSWState>((byte.byte >> 4) & 0x03);
            } else if (byte.bits.b6) {
                input_state.fc[3] = {.v = byte.bits.b0, .ob = byte.bits.b1};
                input_state.ts[0] = {.w = byte.bits.b2, .y = byte.bits.b3};
                input_state.ts[1] = {.w = byte.bits.b4, .y = byte.bits.b5};
            } else {
                input_state.fc[0] = {.v = byte.bits.b0, .ob = byte.bits.b1};
                input_state.fc[1] = {.v = byte.bits.b2, .ob = byte.bits.b3};
                input_state.fc[2] = {.v = byte.bits.b4, .ob = byte.bits.b5};
            }
        }

        if (!zone_io_state_eq(input_state, last_input_state_)) {
            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                last_input_state_ = input_state;
                xSemaphoreGive(mutex);
            }
        }
    }
}

InputState zone_io_get_state() {
    assert(xSemaphoreTake(mutex, portMAX_DELAY));
    InputState state = last_input_state_;
    xSemaphoreGive(mutex);
    return state;
}

#else // defined(ESP_PLATFORM)

#endif // defined(ESP_PLATFORM)
