#pragma once
#include "Types.h"
#include <WebServer.h>

struct AppContext {
    CoreParams params;
    WebServer* apiServer = nullptr;
    bool restartPending = false;

    void requestRestart() { restartPending = true; }
    bool isRestartRequested() const { return restartPending; }
};

void executeRestart();
