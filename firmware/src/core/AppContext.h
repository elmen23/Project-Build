#pragma once
#include "Types.h"

struct AppContext {
    CoreParams params;
    bool restartPending = false;

    void requestRestart() { restartPending = true; }
    bool isRestartRequested() const { return restartPending; }
};

void executeRestart();
