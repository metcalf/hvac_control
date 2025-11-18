#include "zone_io_client.h"

#include "driver/uart.h"
#include "esp_log.h"

#define ZIO_UART_NUM UART_NUM_2
#define ZIO_RXD 35

#define BUF_SIZE 1024
#define QUEUE_SIZE 1024

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                       \
    ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'), ((byte) & 0x20 ? '1' : '0'),         \
        ((byte) & 0x10 ? '1' : '0'), ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'),     \
        ((byte) & 0x02 ? '1' : '0'), ((byte) & 0x01 ? '1' : '0')

static const char *TAG = "ZIO";

static SemaphoreHandle_t mutex;
static uint8_t zio_buf_[BUF_SIZE];
static InputState last_input_state_;
static QueueHandle_t uart_queue;

struct Bits {
    unsigned b0 : 1, b1 : 1, b2 : 1, b3 : 1, b4 : 1, b5 : 1, b6 : 1, b7 : 1;
};
union BitByte {
    struct Bits bits;
    unsigned char byte;
};

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
    ESP_ERROR_CHECK(
        uart_driver_install(ZIO_UART_NUM, BUF_SIZE, 0, QUEUE_SIZE, &uart_queue, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ZIO_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ZIO_UART_NUM, UART_PIN_NO_CHANGE, ZIO_RXD, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
}

bool do_rx(size_t to_rx) {
    int rx_bytes = uart_read_bytes(ZIO_UART_NUM, zio_buf_, to_rx, portMAX_DELAY);

    if (rx_bytes < to_rx) {
        ESP_LOGD(TAG, "Read %d bytes (of %d expected)", rx_bytes, to_rx);
        return false;
    } else if (rx_bytes == 3) {
        // Special case is just for debug logging, it's OK if we don't receive 3 bytes at a time.
        ESP_LOGD(TAG,
                 "Read %d bytes: " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN
                 " " BYTE_TO_BINARY_PATTERN,
                 rx_bytes, BYTE_TO_BINARY(zio_buf_[0]), BYTE_TO_BINARY(zio_buf_[1]),
                 BYTE_TO_BINARY(zio_buf_[2]));
    } else {
        ESP_LOGD(TAG, "Read %d bytes, 1st: " BYTE_TO_BINARY_PATTERN, rx_bytes,
                 BYTE_TO_BINARY(zio_buf_[0]));
    }

    // Apply each update byte, modifying the last state
    InputState input_state = last_input_state_;
    for (int i = 0; i < rx_bytes; i++) {
        BitByte byte = {.byte = zio_buf_[i]};
        if (byte.bits.b7) {
            input_state.ts[2] = {.w = byte.bits.b0, .y = 0};
            // input_state.ts[3] = {.w = byte.bits.b1, .y = 0};
            input_state.load_control = byte.bits.b1;
            input_state.valve_sw[0] = static_cast<ValveSWState>((byte.byte >> 2) & 0x03);
            input_state.valve_sw[1] = static_cast<ValveSWState>((byte.byte >> 4) & 0x03);
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

    input_state.load_control = true; // TODO: Remove when powerwall is online

    if (input_state == last_input_state_) {
        return false;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    last_input_state_ = input_state;
    xSemaphoreGive(mutex);
    return true;
}

void zone_io_task(void *updateCb) {

    uart_event_t event;

    while (1) {
        // Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                if (do_rx(event.size)) {
                    ((void (*)())updateCb)();
                }

                break;
            case UART_BREAK:
                ESP_LOGD(TAG, "uart rx break");
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your
                // buffer size
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow
                // control for your application.
                break;
            case UART_FRAME_ERR:
                ESP_LOGW(TAG, "uart frame error");
                break;
            case UART_PARITY_ERR:
                ESP_LOGW(TAG, "uart parity error");
                break;
            case UART_DATA_BREAK:
                ESP_LOGW(TAG, "unexpected uart data break event");
                break;
            case UART_PATTERN_DET:
                ESP_LOGW(TAG, "unexpected uart pattern event");
                break;
            default:
                ESP_LOGW(TAG, "unexpected uart event type: %d", event.type);
                break;
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
