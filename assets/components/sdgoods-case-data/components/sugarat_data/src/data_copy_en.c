/* SPDX-License-Identifier: Apache-2.0 */
/** English text owned by the data application. */
#include "data_copy.h"
const sd_copy_t sd_copy_en = {
    .scenes = {"Temperature", "Device overview", "History", "Threshold alert"},
    .statuses = {"Waiting for data", "Data live", "Data expired", "Source offline", "Invalid data"},
    .language = "EN", .demo = "DEMO DATA", .external = "EXTERNAL DATA",
    .temperature = "Temperature", .humidity = "Humidity", .battery = "Battery",
    .normal = "In range", .alarm = "High temperature", .acknowledged = "Ack · pending recovery", .acknowledge = "Acknowledge",
    .range = "Range", .history = "Recent samples · gaps on loss", .empty = "No history yet",
    .threshold = "Alert", .recovery = "Clear", .monitoring = "Monitoring", .waiting = "Awaiting valid data",
};
