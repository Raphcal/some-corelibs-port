# Keyboard

**PDKeyboard\* keyboardApi.newKeyboard(void);**  
Allocates and returns a new keyboard.

**void keyboardApi.freeKeyboard(PDKeyboard\* keyboard);**  
Frees the given *keyboard*.

**void keyboardApi.setPlaydateUpdateCallback(PDKeyboard\* keyboard, PDCallbackFunction\* playdateUpdate, void\* userdata);**  
C API does not provides a way to get the update callback so this method is necessary for the keyboard to call the main update function.

**void keyboardApi.setRefreshRate(PDKeyboard\* keyboard, float refreshRate);**  
C API does not provides a way to get the current refresh rate so this method is necessary to calculate the animation frame count.

**void keyboardApi.show(PDKeyboard\* keyboard, const char\* text, const unsigned int textLength);**  
Opens the keyboard. Input is not taken over, you have to manually check if the keyboard is visible to avoid conflicts.

`text`, if provided, will be used to set the initial text value of the keyboard.

**void keyboardApi.hide(PDKeyboard\* keyboard);**  
Hides the keyboard.

**void keyboardApi.getText(PDKeyboard\* keyboard, char\*\* text, unsigned int\* count);**  
Realloc *text* and copy the current text. Note that the caller is responsible for freeing *text*.

**int keyboardApi.isVisible(PDKeyboard\* keyboard);**  
Returns 1 if the keyboard is currently being shown, otherwise 0.

**int keyboardApi.getWidth(PDKeyboard\* keyboard);**  
Returns the pixel width of the keyboard.

**int keyboardApi.getLeft(PDKeyboard\* keyboard);**  
Returns the current x location of the left edge of the keyboard.

**void keyboardApi.setCapitalizationBehavior(PDKeyboard\* keyboard, PDKeyboardCapitalization capitalizationBehavior);**  
*behavior* should be one of the constants *kCapitalizationNormal*, *kCapitalizationWords*, or *kCapitalizationSentences*.

In the case of *kCapitalizationWords*, the keyboard selection will automatically move to the upper case column after a space is entered. For *kCapitalizationSentences* the selection will automatically move to the upper case column after a period and a space have been entered.

**PDKeyboardCapitalization keyboardApi.getCapitalizationBehavior(PDKeyboard\* keyboard);**  
Returns the current capitalisation behavior.

**void keyboardApi.setKeyboardDidShowCallback(PDKeyboard\* keyboard, PDKeyboardCallback\* callback, void\* userdata);**  
If set, this function will be called with the given *userdata* when the keyboard has finished the opening animation.

**void keyboardApi.setKeyboardWillHideCallback(PDKeyboard\* keyboard, PDKeyboardWillHideCallback\* callback, void\* userdata);**  
If set, this function will be called with the given *userdata* when the keyboard starts to close. An integer argument will be passed to the callback, `1` if the user selected "OK" close the keyboard, `0` otherwise.

**void keyboardApi.setKeyboardDidHideCallback(PDKeyboard\* keyboard, PDKeyboardCallback\* callback, void\* userdata);**  
If set, this function will be called with the given *userdata* when the keyboard has finished the hide animation.

**void keyboardApi.setKeyboardAnimatingCallback(PDKeyboard\* keyboard, PDKeyboardCallback\* callback, void\* userdata);**  
If set, this function is called with the given *userdata* as the keyboard animates open or closed. Provided as a way to sync animations with the keyboard movement.

**void keyboardApi.setTextChangedCallback(PDKeyboard\* keyboard, PDKeyboardCallback\* callback, void\* userdata);**  
If set, this function will be called with the given *userdata* every time a character is entered or deleted.
