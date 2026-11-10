# Sugarat Wi-Fi provisioning

English | [中文](README.zh.md)

An ESP-IDF 5.5.4 component for scan-based personal Wi-Fi setup. Use `wifi-provisioning` with `target=esp-idf` and the ESP-IDF project root as the installer directory. No SSID text entry or hard-coded credentials are supplied. The HTML page is embedded in the device and defaults to English; its language button switches to Chinese.

Initialize NVS using the application/BSP owner. Do not erase the partition on initialization errors. Add `sugarat_wifi` to the consuming component's `REQUIRES`. Obtain explicit configuration with `sugarat_wifi_default_config()`, then call `sugarat_wifi_create(&config, &wifi)`. The component owns default STA/AP netifs and the Wi-Fi driver; an existing radio manager is a conflict. An existing default event loop is reused. The application must delete dependent UI before `sugarat_wifi_destroy(wifi)`.

## Events and application integration

Set `config.on_event` and `config.user_data` before creation. Notifications execute on the network worker without its state mutex held, and may begin before `sugarat_wifi_create` returns. Prepare user data first. Copy the event if retaining it; it contains sequence, attempt, SSID, IP, saved/portal flags and error/reason codes, never a password. Keep callbacks bounded, do not call LVGL or destroy this owner from them, and handle busy responses from nonblocking command APIs. No callback remains after destroy returns. The round-screen UI supplies a separate LVGL-thread `on_connected` callback for navigation.

| Event | Meaning |
|---|---|
| `SUGARAT_WIFI_EVENT_CONNECTING` | A join or bounded retry started. |
| `SUGARAT_WIFI_EVENT_CONNECTED` | DHCP completed, credentials saved, and hotspot/DNS/HTTP closed; safe to enter the application. |
| `SUGARAT_WIFI_EVENT_DISCONNECTED` | The worker observed link loss; retries may follow. |
| `SUGARAT_WIFI_EVENT_FAILED` | A join failed or exhausted its retry/deadline budget. |
| `SUGARAT_WIFI_EVENT_STORAGE_FAILED` | Network connected, but credentials were not committed; setup stays available. |
| `SUGARAT_WIFI_EVENT_PORTAL_OPENED` | HTTP and captive DNS are ready. |
| `SUGARAT_WIFI_EVENT_PORTAL_CLOSED` | Setup services and the hotspot have stopped. |
| `SUGARAT_WIFI_EVENT_PORTAL_FAILED` | Hotspot/service shutdown failed; application-ready success is withheld. |

Snapshots retain `last_event` and `last_connected` for late consumers. A new join or link loss invalidates the retained connection notification. `last_connected` is published after teardown and has its own sequence; polling the same revision does not represent a new connection. Consumers opening Wi-Fi settings should ignore the already-seen connection. Treat save and portal failures separately from a bad password. Internet access is an application-owned check.

Set `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM` in the consuming project's `sdkconfig.defaults` to a value verified on its board and power supply. Sugarat Radio sets 8 dBm in sdkconfig.defaults; the component default is 20 dBm, with a 2–20 dBm range; the driver maps the limit to its supported power levels. The component applies the limit before its initial scan or saved-network reconnect and logs the driver readback in quarter-dBm units. Failure to apply or read the limit fails creation and releases acquired resources.

Creation automatically scans or reconnects to this component's saved network. `sugarat_wifi_snapshot` copies synchronized state; `sugarat_wifi_scan`, `sugarat_wifi_connect`, `sugarat_wifi_portal` and `sugarat_wifi_forget` queue worker operations without blocking the UI. Connection takes a scanned id, its generation and the password. Stale selections, unsupported security and invalid password lengths are rejected. Busy requests must wait for a new snapshot before retrying. At most 16 distinct visible SSIDs are shown, using the strongest scanned access point for each. Hidden networks, WEP/enterprise and 64-digit raw PSKs are unsupported; personal passwords must contain 8–63 bytes. ESP32-S3 supports 2.4 GHz. Network selection identifies an SSID, so the driver scans all channels and can use another access point with that SSID instead of retaining a BSSID lock. WPA3 SAE supports both PWE methods. Default logs include disconnect reason codes without passwords.

The portal opens only on request. The device screen shows the open hotspot name. Connect a phone without entering a hotspot password, then open `http://192.168.4.1/`. The page automatically scans; users select the network and enter only its password. The page has no automatic input focus, uses 17px input text and requests a fixed viewport scale. HTTP access is limited to the SoftAP subnet, including IPv4 clients represented as IPv4-mapped IPv6 peers by the dual-stack server. Mutation requests require a per-session token and same-origin JSON. The server provides no CORS access, external assets, analytics, password readback or credential logging. Keep the phone connected even if it reports no Internet. While the portal is open, DHCP advertises hotspot DNS, IPv4 DNS answers point to the hotspot, and HTTP detection requests redirect to the canonical setup URL with a response body. This supports captive-network detection on iOS and other clients; the operating system controls whether its login window appears. HTTPS is not intercepted. DNS is bound to the hotspot interface and removed synchronously on the TCP/IP thread when the portal closes.

Status polling, language changes and rescanning retain the selected SSID and password draft when that network remains available with the same security. Selecting a different network or losing the selection clears the draft. A connect response carries an attempt receipt; status reports the last DHCP-completed receipt. The page clears a submitted password only when that receipt has connected and saved successfully, so an existing connection or a failed attempt cannot erase new input. Drafts remain only in page memory.

The idle portal expires after 15 minutes. A successful join saves credentials, switches the radio to STA-only immediately, removes DNS and HTTP, and then publishes CONNECTED. There is no success hold timer. This closes only the setup hotspot; the router connection remains active. Save failure leaves setup available and publishes STORAGE_FAILED. Shutdown failure publishes PORTAL_FAILED and does not publish CONNECTED. iOS owns dismissal of its captive window; an ordinary Safari tab cannot be forcibly closed. Because hotspot removal can precede the final HTTP poll, the page directs the user to the device screen when a submitted connection loses transport without a final result. Default joins have a 20-second deadline with three bounded reconnect attempts. After DHCP, credentials are committed under NVS namespace `sugarat_wifi`; a failed replacement leaves the last saved configuration intact. A connected-but-not-saved outcome is visible. Forget removes only this namespace's station key. NVS credential storage is not automatically encrypted; applications that require encryption must configure an appropriate NVS security scheme without blindly changing eFuses or partition layouts.

Radio, HTTP and event-loop resources are released by destroy. Partial initialization releases acquired resources. Connection success means the station received a local IP, not that Internet services are reachable. Compile checks do not establish real-device behavior. Verify both setup paths, wrong-password recovery, reboot persistence, portal expiry and the target's radio performance on hardware.

Implementation references: [ESP-IDF Wi-Fi driver](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-guides/wifi.html), [Espressif provisioning overview](https://developer.espressif.com/blog/2026/05/simple-provisioning/).

The Sugarat Radio copy adds `CONFIG_SUGARAT_WIFI_US_CHANNELS` (enabled by default). It fixes US channels 1–11 with a manual country policy before the first scan or reconnect.
