#include "CTFeedback.h"

CTFeedback::CTFeedback() {}

void CTFeedback::begin() {
    analogReadResolution(12);
    pinMode(ADC_GPIO, INPUT);

    gpio_config_t io = {};
    io.intr_type = GPIO_INTR_POSEDGE;
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = 1ULL << ZCD_GPIO;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        Serial.println(F("[CT] gpio_config fail"));
        return;
    }

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[CT] ISR service err %d\n", (int)err);
        return;
    }

    if (gpio_isr_handler_add(ZCD_GPIO, _zcdIsr, this) != ESP_OK) {
        Serial.println(F("[CT] ISR add fail"));
        return;
    }

    Serial.printf("[CT] ADC on %d, ZCD on %d\n", (int)ADC_GPIO, (int)ZCD_GPIO);
}

void IRAM_ATTR CTFeedback::_zcdIsr(void* arg) {
    auto* self = static_cast<CTFeedback*>(arg);
    uint32_t now = (uint32_t)esp_timer_get_time();
    self->_prevEdgeTimeUs = self->_lastEdgeTimeUs;
    self->_lastEdgeTimeUs = now;
    self->_edgeCount++;
    self->_hasSignal = true;
    __sync_synchronize();

    if (self->_prevEdgeTimeUs != 0) {
        uint32_t period = now - self->_prevEdgeTimeUs;
        if (period > 0)
            self->_tankFreq = 1000000.0f / (float)period;
    }
}

void CTFeedback::loop() {
    __sync_synchronize();
    uint32_t now = millis();

    uint32_t edges = _edgeCount;
    if (edges != _processedEdges) {
        _processedEdges = edges;
        _lastSignalMs = now;
    }

    if (_lastSignalMs == 0 || (uint32_t)(now - _lastSignalMs) > 500) {
        _hasSignal = false;
        if (_edgeCount != _processedEdges) _hasSignal = true;
    }

    if (now - _lastAdcMs < ADC_INTERVAL_MS) return;
    _lastAdcMs = now;

    int raw = analogRead(ADC_GPIO);
    _tankAmp = (float)raw / 4095.0f;
}
