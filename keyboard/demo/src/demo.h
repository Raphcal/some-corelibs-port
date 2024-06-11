//
//  demo.h
//  pd-keyboard
//
//  Created by Raphaël Calabro on 01/07/2023.
//

#ifndef demo_h
#define demo_h

#include "common.h"

#include "../../src/keyboard.h"

typedef struct {
    PDKeyboard * _Nullable keyboard;
    int keyboardIsAnimating;
    LCDFont * _Nullable font;
    char * _Nullable text;
    unsigned int length;
    float cameraX;
} Demo;

Demo * _Nonnull newDemo(void);

int demoUpdate(void * _Nonnull userdata);

#endif /* demo_h */
