//
//  main.c
//  pd-keyboard
//
//  Created by Raphaël Calabro on 30/06/2023.
//

#include "common.h"

#include "demo.h"

PlaydateAPI * _Nullable playdate;

const float kRefreshRate = 30.0f;

static void init(void) {
    playdate->display->setRefreshRate(kRefreshRate);
    playdate->graphics->setBackgroundColor(kColorWhite);

    Demo *demo = newDemo();
    playdate->system->setUpdateCallback(demoUpdate, demo);
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI * _Nonnull api, PDSystemEvent event, uint32_t arg) {
    switch (event) {
        case kEventInit:
            playdate = api;
            init();
            break;
        case kEventTerminate:
        case kEventLowPower:
            api->system->logToConsole("Terminate or low power event received: %d", event);
            break;
        case kEventPause:
            break;
        default:
            break;
    }
    return 0;
}

