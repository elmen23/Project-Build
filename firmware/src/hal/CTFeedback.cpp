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
    gpio_config(&io);

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[CT] ISR service err %d\n", (int)err);
    }

    Serial.printf("[CT] ADC on %d, ZCD on %d (disabled)\n",
        (int)ADC_GPIO, (int)ZCD_GPIO);
}

void CTFeedback::setEnabled(bool v) {
    _enabled = v;
    if (!v) disableZCD();
}

void CTFeedback::enableZCD() {
    if (!_enabled) return;
    gpio_isr_handler_add(ZCD_GPIO, _zcdIsr, this);
    Serial.println(F("[CT] ZCD enabled"));
}

void CTFeedback::disableZCD() {
    gpio_isr_handler_remove(ZCD_GPIO);
    _lastEdgeTimeUs = 0;
    _prevEdgeTimeUs = 0;
    _lastPeriod = 0;
    _edgeCount = 0;
    _processedEdges = 0;
    _lastSignalMs = 0;
    _hasSignal = false;
    Serial.println(F("[CT] ZCD disabled"));
}

void IRAM_ATTR CTFeedback::_zcdIsr(void* arg) {
    auto* self = static_cast<CTFeedback*>(arg);
    uint32_t now = (uint32_t)esp_timer_get_time();
    self->_prevEdgeTimeUs = self->_lastEdgeTimeUs;
    self->_lastEdgeTimeUs = now;
    self->_edgeCount++;
    self->_hasSignal = true;
    if (self->_prevEdgeTimeUs != 0) {
        self->_lastPeriod = now - self->_prevEdgeTimeUs;
    }
}

void CTFeedback::loop() {
    if (!_enabled) return;

    __sync_synchronize();
    uint32_t now = millis();

    uint32_t period = _lastPeriod;
    if (period > 0 && period < 100000)
        _tankFreq = 1000000.0f / (float)period;

    uint32_t edges = _edgeCount;
    if (edges != _processedEdges) {
        _processedEdges = edges;
        _lastSignalMs = now;
    }

    if ((uint32_t)(now - _lastSignalMs) > 500) {
        _hasSignal = false;
        if (_edgeCount != _processedEdges) _hasSignal = true;
    }

    if (now - _lastAdcMs < ADC_INTERVAL_MS) return;
    _lastAdcMs = now;

    int raw = analogRead(ADC_GPIO);
    _tankAmp = (float)raw / 4095.0f;
}
