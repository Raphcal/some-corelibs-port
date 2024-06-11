//
//  keyboard.h
//  Kuroobi
//
//  Created by Raphaël Calabro on 23/06/2023.
//

#ifndef keyboard_h
#define keyboard_h

#include "pd_api.h"

extern PlaydateAPI * _Nullable playdate;

typedef struct pdkeyboard PDKeyboard;

typedef enum {
    kCapitalizationNormal,
    kCapitalizationWords,
    kCapitalizationSentences,
} PDKeyboardCapitalization;

typedef void PDKeyboardCallback(void * _Nullable userdata);
typedef void PDKeyboardWillHideCallback(int okButtonPressed, void * _Nullable userdata);

struct pd_keyboard {
    PDKeyboard * _Nonnull (* _Nonnull newKeyboard)(void);
    void (* _Nonnull freeKeyboard)(PDKeyboard * _Nonnull keyboard);

    void (* _Nonnull setPlaydateUpdateCallback)(PDKeyboard * _Nonnull keyboard, PDCallbackFunction * _Nonnull playdateUpdate, void * _Nullable userdata);
    void (* _Nonnull setRefreshRate)(PDKeyboard * _Nonnull keyboard, float refreshRate);

    void (* _Nonnull show)(PDKeyboard * _Nonnull keyboard, const char * _Nullable newText, const unsigned int newTextLength);
    void (* _Nonnull hide)(PDKeyboard * _Nonnull keyboard);

    /**
     * Allocates a buffer and retrieves the entered text. The caller is responsible for freeing <em>text</em>.
     */
    void (* _Nonnull getText)(PDKeyboard * _Nonnull keyboard, char * _Nonnull * _Nullable text, unsigned int * _Nullable count);

    int (* _Nonnull isVisible)(PDKeyboard * _Nonnull keyboard);
    int (* _Nonnull getWidth)(PDKeyboard * _Nonnull keyboard);
    int (* _Nonnull getLeft)(PDKeyboard * _Nonnull keyboard);

    PDKeyboardCapitalization (* _Nonnull getCapitalizationBehavior)(PDKeyboard * _Nonnull keyboard);
    void (* _Nonnull setCapitalizationBehavior)(PDKeyboard * _Nonnull keyboard, PDKeyboardCapitalization capitalizationBehavior);

    void (* _Nonnull setKeyboardDidShowCallback)(PDKeyboard * _Nonnull keyboard, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata);
    void (* _Nonnull setKeyboardDidHideCallback)(PDKeyboard * _Nonnull keyboard, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata);
    void (* _Nonnull setKeyboardWillHideCallback)(PDKeyboard * _Nonnull keyboard, PDKeyboardWillHideCallback * _Nullable callback, void * _Nullable userdata);
    void (* _Nonnull setKeyboardAnimatingCallback)(PDKeyboard * _Nonnull keyboard, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata);
    void (* _Nonnull setTextChangedCallback)(PDKeyboard * _Nonnull keyboard, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata);
};

extern const struct pd_keyboard keyboardApi;

#endif /* keyboard_h */
