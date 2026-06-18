#include "PLLController.h"
#include "CTFeedback.h"
#include "PWMManager.h"

PLLController::PLLController() {}

void PLLController::begin(CTFeedback* ct, PWMManager* pwm) {
    _ct = ct;
    _pwm = pwm;
    _state = IDLE;
    _freq = CoreParams::FREQ_MIN;
    _sweepFreq = CoreParams::FREQ_MAX;
    _lockCount = 0;
    _unlockCount = 0;
    _lastLoopUs = micros();
    _lastZcdUs = 0;
    _pwmSyncUs = 0;
    _integral = 0;
    _phaseError = 0;
    Serial.println(F("[PLL] ready"));
}

void PLLController::reset() {
    _state = IDLE;
    _freq = CoreParams::FREQ_MIN;
    _sweepFreq = CoreParams::FREQ_MAX;
    _lockCount = 0;
    _unlockCount = 0;
    _integral = 0;
    _phaseError = 0;
    _ct->disableZCD();
}

void PLLController::loop() {
    if (!_pwm || !_ct) return;
    uint32_t now = micros();
    if (now - _lastLoopUs < LOOP_INTERVAL_US) return;
    _lastLoopUs = now;

    if (!enabled) {
        if (_state != IDLE) {
            _state = IDLE;
            _integral = 0;
            _ct->disableZCD();
        }
        return;
    }

    bool pwmRunning = _pwm->isRunning();

    if (!pwmRunning && _state != IDLE) {
        _state = IDLE;
        _integral = 0;
        _lockCount = 0;
        _unlockCount = 0;
        _ct->disableZCD();
        return;
    }

    if (pwmRunning && _state == IDLE) {
        _state = SWEEPING;
        _sweepFreq = CoreParams::FREQ_MAX;
        _lockCount = 0;
        _unlockCount = 0;
        _ct->enableZCD();
        Serial.println(F("[PLL] SWEEP"));
    }

    bool doSweep = (now - _lastSweepUs >= SWEEP_INTERVAL_US);

    switch (_state) {
    case IDLE:
        return;

    case SWEEPING:
        if (doSweep) {
            _lastSweepUs = now;
            _sweepTick();
        }
        if (_ct->hasSignal()) {
            _lockCount++;
            if (_lockCount >= LOCK_THRESHOLD) {
                float tf = _ct->getTankFrequency();
                if (tf >= CoreParams::FREQ_MIN && tf <= CoreParams::FREQ_MAX) {
                    _freq = tf;
                    _pwm->setFrequency(_freq);
                }
                _pwmSyncUs = _ct->getLastEdgeTimeUs();
                _integral = 0;
                _state = PLL_LOCK;
                Serial.printf("[PLL] LOCK %.0f Hz\n", _freq);
            }
        } else {
            _lockCount = 0;
        }
        break;

    case PLL_LOCK: {
        uint32_t zcdUs = _ct->getLastEdgeTimeUs();
        if (zcdUs != _lastZcdUs) {
            _lastZcdUs = zcdUs;
            _pllTick(zcdUs);
        }
        if (!_ct->hasSignal()) {
            _unlockCount++;
            if (_unlockCount >= UNLOCK_THRESHOLD) {
                _state = PLL_UNLOCK;
                _sweepFreq = _freq;
                _unlockCount = 0;
                Serial.println(F("[PLL] UNLOCK"));
            }
        } else {
            _unlockCount = 0;
        }
        break;
    }

    case PLL_UNLOCK:
        if (_ct->hasSignal()) {
            _lockCount++;
            if (_lockCount >= LOCK_THRESHOLD) {
                float tf = _ct->getTankFrequency();
                if (tf >= CoreParams::FREQ_MIN && tf <= CoreParams::FREQ_MAX) {
                    _freq = tf;
                    _pwm->setFrequency(_freq);
                }
                _pwmSyncUs = _ct->getLastEdgeTimeUs();
                _integral = 0;
                _state = PLL_LOCK;
                Serial.printf("[PLL] RE-LOCK %.0f Hz\n", _freq);
            }
        } else {
            _lockCount = 0;
            _unlockCount++;
            if (_unlockCount >= UNLOCK_THRESHOLD * 3) {
                _state = SWEEPING;
                _sweepFreq = CoreParams::FREQ_MAX;
                _unlockCount = 0;
                Serial.println(F("[PLL] SWEEP"));
            }
        }
        break;
    }
}

void PLLController::_sweepTick() {
    _sweepFreq -= SWEEP_STEP_HZ;
    if (_sweepFreq < CoreParams::FREQ_MIN)
        _sweepFreq = CoreParams::FREQ_MAX;
    _pwm->setFrequency(_sweepFreq);
    _freq = _sweepFreq;
}

void PLLController::_pllTick(uint32_t zcdUs) {
    float periodUs = 1000000.0f / _freq;
    float elapsed = (float)(zcdUs - _pwmSyncUs);
    uint32_t cycles = (uint32_t)(elapsed / periodUs + 0.5f);
    if (cycles < 1) cycles = 1;

    float expectedUs = _pwmSyncUs + (float)cycles * periodUs;
    int32_t errUs = (int32_t)(zcdUs - expectedUs);
    float halfPeriod = periodUs * 0.5f;
    if (errUs > (int32_t)halfPeriod)       errUs -= (int32_t)(halfPeriod * 2.0f);
    if (errUs < -(int32_t)halfPeriod)      errUs += (int32_t)(halfPeriod * 2.0f);

    _phaseError = (float)errUs / halfPeriod;

    _integral += ki * _phaseError;
    _integral = constrain(_integral, -INTEGRAL_CLAMP, INTEGRAL_CLAMP);

    float correction = kp * _phaseError + _integral;
    correction = constrain(correction, -PI_CLAMP_HZ, PI_CLAMP_HZ);

    _freq += correction;
    _freq = constrain(_freq, CoreParams::FREQ_MIN, CoreParams::FREQ_MAX);

    _pwm->setFrequency(_freq);

    _pwmSyncUs = zcdUs;
}
