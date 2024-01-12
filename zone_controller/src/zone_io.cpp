#include "zone_io.h"

#include "driver/uart.h"

#define ZIO_UART_NUM UART_NUM_1
#define ZIO_RXD 35

#define BUF_SIZE (1024)

static SemaphoreHandle_t mutex;
static uint8_t zio_buf_[BUF_SIZE];
static ZoneIOState last_zio_state_;

struct Bits {
    unsigned b0 : 1, b1 : 1, b2 : 1, b3 : 1, b4 : 1, b5 : 1, b6 : 1, b7 : 1;
};
union BitByte {
    struct Bits bits;
    unsigned char byte;
};

static bool state_eq(ZoneIOState s1, ZoneIOState s2) {
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

        if (rx_bytes < 0) {
            // TODO: Report error
            continue;
        }

        // Apply each update byte, modifying the last state
        ZoneIOState zio_state = last_zio_state_;
        for (int i = 0; i < rx_bytes; i++) {
            BitByte byte = {.byte = zio_buf_[i]};
            if (byte.bits.b7) {
                zio_state.ts[2] = {.w = byte.bits.b0, .y = byte.bits.b1};
                zio_state.ts[3] = {.w = byte.bits.b2, .y = byte.bits.b3};
                zio_state.valve_sw = static_cast<ValveSW>((byte.byte >> 4) & 0x03);
            } else if (byte.bits.b6) {
                zio_state.fc[3] = {.v = byte.bits.b0, .ob = byte.bits.b1};
                zio_state.ts[0] = {.w = byte.bits.b2, .y = byte.bits.b3};
                zio_state.ts[1] = {.w = byte.bits.b4, .y = byte.bits.b5};
            } else {
                zio_state.fc[0] = {.v = byte.bits.b0, .ob = byte.bits.b1};
                zio_state.fc[1] = {.v = byte.bits.b2, .ob = byte.bits.b3};
                zio_state.fc[2] = {.v = byte.bits.b4, .ob = byte.bits.b5};
            }
        }

        if (!state_eq(zio_state, last_zio_state_)) {
            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                last_zio_state_ = zio_state;
                xSemaphoreGive(mutex);
            }
        }
    }
}

ZoneIOState zone_io_get_state() {
    assert(xSemaphoreTake(mutex, portMAX_DELAY));
    ZoneIOState state = last_zio_state_;
    xSemaphoreGive(mutex);
    return state;
}
