/** I2S DMA completions advance the game clock in 8 ms increments. */
#include "beat_audio.h"

#include "beat_level.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdgoods_audio.h"

enum {
    RATE = 16000,
    FRAMES = 128,
    BLOCK_MS = FRAMES * 1000 / RATE,
    DESCRIPTORS = 8,
    TRACK_FRAMES = TRACK_MS * (RATE / 1000),
    MIX_PEAK = 9400,
};
static_assert(TRACK_FRAMES % FRAMES == 0, "the track must fill whole DMA blocks");

extern const int16_t song_start[] asm("_binary_kinetic_circuit_pcm_start");
extern const int16_t song_end[] asm("_binary_kinetic_circuit_pcm_end");
extern const int16_t beat_chirp[];
extern const uint32_t beat_chirp_frames;

static const char *TAG = "beat_audio";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t player;
static i2s_chan_handle_t tx;
static uint32_t requested;
static uint32_t requested_ms;
static bool play_requested;
static bool chirp_pending;
static uint32_t chirp_ms;
static uint32_t playing_id;
static uint32_t sent_blocks;
static uint32_t starting_ms;
static beat_audio_state_t state;
static bool overflow;
static int16_t stereo[2 * FRAMES];

static bool IRAM_ATTR on_sent(i2s_chan_handle_t chan, i2s_event_data_t *event, void *context)
{
    (void)chan;
    (void)event;
    (void)context;
    portENTER_CRITICAL_ISR(&lock);
    sent_blocks++;
    portEXIT_CRITICAL_ISR(&lock);
    return false;
}

static bool IRAM_ATTR on_overflow(i2s_chan_handle_t chan, i2s_event_data_t *event, void *context)
{
    (void)chan;
    (void)event;
    (void)context;
    portENTER_CRITICAL_ISR(&lock);
    overflow = true;
    portEXIT_CRITICAL_ISR(&lock);
    return false;
}

static void amplifier(bool on)
{
    gpio_set_level(BOARD_AUDIO_PA_EN_GPIO, on == BOARD_AUDIO_PA_EN_ACTIVE_LEVEL);
}

static esp_err_t configure(void)
{
    if ((uintptr_t)song_end - (uintptr_t)song_start != TRACK_FRAMES * sizeof(int16_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    const gpio_config_t power = {
        .pin_bit_mask = 1ULL << BOARD_AUDIO_PA_EN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t result = gpio_config(&power);
    if (result != ESP_OK) {
        return result;
    }
    amplifier(false);
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = DESCRIPTORS;
    channel.dma_frame_num = FRAMES;
    channel.auto_clear_before_cb = true;
    result = i2s_new_channel(&channel, &tx, NULL);
    if (result != ESP_OK) {
        return result;
    }

    i2s_std_config_t format = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_SPK_I2S_GPIO_MCLK, .bclk = BOARD_SPK_I2S_GPIO_BCLK,
            .ws = BOARD_SPK_I2S_GPIO_WS, .dout = BOARD_SPK_I2S_GPIO_DOUT, .din = GPIO_NUM_NC,
        },
    };
    format.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    result = i2s_channel_init_std_mode(tx, &format);
    if (result != ESP_OK) {
        return result;
    }
    const i2s_event_callbacks_t callbacks = {.on_sent = on_sent, .on_send_q_ovf = on_overflow};
    return i2s_channel_register_event_callback(tx, &callbacks, NULL);
}

static esp_err_t start_channel(void)
{
    /* Write alignment depends on every startup descriptor containing a full silence block. */
    for (unsigned i = 0; i < DESCRIPTORS; ++i) {
        size_t bytes = 0;
        esp_err_t result = i2s_channel_preload_data(tx, stereo, sizeof(stereo), &bytes);
        if (result != ESP_OK || bytes != sizeof(stereo)) {
            return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
        }
    }
    return i2s_channel_enable(tx);
}

static void play_track(uint32_t id, uint32_t offset)
{
    portENTER_CRITICAL(&lock);
    playing_id = id;
    sent_blocks = 0;
    overflow = false;
    starting_ms = offset;
    state = BEAT_AUDIO_PLAYING;
    portEXIT_CRITICAL(&lock);

    for (unsigned i = 0; i < 2 * FRAMES; ++i) {
        stereo[i] = 0;
    }
    esp_err_t result = start_channel();
    if (result == ESP_OK) {
        const uint32_t first = offset * (RATE / 1000);
        uint32_t cursor = first;
        uint32_t chirp_start = UINT32_MAX;
        const uint32_t last_block = DESCRIPTORS + (TRACK_FRAMES - first) / FRAMES;
        bool pa_on = false;
        for (;;) {
            portENTER_CRITICAL(&lock);
            const bool cancelled = requested != id;
            const bool lost_sync = overflow;
            const uint32_t completed = sent_blocks;
            if (!cancelled && chirp_pending) {
                const uint32_t target = chirp_ms * (RATE / 1000);
                chirp_start = target > cursor ? target : cursor;
                chirp_pending = false;
            }
            portEXIT_CRITICAL(&lock);
            if (cancelled || completed >= last_block || lost_sync) {
                if (lost_sync) {
                    result = ESP_ERR_TIMEOUT;
                }
                break;
            }
            if (!pa_on && completed >= 1) {
                amplifier(true);
                pa_on = true;
            }
            const int volume = sdgoods_audio_get_volume();
            for (unsigned i = 0; i < FRAMES; ++i) {
                int32_t mixed = 0;
                if (cursor < TRACK_FRAMES) {
                    mixed = song_start[cursor];
                    if (cursor >= chirp_start && cursor - chirp_start < beat_chirp_frames) {
                        mixed += beat_chirp[cursor - chirp_start];
                        mixed = mixed > MIX_PEAK ? MIX_PEAK : mixed < -MIX_PEAK ? -MIX_PEAK : mixed;
                    }
                    ++cursor;
                }
                const int16_t sample = (int16_t)(mixed * volume / 100);
                stereo[2 * i] = sample;
                stereo[2 * i + 1] = sample;
            }
            size_t bytes = 0;
            result = i2s_channel_write(tx, stereo, sizeof(stereo), &bytes, 100);
            if (result != ESP_OK || bytes != sizeof(stereo)) {
                if (result == ESP_OK) {
                    result = ESP_ERR_INVALID_SIZE;
                }
                break;
            }
        }
        amplifier(false);
        i2s_channel_disable(tx);
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Track playback stopped: %s", esp_err_to_name(result));
    }
    portENTER_CRITICAL(&lock);
    state = result == ESP_OK ? BEAT_AUDIO_DONE : BEAT_AUDIO_ERROR;
    portEXIT_CRITICAL(&lock);
}

static void audio_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        portENTER_CRITICAL(&lock);
        const uint32_t id = requested;
        const uint32_t offset = requested_ms;
        const bool start = play_requested && id != playing_id;
        portEXIT_CRITICAL(&lock);
        if (start) {
            play_track(id, offset);
        }
    }
}

void beat_audio_init(void)
{
    esp_err_t result = configure();
    if (result == ESP_OK && xTaskCreate(audio_task, "beat_audio", 3072, NULL, 6, &player) != pdPASS) {
        result = ESP_ERR_NO_MEM;
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Speaker unavailable: %s", esp_err_to_name(result));
        if (tx) {
            i2s_del_channel(tx);
            tx = NULL;
        }
    }
}

uint32_t beat_audio_start(uint32_t position_ms)
{
    portENTER_CRITICAL(&lock);
    const uint32_t id = ++requested;
    requested_ms = position_ms / BLOCK_MS * BLOCK_MS;
    play_requested = true;
    chirp_pending = false;
    portEXIT_CRITICAL(&lock);
    if (player) {
        xTaskNotifyGive(player);
    }
    return id;
}

void beat_audio_queue_chirp(uint32_t id, uint32_t position_ms)
{
    portENTER_CRITICAL(&lock);
    if (requested == id && play_requested) {
        chirp_ms = position_ms;
        chirp_pending = true;
    }
    portEXIT_CRITICAL(&lock);
}

void beat_audio_stop(void)
{
    portENTER_CRITICAL(&lock);
    ++requested;
    play_requested = false;
    portEXIT_CRITICAL(&lock);
}

beat_audio_state_t beat_audio_position(uint32_t id, uint32_t *position_ms)
{
    if (!player) {
        return BEAT_AUDIO_ERROR;
    }
    portENTER_CRITICAL(&lock);
    beat_audio_state_t result = BEAT_AUDIO_WAITING;
    if (playing_id == id) {
        result = state;
        if (state == BEAT_AUDIO_PLAYING && sent_blocks <= DESCRIPTORS) {
            result = BEAT_AUDIO_WAITING;
        }
        *position_ms = starting_ms + (sent_blocks > DESCRIPTORS ? sent_blocks - DESCRIPTORS : 0) * BLOCK_MS;
    }
    portEXIT_CRITICAL(&lock);
    return result;
}
