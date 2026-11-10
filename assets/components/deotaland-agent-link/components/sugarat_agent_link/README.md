# Deotaland Agent Link

English | [中文](README.zh.md)

Install `deotaland-agent-link` with `target=esp-idf` under the ESP-IDF project root of the SDGOODS round badge. The installer adds its dependencies `https-client` and `sdgoods-audio`. Add `sugarat_agent_link` to `main/REQUIRES`. The component connects the device to a Deotaland Agent. It obtains an activation code, signs the device in, creates realtime-voice conversations and runs push-to-talk voice chat over Opus. The protocol follows the public Deotaland Agent_link device interface; the source is independent.

```c
/* app_main, after nvs_flash_init(), sugarat_audio_init() and before Wi-Fi starts */
ESP_ERROR_CHECK(sugarat_agent_cloud_start());
ESP_ERROR_CHECK(sugarat_agent_voice_init());
/* Wi-Fi event callback */
sugarat_agent_cloud_set_network(event == SUGARAT_WIFI_EVENT_CONNECTED);
/* LVGL timer: read status and drive the UI */
sugarat_agent_cloud_status_t cloud; sugarat_agent_cloud_status(&cloud);
if (cloud.phase == SUGARAT_AGENT_CLOUD_ONLINE && cloud.voice_supported) sugarat_agent_voice_open();
/* talk button */
sugarat_agent_voice_talk(pressed);
```

## Configuration

Set the product in `sdkconfig.defaults` or menuconfig under "Deotaland Agent Link". `CONFIG_SUGARAT_AGENT_LINK_PRODUCT_ID` is the product id registered on the Deotaland platform; the build fails while it is 0. `CONFIG_SUGARAT_AGENT_LINK_CHIP_TYPE` (default `esp32-s3`) must equal the chip registered for that product: a different value makes the platform refuse activation with `chipTypeMismatch`, reported as `SUGARAT_AGENT_CLOUD_REJECTED`. `CONFIG_SUGARAT_AGENT_LINK_BASE_URL` selects the gateway origin (default `https://bot.deotaland.cn`). Ask the human for the product id and its registered chip; never guess either value.

The firmware needs about 156 KiB of internal RAM for Wi-Fi, two TLS sessions and the display. Move network buffers and task stacks to PSRAM with `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`, `CONFIG_MBEDTLS_DYNAMIC_BUFFER`, `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` and `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`, and disable the Wi-Fi and LWIP IRAM optimizations. The `ip-agent` application's `sdkconfig.defaults` lists the complete set.

## Activation and sign-in

`sugarat_agent_cloud_start()` creates one worker that owns the platform session. The worker waits for `sugarat_agent_cloud_set_network(true)`. On first start it stores a random UUIDv4 serial in the `agent_link` NVS namespace and requests a six-digit activation code (`SUGARAT_AGENT_CLOUD_CODE`, renewed 30 s before it expires). The human enters the code in the Deotaland console. The worker polls bind status every 3 s and stores the returned device key in NVS. With a key, it authenticates and sends a heartbeat every 60 s. Phases are `SIGNING_IN`, `NO_AGENT` (claimed, no Agent bound; retried every 10 s), `ONLINE` and `FAULT` (backoff 3–60 s). `ALREADY_BOUND` means another account owns the device. `sugarat_agent_cloud_forget()` erases the key and requests a new code. A revoked key does the same automatically. `revision` increments on every change, and `agent_epoch` increments when the bound Agent changes. The request functions block on HTTPS; call them only from a worker task.

## Voice chat

`sugarat_agent_voice_init()` creates the uplink, control and receive tasks on PSRAM stacks. `sugarat_agent_voice_open()` creates a new conversation for the bound Agent; the newest existing one is reused when the platform refuses (`reused_conversation`). It also starts `sugarat_audio` streaming and connects the realtime WebSocket (`tidb-link.realtime.v1`, Opus 16 kHz up, Opus or PCM down) with a one-use grant. The microphone runs while the chat is open. `sugarat_agent_voice_talk(true)` starts a turn at once and includes the 300 ms before the press; `talk(false)` ends it. Releases within 250 ms are discarded (`SUGARAT_AGENT_VOICE_ERR_TOO_SHORT`). A press during a reply interrupts and flushes playback. Speech recorded before the link is ready, or during reconnection, is queued (up to 10 s) and sent when it is. A lost connection is replaced with a fresh grant after 1, 2, 4 and 8 s, then every 15 s (`RECONNECTING`, `FAILED`).

Turns use client push-to-talk endpointing. A turn without a reply for 20 s returns to idle with `SUGARAT_AGENT_VOICE_ERR_NO_REPLY`. When the platform refuses push-to-talk, or the first turn on a connection gets no reply, the session reconnects with server endpointing for the rest of the chat; repeat that turn. `turn` reports `LISTENING`, `THINKING` and `SPEAKING`. `SPEAKING` ends when playback has drained and the reply finished, or 4 s after reply audio stopped arriving. All functions return immediately and are safe from any task, including LVGL; status copies are lock-protected.

## Audio quality

The badge amplifier is designed for about a quarter of full scale, so reply audio passes a peak limiter at 9000 (gain recovers over about 0.4 s) and a [1 2 1] low-pass. `sugarat_audio` prebuffers 200 ms and rebuffers after running dry, so network jitter does not become clicks. Wi-Fi power save (`WIFI_PS_MIN_MODEM`) couples an audible buzz into the amplifier: an open chat sets `WIFI_PS_NONE` and closing restores `WIFI_PS_MIN_MODEM`. Limit transmit power with `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM=15`.

## Secrets

The device key, connect token and grant URL stay in RAM or NVS and never appear in logs. Logs carry phases, platform codes, conversation ids, frame counts and heap figures. NVS is not encrypted unless the deployment enables NVS encryption. Do not compile keys into firmware.

## Known limitations

Verified on an SDGOODS round badge with ESP-IDF 5.5.4 against a Deotaland product whose registered chip was ESP32-C3, which required reporting `esp32-c3`. Activation, binding, push-to-talk, playback without underruns or noise, and about 48 KiB of free internal heap with voice connected were observed. The component as packaged is verified by compilation; verify activation and voice on each new device. There is no OTA, text chat, wake word or acoustic echo cancellation. A full-duplex voice reply while the user is speaking is not supported. One chat runs at a time.
