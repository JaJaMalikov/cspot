#pragma once
#include "esp_log.h"

#define BELL_LOG(level, tag, fmt, ...) BELL_LOG_##level(tag, fmt, ##__VA_ARGS__)
#define BELL_LOG_info(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define BELL_LOG_error(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define BELL_LOG_warn(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define BELL_LOG_debug(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
