//
//  demo.c
//  pd-keyboard
//
//  Created by Raphaël Calabro on 01/07/2023.
//

#include "demo.h"

static const int textLeft = 190;

static void keyboardAnimating(void * _Nonnull userdata) {
    Demo *self = userdata;
    self->cameraX = LCD_COLUMNS - keyboardApi.getLeft(self->keyboard);
    self->keyboardIsAnimating = 1;
}

static void textChanged(void * _Nonnull userdata) {
    Demo *self = userdata;
    keyboardApi.getText(self->keyboard, &self->text, &self->length);
}

Demo * _Nonnull newDemo(void) {
    Demo *self = playdate->system->realloc(NULL, sizeof(Demo));
    PDKeyboard *keyboard = keyboardApi.newKeyboard();
    const char *error = NULL;
    *self = (Demo) {
        .keyboard = keyboard,
        .font = playdate->graphics->loadFont("Asheville-Sans-14-Bold", &error),
    };
    if (error) {
        playdate->system->error("Unable to load font: %s", error);
    }
    keyboardApi.setPlaydateUpdateCallback(keyboard, demoUpdate, self);
    keyboardApi.setRefreshRate(keyboard, kRefreshRate);
    keyboardApi.setKeyboardAnimatingCallback(keyboard, keyboardAnimating, self);
    keyboardApi.setTextChangedCallback(keyboard, textChanged, self);
    return self;
}

int demoUpdate(void * _Nonnull userdata) {
    Demo *self = userdata;
    const int keyboardIsVisible = keyboardApi.isVisible(self->keyboard);

    playdate->graphics->clear(kColorWhite);
    playdate->graphics->setFont(self->font);
    playdate->graphics->setDrawMode(kDrawModeCopy);
    playdate->graphics->drawText("An input box:", 99, kASCIIEncoding, 32 - self->cameraX, 100);
    playdate->graphics->drawRect(textLeft - 4 - self->cameraX, 96, 180, 24, kColorBlack);

    if (self->length == 0 && !keyboardIsVisible) {
        playdate->graphics->drawText("Press A to edit", 99, kASCIIEncoding, textLeft - self->cameraX, 100);
    } else {
        if (keyboardIsVisible) {
            playdate->graphics->fillRect(textLeft - 4 - self->cameraX, 96, 180, 24, kColorBlack);
            playdate->graphics->setDrawMode(kDrawModeInverted);
        }
        playdate->graphics->drawText(self->text, self->length, kASCIIEncoding, textLeft - self->cameraX, 100);
    }

    if (!keyboardApi.isVisible(self->keyboard)) {
        PDButtons pressed;
        playdate->system->getButtonState(NULL, &pressed, NULL);
        if (pressed & kButtonA) {
            keyboardApi.show(self->keyboard, self->text, self->length);
        }
    } else if (!self->keyboardIsAnimating && (playdate->system->getCurrentTimeMilliseconds() % 1000 < 500)) {
        const int textWidth = playdate->graphics->getTextWidth(self->font, self->text, self->length, kASCIIEncoding, 0);
        const int carretLeft = textLeft - self->cameraX + textWidth + 4;
        playdate->graphics->fillRect(carretLeft, 98, 3, 20, kColorBlack);
    }

    self->keyboardIsAnimating = 0;
    return 1;
}
