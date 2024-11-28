//
//  keyboard.c
//  Kuroobi
//
//  Created by Raphaël Calabro on 23/06/2023.
//

#include "keyboard.h"

typedef int bool_t;
#define false 0
#define true 1

typedef enum {
    kAnimationTypeNone,
    kAnimationTypeKeyboardShow,
    kAnimationTypeKeyboardHide,
    kAnimationTypeSelectionUp,
    kAnimationTypeSelectionDown,
} PDKeyboardAnimationType;

typedef enum {
    kColumnSymbols,
    kColumnUpper,
    kColumnLower,
    kColumnMenu,
} PDKeyboardColumn;
#define kColumnCount 4

typedef enum {
    kMenuOptionSpace,
    kMenuOptionOK,
    kMenuOptionDelete,
    kMenuOptionCancel,
} PDKeyboardMenuOption;

typedef enum {
    kJiggleLeft = -1,
    kJiggleRight = 1
} PDKeyboardJiggleDirection;

typedef struct {
    char * _Nullable data;
    unsigned int count;
} PDKeyboardText;

typedef struct {
    PDKeyboardText super;
    unsigned int capacity;
} PDKeyboardMutableText;

struct size {
    float width;
    float height;
};
struct point {
    float x;
    float y;
};
struct rectangle {
    struct point origin;
    struct size size;
};

struct int_size {
    int width;
    int height;
};

typedef struct pdkeyboard {
    PDKeyboardMutableText text;
    PDKeyboardText originalText;
    bool_t okButtonPressed;
    float degreesSinceClick;

    PDKeyboardCapitalization capitalizationBehavior;

    PDKeyboardColumn selectedColumn;
    PDKeyboardColumn lastTypedColumn;
    int8_t selectionIndexes[kColumnCount];

    bool_t isVisible;
    bool_t justOpened;

    PDKeyboardAnimationType currentAnimationType;
    unsigned int animationDuration;
    unsigned int animationStartTime;

    float refreshRate;
    int8_t frameRateAdjustedScrollRepeatDelay;

    int8_t columnJiggle;
    int8_t rowJiggle;
    int8_t rowShift;

    int8_t selectionY;
    struct rectangle keyboardRect;
    struct rectangle selectedCharacterRect;

    PDKeyboardCallback * _Nullable textChangedCallback;
    PDKeyboardWillHideCallback * _Nullable keyboardWillHideCallback;
    PDKeyboardCallback * _Nullable keyboardDidShowCallback;
    PDKeyboardCallback * _Nullable keyboardDidHideCallback;
    PDKeyboardCallback * _Nullable keyboardAnimatingCallback;

    void * _Nullable textChangedCallbackUserdata;
    void * _Nullable keyboardWillHideCallbackUserdata;
    void * _Nullable keyboardDidShowCallbackUserdata;
    void * _Nullable keyboardDidHideCallbackUserdata;
    void * _Nullable keyboardAnimatingCallbackUserdata;

    PDCallbackFunction * _Nullable playdateUpdate;
    void * _Nullable playdateUpdateUserdata;

    bool_t scrollingVertically;
    int8_t scrollRepeatDelay;

    int8_t keyRepeatDelay;

    unsigned int lastKeyEnteredTime;

    // Animation Variables
    float selectionYOffset;
    float selectionStartY;

    // Sounds
    SamplePlayer * _Nullable samplePlayer;
    AudioSample * _Nullable columnNextSound;
    AudioSample * _Nullable columnPreviousSound;
    AudioSample * _Nullable rowSound;
    AudioSample * _Nullable bumpSound;
    AudioSample * _Nullable keySound;
} PDKeyboard;

static void selectColumn(PDKeyboard * _Nonnull self, PDKeyboardColumn column);
static void moveSelectionDown(PDKeyboard * _Nonnull self, int count, bool_t shiftRow);
static void moveSelectionUp(PDKeyboard * _Nonnull self, int count, bool_t shiftRow);

static void PDKeyboardMutableTextPush(PDKeyboardMutableText * _Nonnull self, char letter);
static void PDKeyboardMutableTextEnsureCapacity(PDKeyboardMutableText * _Nonnull self, int required);
static void PDKeyboardMutableTextGrow(PDKeyboardMutableText * _Nonnull self, int newSize);

static void PDKeyboardHide(PDKeyboard * _Nonnull self);


#pragma mark - Constants

static const float displayWidth = LCD_COLUMNS;
static const float displayHeight = LCD_ROWS;

static LCDFont * _Nullable keyboardFont;
static LCDBitmap * _Nullable menuImageSpace;
static LCDBitmap * _Nullable menuImageOK;
static LCDBitmap * _Nullable menuImageDelete;
static LCDBitmap * _Nullable menuImageCancel;

static float fontHeight;
static const char lowerColumn[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
static const char upperColumn[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
static const char numbersColumn[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.', ',', ':', ';', '<', '=', '>', '?', '!', '\'', '"', '#', '$', '%', '&', '(', ')', '*', '+', '-', '/', '|', '\\', '[', ']', '^', '_', '`', '{', '}', '~', '@'};
#define kMenuColumnCount 4
static LCDBitmap * _Nullable menuColumn[kMenuColumnCount];

static const void * _Nonnull columns[] = {numbersColumn, upperColumn, lowerColumn, menuColumn};
static unsigned int columnCounts[] = {sizeof(numbersColumn), sizeof(upperColumn), sizeof(lowerColumn), 4};

#define rightMargin 8.0f
#define standardColumnWidth 36.0f
#define menuColumnWidth 50.0f
#define leftMargin 12.0f

static const float keyboardWidth = rightMargin + (standardColumnWidth * 3) + menuColumnWidth + leftMargin;

static const float columnWidths[] = {standardColumnWidth, standardColumnWidth, standardColumnWidth, menuColumnWidth};

#define p1 leftMargin
#define p2 (p1 + standardColumnWidth)
#define p3 (p2 + standardColumnWidth)
#define p4 (p3 + standardColumnWidth)

static const float columnPositions[] = {p1, p2, p3, p4};

static const float rowHeight = 38.0f;

// this is used for debouncing, I believe we were getting double letter entry at one point in initial wifi password setup
static const unsigned int minKeyRepeatMilliseconds = 100;

static int max(int lhs, int rhs) {
    return lhs > rhs ? lhs : rhs;
}

#pragma mark - Keyboard Input Handler

static const float clickDegrees = 360.0f / 15.0f;

static void keyboardInputCranked(PDKeyboard * _Nonnull self, float change) {
    const float acceleratedChange = change * (1.0f / (0.2f + powf(1.04f, -fabsf(change) + 20.0f)));
    const float degreesSinceClick = self->degreesSinceClick + acceleratedChange;

    if (degreesSinceClick > clickDegrees) {
        const int clickCount = floorf(degreesSinceClick / clickDegrees);
        self->degreesSinceClick = 0.0f;
        moveSelectionDown(self, clickCount, false);
    } else if (degreesSinceClick < -clickDegrees) {
        const int clickCount = ceilf(-degreesSinceClick / clickDegrees);
        self->degreesSinceClick = 0.0f;
        moveSelectionUp(self, clickCount, false);
    } else {
        self->degreesSinceClick = degreesSinceClick;
    }
}


#pragma mark - Easing Functions

static float linearEase(float t, float b, float c, float d) {
    return c * t / d + b;
}

static float outBackEase(float t, float b, float c, float d, float s) {
    if (s == 0) {
        s = 1.70158f;
    }
    t = t / d - 1.0f;
    return floorf(c * (t * t * ((s + 1) * t + s) + 1) + b);
}

#pragma mark - Sounds

static const char kSoundColumnMoveNext[] = "CoreLibs/assets/sfx/selection";
static const char kSoundColumnMovePrevious[] = "CoreLibs/assets/sfx/selection-reverse";
static const char kSoundRowMove[] = "CoreLibs/assets/sfx/click";
static const char kSoundBump[] = "CoreLibs/assets/sfx/denial";
static const char kSoundKeyPress[] = "CoreLibs/assets/sfx/key";

static AudioSample * _Nonnull newSampleOrError(const char * _Nonnull path) {
    AudioSample *sample = playdate->sound->sample->load(path);
    if (sample == NULL) {
        playdate->system->error("Unable to load sample at path: %s", path);
    }
    return sample;
}

static void playSound(SamplePlayer * _Nonnull * _Nullable samplePlayer, AudioSample * _Nonnull * _Nullable sample, const char * _Nonnull path) {
    AudioSample *theSample = *sample;
    if (theSample == NULL) {
        theSample = newSampleOrError(path);
        *sample = theSample;
    }
    SamplePlayer *theSamplePlayer = *samplePlayer;
    if (theSamplePlayer == NULL) {
        theSamplePlayer = playdate->sound->sampleplayer->newPlayer();
        *samplePlayer = theSamplePlayer;
    }
    playdate->sound->sampleplayer->setSample(theSamplePlayer, theSample);
    playdate->sound->sampleplayer->play(theSamplePlayer, 1, 1.0f);
}

static void freeSounds(PDKeyboard * _Nonnull self) {
    if (self->samplePlayer) {
        playdate->sound->sampleplayer->freePlayer(self->samplePlayer);
        self->samplePlayer = NULL;
    }
    if (self->columnNextSound) {
        playdate->sound->sample->freeSample(self->columnNextSound);
        self->columnNextSound = NULL;
    }
    if (self->columnPreviousSound) {
        playdate->sound->sample->freeSample(self->columnPreviousSound);
        self->columnPreviousSound = NULL;
    }
    if (self->rowSound) {
        playdate->sound->sample->freeSample(self->rowSound);
        self->rowSound = NULL;
    }
    if (self->bumpSound) {
        playdate->sound->sample->freeSample(self->bumpSound);
        self->bumpSound = NULL;
    }
    if (self->keySound) {
        playdate->sound->sample->freeSample(self->keySound);
        self->keySound = NULL;
    }
}

#pragma mark - Graphics

/// @brief Draw a rounded rect with a radius of 2.
/// @param rectangle Rectangle to draw.
static void fillRoundRect(struct rectangle rectangle, LCDColor color) {
    if (rectangle.size.width < 4 || rectangle.size.height < 4) {
        playdate->graphics->fillRect(rectangle.origin.x, rectangle.origin.y, rectangle.size.width, rectangle.size.height, color);
        return;
    }
    playdate->graphics->drawLine(rectangle.origin.x + 2, rectangle.origin.y,
                                 rectangle.origin.x + rectangle.size.width - 3, rectangle.origin.y,
                                 1, color);
    playdate->graphics->drawLine(rectangle.origin.x + 1, rectangle.origin.y + 1,
                                 rectangle.origin.x + rectangle.size.width - 2, rectangle.origin.y + 1,
                                 1, color);
    playdate->graphics->fillRect(rectangle.origin.x, rectangle.origin.y + 2, rectangle.size.width, rectangle.size.height - 4, color);
    playdate->graphics->drawLine(rectangle.origin.x + 1, rectangle.origin.y + rectangle.size.height - 2,
                                 rectangle.origin.x + rectangle.size.width - 2, rectangle.origin.y + rectangle.size.height - 2,
                                 1, color);
    playdate->graphics->drawLine(rectangle.origin.x + 2, rectangle.origin.y + rectangle.size.height - 1,
                                 rectangle.origin.x + rectangle.size.width - 3, rectangle.origin.y + rectangle.size.height - 1,
                                 1, color);
}

#pragma mark - Draw

static bool_t isShowOrHideAnimation(PDKeyboardAnimationType animationType) {
    return animationType == kAnimationTypeKeyboardShow || animationType == kAnimationTypeKeyboardHide;
}

static void drawKeyboard(PDKeyboard * _Nonnull self) {
    // playdate->graphics->pushContext();
    const struct playdate_graphics gfx = *playdate->graphics;

    const PDKeyboardAnimationType currentAnimationType = self->currentAnimationType;
    const bool_t animating = isShowOrHideAnimation(currentAnimationType);

    const struct rectangle keyboardRect = self->keyboardRect;
    int columnOffsets[kColumnCount];

    if (!animating) {
        for (unsigned int index = 0; index < kColumnCount; index++) {
            columnOffsets[index] = keyboardRect.origin.x + columnPositions[index];
        }
    } else {
        const float progress = keyboardRect.size.width / keyboardWidth;
        for (unsigned int index = 0; index < kColumnCount; index++) {
            columnOffsets[index] = keyboardRect.origin.x + (columnPositions[index] * progress);
        }
    }

    const float leftX = keyboardRect.origin.x;

    // background
    gfx.setDrawMode(kDrawModeCopy);
    gfx.fillRect(leftX + 2, 0, displayWidth - leftX, displayHeight, kColorBlack);
    gfx.fillRect(leftX, 0, 2, displayHeight, kColorWhite);

    // selection
    struct rectangle selectedRect = self->selectedCharacterRect;
    selectedRect.origin.x += leftX;
    switch (currentAnimationType) {
        case kAnimationTypeSelectionUp:
            selectedRect.origin.y += 3;
            break;
        case kAnimationTypeSelectionDown:
            selectedRect.origin.y -= 3;
            break;
        default:
            break;
    }

    const int8_t rowShift = self->rowShift;
    if (rowShift > 0) {
        selectedRect.origin.y += 5;
        self->rowShift = rowShift - 1;
    } else if (rowShift < 0) {
        selectedRect.origin.y -= 5;
        self->rowShift = rowShift + 1;
    }

    const int8_t rowJiggle = self->rowJiggle;
    if (rowJiggle > 0) {
        selectedRect.origin.y -= 3;
        selectedRect.size.height += 2;
        self->rowJiggle = rowJiggle - 1;
    } else if (rowJiggle < 0) {
        selectedRect.origin.y += 1;
        selectedRect.size.height += 2;
        self->rowJiggle = rowJiggle + 1;
    }

    const int8_t columnJiggle = self->columnJiggle;
    if (columnJiggle > 0) {
        selectedRect.origin.x += 1;
        selectedRect.size.width += 2;
        self->columnJiggle = columnJiggle - 1;
    } else if (columnJiggle < 0) {
        selectedRect.origin.x -= 3;
        selectedRect.size.width += 2;
        self->columnJiggle = columnJiggle + 1;
    }


    if (!animating) {
        fillRoundRect(selectedRect, kColorWhite);
    }


    gfx.setDrawMode(kDrawModeNXOR);    


    // menu column
    const PDKeyboardColumn selectedColumn = self->selectedColumn;
    const uint8_t selectedMenuIndex = self->selectionIndexes[kColumnMenu];
    const float w = columnWidths[kColumnMenu];
    const float y = self->selectionY - (selectedMenuIndex * rowHeight) + rowHeight;
    const float x = columnOffsets[kColumnMenu];
    float yOffset = 0;
    if (selectedColumn == kColumnMenu) {
        yOffset = self->selectionYOffset;
    }

    const float cx = x + menuColumnWidth / 2;
    float cy = y - rowHeight/2 + yOffset;

    if (animating) {
        gfx.setDrawMode(kDrawModeCopy);
        gfx.fillRect(x, 0, w, displayHeight, kColorBlack);
        gfx.setDrawMode(kDrawModeNXOR);
        
        if (selectedColumn == kColumnMenu) {
            selectedRect.origin.x = x;
            fillRoundRect(selectedRect, kColorWhite);
        }
    }

    for (unsigned int index = 0; index < kMenuColumnCount; index++) {
        LCDBitmap *glyphImage = menuColumn[index];
        struct int_size size;
        gfx.getBitmapData(glyphImage, &size.width, &size.height, NULL, NULL, NULL);
        gfx.drawBitmap(glyphImage, cx - size.width / 2, cy - size.height / 2, kBitmapUnflipped);
        cy += rowHeight;
    }
    
    // letter/symbol columns

    playdate->graphics->setFont(keyboardFont);
    for (unsigned int index = 0; index < kColumnMenu; index++) {
        const float w = columnWidths[index];
        int y = self->selectionY;
        int y2 = y;
        const int x = columnOffsets[index];
        float yOffset = 0;

        if (index == selectedColumn
            || (selectedColumn == kColumnLower && index == kColumnUpper)
            || (selectedColumn == kColumnUpper && index == kColumnLower)) {
            // while scrolling vertically, don't offset, instead center letters on selection rect - easier to read and looks better
            if (!self->scrollingVertically) {
                yOffset = self->selectionYOffset;
            }
        }
        
        if (animating) {
            gfx.setDrawMode(kDrawModeCopy);
            gfx.fillRect(x, 0, w, displayHeight, kColorBlack);
            gfx.setDrawMode(kDrawModeNXOR);
            
            if (index == selectedColumn) {
                selectedRect.origin.x = x;
                fillRoundRect(selectedRect, kColorWhite);
            }
        }

        const char *charColumn = columns[index];
        const int selectedIndex = self->selectionIndexes[index];
        char glyph = charColumn[selectedIndex];
        int cx = x;
        int cy = y + 4 + yOffset;

        playdate->graphics->drawText(&glyph, 1, kASCIIEncoding, cx, cy);

        // letters above
        int j = 0;
        while (y2 + rowHeight + yOffset > fontHeight) {
            j += 1;
            y2 -= rowHeight;
            cy = y2 + 4 + yOffset;

            glyph = charColumn[(selectedIndex - j + columnCounts[index]) % (columnCounts[index])];
            playdate->graphics->drawText(&glyph, 1, kASCIIEncoding, cx, cy);
        }

        // letters below
        j = 0;
        while (y + rowHeight + yOffset < displayHeight) {
            j += 1;
            y += rowHeight;
            cy = y + 4 + yOffset;
            
            glyph = charColumn[(selectedIndex + j) % (columnCounts[index])];
            playdate->graphics->drawText(&glyph, 1, kASCIIEncoding, cx, cy);
            
        }
    }
}

#pragma mark Text

static void PDKeyboardTextFree(PDKeyboardText * _Nonnull self) {
    if (self->data) {
        free(self->data);
        self->data = NULL;
        self->count = 0;
    }
}

#pragma mark - Mutable Text

static void PDKeyboardMutableTextFree(PDKeyboardMutableText * _Nonnull self) {
    if (self->super.data) {
        free(self->super.data);
        self->super.data = NULL;
        self->super.count = 0;
        self->capacity = 0;
    }
}

static void PDKeyboardMutableTextPush(PDKeyboardMutableText * _Nonnull self, char letter) {
    PDKeyboardMutableTextEnsureCapacity(self, self->super.count + 1);
    self->super.data[self->super.count++] = letter;
}

static void PDKeyboardMutableTextEnsureCapacity(PDKeyboardMutableText * _Nonnull self, int required) {
    const unsigned int capacity = self->capacity;
    if (capacity < required) {
        const unsigned int newCapacity = capacity == 0 ? 8 : capacity + (capacity * 2) + 1;
        PDKeyboardMutableTextGrow(self, newCapacity > required ? newCapacity : required);
    }
}

static void PDKeyboardMutableTextGrow(PDKeyboardMutableText * _Nonnull self, int newSize) {
    self->super.data = playdate->system->realloc(self->super.data, newSize * sizeof(char));
    self->capacity = newSize;
}

#pragma mark - Menu Commands

static void hideKeyboard(PDKeyboard * _Nonnull self, bool_t okPressed) {
    self->okButtonPressed = okPressed;
    PDKeyboardHide(self);

    // free up memory
    freeSounds(self);
    PDKeyboardTextFree(&self->originalText);
}


static void deleteAction(PDKeyboard * _Nonnull self) {
    if (self->text.super.count == 0) {
        return;
    }
    self->text.super.count = self->text.super.count - 1;
    if (self->textChangedCallback) {
        self->textChangedCallback(self->textChangedCallbackUserdata);
    }
}


static void cancelAction(PDKeyboard * _Nonnull self) {
    memcpy(self->text.super.data, self->originalText.data, sizeof(char) * self->originalText.count);
    self->text.super.count = self->originalText.count;

    if (self->textChangedCallback) {
        self->textChangedCallback(self->textChangedCallbackUserdata);
    }

    hideKeyboard(self, false);
}


static void addLetter(PDKeyboard * _Nonnull self, char newLetter) {
    const char lastLetter = self->text.super.count > 0
        ? self->text.super.data[self->text.super.count - 1]
        : '_';
    PDKeyboardMutableTextPush(&self->text, newLetter);
    if (self->textChangedCallback) {
        self->textChangedCallback(self->textChangedCallbackUserdata);
    }

    PDKeyboardCapitalization capitalizationBehavior = self->capitalizationBehavior;
    if ((newLetter == ' ' && capitalizationBehavior == kCapitalizationWords) ||
        (newLetter == ' ' && lastLetter == '.' && capitalizationBehavior == kCapitalizationSentences)) {
        selectColumn(self, kColumnUpper);
    }
}


static void handleMenuCommand(PDKeyboard * _Nonnull self) {
    const int selectedMenuOption = self->selectionIndexes[kColumnMenu];
    switch (selectedMenuOption) {
        case kMenuOptionDelete:
            deleteAction(self);
            break;
        case kMenuOptionOK:
            hideKeyboard(self, true);
            break;
        case kMenuOptionSpace:
            addLetter(self, ' ');
            break;
        case kMenuOptionCancel:
            cancelAction(self);
            break;
        default:
            playdate->system->error("Unsupported menu option: %d", selectedMenuOption);
            break;
    }
}

static void enterKey(PDKeyboard * _Nonnull self) {
    const PDKeyboardColumn selectedColumn = self->selectedColumn;
    if (selectedColumn == kColumnMenu) {
        handleMenuCommand(self);
    } else {
        const char *column = columns[selectedColumn];
        const char newLetter = column[self->selectionIndexes[selectedColumn]];
        addLetter(self, newLetter);
        self->lastTypedColumn = selectedColumn;
    }
    playSound(&self->samplePlayer, &self->keySound, kSoundKeyPress);
}


static void enterNewLetterIfNecessary(PDKeyboard * _Nonnull self) {
    if (!self->isVisible || isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }
    
    const unsigned int currentMillis = playdate->system->getCurrentTimeMilliseconds();
    
    PDButtons pressing;
    PDButtons justPressed;
    playdate->system->getButtonState(&pressing, &justPressed, NULL);
    if ((justPressed & kButtonA) && (currentMillis > self->lastKeyEnteredTime + minKeyRepeatMilliseconds)) {
        enterKey(self);
        self->lastKeyEnteredTime = currentMillis;
        
        const float initialKeyRepeatSeconds = 0.3f;
        self->keyRepeatDelay = floorf(initialKeyRepeatSeconds * self->refreshRate);
    }
    else if (pressing & kButtonA) {
        if (self->keyRepeatDelay <= 0) {
            enterKey(self);
            const float keyRepeatSeconds = 0.1f; // the following repeat delays should be shorter
            self->keyRepeatDelay = floorf(keyRepeatSeconds * self->refreshRate);
        } else {
            self->keyRepeatDelay--;
        }
    }
}

#pragma mark - Animations

static void updateAnimation(PDKeyboard * _Nonnull self) {
    const unsigned int animationTime = playdate->system->getCurrentTimeMilliseconds() - self->animationStartTime;

    if (animationTime >= self->animationDuration) {
        // animation ended
        switch (self->currentAnimationType) {
            case kAnimationTypeKeyboardShow:
                self->keyboardRect.origin.x = displayWidth - keyboardWidth;
                if (self->keyboardDidShowCallback) {
                    self->keyboardDidShowCallback(self->keyboardDidShowCallbackUserdata);
                }
                break;
            case kAnimationTypeKeyboardHide:
                self->keyboardRect.origin.x = displayWidth;
                self->isVisible = false;
                // reset main update function
                playdate->system->setUpdateCallback(self->playdateUpdate, self->playdateUpdateUserdata);
                if (self->keyboardDidHideCallback) {
                    self->keyboardDidHideCallback(self->keyboardDidHideCallbackUserdata);
                }
                self->text.super.count = 0;
                break;
            case kAnimationTypeSelectionUp:
            case kAnimationTypeSelectionDown:
                self->selectionYOffset = 0;
                break;
            default:
                // Nothing to do.
                break;
        }
        self->currentAnimationType = kAnimationTypeNone;
    }
    else {
        // see what type of animation we are running, and continue it
        switch (self->currentAnimationType) {
            case kAnimationTypeKeyboardShow:
                self->keyboardRect.origin.x = outBackEase(animationTime, displayWidth, - keyboardWidth, self->animationDuration, 1);
                self->keyboardRect.size.width = displayWidth - self->keyboardRect.origin.x;
                break;
            case kAnimationTypeKeyboardHide:
                self->keyboardRect.origin.x = outBackEase(animationTime, displayWidth - keyboardWidth, keyboardWidth, self->animationDuration, 1);
                self->keyboardRect.size.width = displayWidth - self->keyboardRect.origin.x;
                break;
            case kAnimationTypeSelectionUp:
            case kAnimationTypeSelectionDown:
                self->selectionYOffset = linearEase(animationTime, self->selectionStartY, -self->selectionStartY, self->animationDuration);
                break;
            default:
                // Nothing to do.
                break;
        }
    }
}


static void startAnimation(PDKeyboard * _Nonnull self, PDKeyboardAnimationType animationType, unsigned int duration) {
    // finish the last animation before starting this one
    if (self->currentAnimationType != kAnimationTypeNone) {
        self->animationDuration = playdate->system->getCurrentTimeMilliseconds() - self->animationStartTime;
        updateAnimation(self);
    }
    
    self->animationDuration = duration;
    self->currentAnimationType = animationType;
    self->animationStartTime = playdate->system->getCurrentTimeMilliseconds();
}

#pragma mark - Selection

static const unsigned int scrollAnimationDuration = 150;

static void moveSelectionUp(PDKeyboard * _Nonnull self, int count, bool_t shiftRow) {
    if (isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }

    const PDKeyboardColumn selectedColumn = self->selectedColumn;
    if (selectedColumn == kColumnMenu) {
        count = 1;
    }

    int8_t *selectionIndexes = self->selectionIndexes;
    if (selectedColumn == kColumnMenu && selectionIndexes[kColumnMenu] == 0) {
        if (self->refreshRate > 30) {
            self->rowJiggle = 2;
        } else {
            self->rowJiggle = 1;
        }
        playSound(&self->samplePlayer, &self->bumpSound, kSoundBump);
        return;
    }

    // moving the selection up means moving the letters down. Set an offset that goes from position of the old current letter and animates to zero

    selectionIndexes[selectedColumn] = (selectionIndexes[selectedColumn] - count + columnCounts[selectedColumn]) % columnCounts[selectedColumn];

    // move upper and lower alphabets together
    if (selectedColumn == kColumnLower) {
        selectionIndexes[kColumnUpper] = selectionIndexes[kColumnLower];
    } else if (selectedColumn == kColumnUpper) {
        selectionIndexes[kColumnLower] = selectionIndexes[kColumnUpper];
    }

    self->selectionYOffset -= (rowHeight * count);
    self->selectionStartY = self->selectionYOffset;

    startAnimation(self, kAnimationTypeSelectionUp, scrollAnimationDuration);
    // let the animation think it's already been going on for a frame
    self->animationStartTime = playdate->system->getCurrentTimeMilliseconds() - (1000 / self->refreshRate);
    
    if (shiftRow) {
        if (self->refreshRate > 30) {
            self->rowShift = -2;
        } else {
            self->rowShift = -1;
        }
    }

    playSound(&self->samplePlayer, &self->rowSound, kSoundRowMove);
}


static void moveSelectionDown(PDKeyboard * _Nonnull self, int count, bool_t shiftRow) {
    if (isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }

    const PDKeyboardColumn selectedColumn = self->selectedColumn;
    if (selectedColumn == kColumnMenu) {
        count = 1;
    }

    int8_t *selectionIndexes = self->selectionIndexes;
    if (selectedColumn == kColumnMenu && selectionIndexes[kColumnMenu] == kMenuColumnCount - 1) {
        if (self->refreshRate > 30) {
            self->rowJiggle = 2;
        } else {
            self->rowJiggle = 1;
        }
        playSound(&self->samplePlayer, &self->bumpSound, kSoundBump);
        return;
    }

    selectionIndexes[selectedColumn] = (selectionIndexes[selectedColumn] + count) % columnCounts[selectedColumn];

    // move upper and lower alphabets together
    if (selectedColumn == kColumnLower) {
        selectionIndexes[kColumnUpper] = selectionIndexes[kColumnLower];
    } else if (selectedColumn == kColumnUpper) {
        selectionIndexes[kColumnLower] = selectionIndexes[kColumnUpper];
    }

    self->selectionYOffset += (rowHeight * count);
    self->selectionStartY = self->selectionYOffset;

    startAnimation(self, kAnimationTypeSelectionDown, scrollAnimationDuration);
    // let the animation think it's already been going on for a frame
    self->animationStartTime = playdate->system->getCurrentTimeMilliseconds() - (1000 / self->refreshRate);

    if (shiftRow) {
        if (self->refreshRate > 30) {
            self->rowShift = 2;
        } else {
            self->rowShift = 1;
        }
    }

    playSound(&self->samplePlayer, &self->rowSound, kSoundRowMove);
}


static void jiggleColumn(PDKeyboard * _Nonnull self, PDKeyboardJiggleDirection jiggleDirection) {
    const int numFrames = self->refreshRate > 30 ? 2 : 1;
    
    self->columnJiggle = jiggleDirection * numFrames;
}

static void selectColumn(PDKeyboard * _Nonnull self, PDKeyboardColumn column) {
    if (isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }

    const PDKeyboardColumn selectedColumn = self->selectedColumn;
    if (selectedColumn > column) {
        jiggleColumn(self, kJiggleLeft);
    } else {
        jiggleColumn(self, kJiggleRight);
    }

    if (column > selectedColumn) {
        playSound(&self->samplePlayer, &self->columnNextSound, kSoundColumnMoveNext);
    } else {
        playSound(&self->samplePlayer, &self->columnPreviousSound, kSoundColumnMovePrevious);
    }

    self->selectedColumn = column;

    struct rectangle selectedCharacterRect = self->selectedCharacterRect;
    selectedCharacterRect.origin.x = columnPositions[column];
    selectedCharacterRect.size.width = columnWidths[column];
    self->selectedCharacterRect = selectedCharacterRect;
}


static void selectPreviousColumn(PDKeyboard * _Nonnull self) {
    if (isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }

    const PDKeyboardColumn selectedColumn = self->selectedColumn = (self->selectedColumn - 1 + kColumnCount) % kColumnCount;
    jiggleColumn(self, kJiggleLeft);
    struct rectangle selectedCharacterRect = self->selectedCharacterRect;
    selectedCharacterRect.origin.x = columnPositions[selectedColumn];
    selectedCharacterRect.size.width = columnWidths[selectedColumn];
    self->selectedCharacterRect = selectedCharacterRect;
    playSound(&self->samplePlayer, &self->columnNextSound, kSoundColumnMoveNext);
}

static void selectNextColumn(PDKeyboard * _Nonnull self) {
    if (isShowOrHideAnimation(self->currentAnimationType)) {
        return;
    }

    const PDKeyboardColumn selectedColumn = self->selectedColumn = (self->selectedColumn + 1) % kColumnCount;
    jiggleColumn(self, kJiggleRight);
    struct rectangle selectedCharacterRect = self->selectedCharacterRect;
    selectedCharacterRect.origin.x = columnPositions[selectedColumn];
    selectedCharacterRect.size.width = columnWidths[selectedColumn];
    self->selectedCharacterRect = selectedCharacterRect;
    playSound(&self->samplePlayer, &self->columnPreviousSound, kSoundColumnMovePrevious);
}


static void checkButtonInputs(PDKeyboard * _Nonnull self) {
    int8_t scrollRepeatDelay = self->scrollRepeatDelay;
    int8_t keyRepeatDelay = self->keyRepeatDelay;

    PDButtons pressing;
    PDButtons justPressed;
    PDButtons justReleased;
    playdate->system->getButtonState(&pressing, &justPressed, &justReleased);

    if (justPressed & kButtonUp) {
        moveSelectionUp(self, 1, true);
        scrollRepeatDelay = self->frameRateAdjustedScrollRepeatDelay;
    }
    else if (pressing & kButtonUp) {
        if (scrollRepeatDelay <= 0) {
            moveSelectionUp(self, 1, true);
            self->scrollingVertically = true;
            if (self->refreshRate > 30) {
                scrollRepeatDelay = 1;
            }
        } else {
            scrollRepeatDelay--;
        }
    }
    else if (justReleased & kButtonUp) {
        self->scrollingVertically = false;
    }
    else if (justPressed & kButtonDown) {
        moveSelectionDown(self, 1, true);
        scrollRepeatDelay = self->frameRateAdjustedScrollRepeatDelay;
    }
    else if (pressing & kButtonDown) {
        if (scrollRepeatDelay <= 0) {
            moveSelectionDown(self, 1, true);
            self->scrollingVertically = true;
            if (self->refreshRate > 30) {
                scrollRepeatDelay = 1;
            }
        } else {
            scrollRepeatDelay--;
        }
    }
    else if (justReleased & kButtonDown) {
        self->scrollingVertically = false;
    }
    else if (justPressed & kButtonLeft) {
        selectPreviousColumn(self);
    }
    else if (justPressed & kButtonRight) {
        selectNextColumn(self);
    }
    else if (justPressed & kButtonB) {
        playSound(&self->samplePlayer, &self->keySound, kSoundKeyPress);
        deleteAction(self);
        const float initialKeyRepeatSeconds = 0.3f;
        keyRepeatDelay = floorf(initialKeyRepeatSeconds * self->refreshRate);
    }
    else if (pressing & kButtonB) {
        if (keyRepeatDelay <= 0) {
            playSound(&self->samplePlayer, &self->keySound, kSoundKeyPress);
            deleteAction(self);
            const float keyRepeatSeconds = 0.1f;
            keyRepeatDelay = floorf(keyRepeatSeconds * self->refreshRate);
        } else {
            keyRepeatDelay--;
        }
    }
    self->scrollRepeatDelay = scrollRepeatDelay;
    self->keyRepeatDelay = keyRepeatDelay;
}

#pragma mark - Update

// override on the main playdate.update function so that we can run our animations without requiring timers
static int keyboardUpdate(void * _Nonnull userdata) {
    PDKeyboard *self = userdata;

    if (self->isVisible) {
        enterNewLetterIfNecessary(self);

        if (self->currentAnimationType != kAnimationTypeNone) {
            updateAnimation(self);

            if (isShowOrHideAnimation(self->currentAnimationType)) {
                if (self->keyboardAnimatingCallback) {
                    self->keyboardAnimatingCallback(self->keyboardAnimatingCallbackUserdata);
                }
            }
        }

        if (!self->justOpened) {
            checkButtonInputs(self);
            const float crankChange = playdate->system->getCrankChange();
            if (crankChange != 0.0f) {
                keyboardInputCranked(self, crankChange);
            }
        } else {
            self->justOpened = false;
        }

        self->playdateUpdate(self->playdateUpdateUserdata);
        drawKeyboard(self);
    }
    return true;
}

#pragma mark - Resources

static LCDBitmap * _Nonnull loadBitmapOrError(const char * _Nonnull path) {
    const char *error = NULL;
    LCDBitmap *bitmap = playdate->graphics->loadBitmap(path, &error);
    if (error) {
        playdate->system->error("Unable to load bitmap at path %s, error: %s", path, error);
    }
    return bitmap;
}

static void loadFontAndImages(void) {
    if (keyboardFont) {
        return;
    }
    const char *error = NULL;
    keyboardFont = playdate->graphics->loadFont("CoreLibs/assets/keyboard/Roobert-24-Keyboard-Medium", &error);
    if (error) {
        playdate->system->error("Unable to load font: %s", error);
    }
    fontHeight = playdate->graphics->getFontHeight(keyboardFont);
    menuColumn[0] = menuImageSpace = loadBitmapOrError("CoreLibs/assets/keyboard/menu-space");
    menuColumn[1] = menuImageOK = loadBitmapOrError("CoreLibs/assets/keyboard/menu-ok");
    menuColumn[2] = menuImageDelete = loadBitmapOrError("CoreLibs/assets/keyboard/menu-del");
    menuColumn[3] = menuImageCancel = loadBitmapOrError("CoreLibs/assets/keyboard/menu-cancel");
}

#pragma mark - Public functions

static PDKeyboard * _Nonnull PDKeyboardNew(void) {
    PDKeyboard *self = playdate->system->realloc(NULL, sizeof(PDKeyboard));

    loadFontAndImages();
    const int selectionY = displayHeight / 2 - rowHeight / 2 - 2;
    const PDKeyboardColumn selectedColumn = kColumnUpper;

    *self = (PDKeyboard) {
        .capitalizationBehavior = kCapitalizationNormal,

        .selectedColumn = selectedColumn,
        .lastTypedColumn = selectedColumn,
        .selectionIndexes = {0, 0, 0, 1},

        .isVisible = false,
        .justOpened = true,

        .currentAnimationType = kAnimationTypeNone,

        // TODO: Ideally, would be retrieved with playdate->display->getRefreshRate()
        .refreshRate = 30.0f,
        .frameRateAdjustedScrollRepeatDelay = 6,

        .selectionY = selectionY,

        .keyboardRect = {
            .origin = {
                .x = displayWidth,
                .y = 0
            },
            .size = {
                .width = 0,
                .height = displayHeight
            }
        },
        .selectedCharacterRect = {
            .origin = {
                .x = columnPositions[selectedColumn],
                .y = selectionY
            },
            .size = {
                .width = columnWidths[selectedColumn],
                .height = rowHeight
            }
        },

        .text = {},
    };
    return self;
}

static void PDKeyboardFree(PDKeyboard * _Nonnull self) {
    PDKeyboardMutableTextFree(&self->text);
    PDKeyboardTextFree(&self->originalText);
    freeSounds(self);
    playdate->system->realloc(self, 0);
}

static void PDKeyboardShow(PDKeyboard * _Nonnull self, const char * _Nullable newText, const unsigned int newTextLength) {
    if (self->playdateUpdate == NULL) {
        playdate->system->error("playdateUpdate must be defined before calling show()");
        return;
    }

    if (self->isVisible) {
        return;
    }

    self->justOpened = true;

    self->okButtonPressed = false;
    // scroll the menu row to OK
    self->selectionIndexes[kColumnMenu] = 1;
    // move the selection back to the last row a character was entered from
    self->selectedColumn = self->lastTypedColumn;
    self->selectedCharacterRect.origin.x = columnPositions[self->selectedColumn];
    self->selectedCharacterRect.size.width = columnWidths[self->selectedColumn];

    self->originalText.data = playdate->system->realloc(self->originalText.data, newTextLength + 1);
    memcpy(self->originalText.data, newText, newTextLength * sizeof(char));
    self->originalText.data[newTextLength] = '\0';
    self->originalText.count = newTextLength;

    PDKeyboardMutableTextEnsureCapacity(&self->text, max((newTextLength * 2) + 1, 10));
    memcpy(self->text.super.data, newText, newTextLength);
    self->text.super.data[newTextLength] = '\0';
    self->text.super.count = newTextLength;

    playdate->system->setUpdateCallback(keyboardUpdate, self);

    if (self->currentAnimationType != kAnimationTypeNone) {
        // force the previous animation to finish
        self->animationStartTime = 0;
        updateAnimation(self);
    }

    const float scrollDelaySeconds = 0.18f;
    self->frameRateAdjustedScrollRepeatDelay = floorf(scrollDelaySeconds * self->refreshRate);

    self->isVisible = true;
    startAnimation(self, kAnimationTypeKeyboardShow, 220);
}

static void PDKeyboardHide(PDKeyboard * _Nonnull self) {
    if (self->isVisible && self->currentAnimationType == kAnimationTypeNone) {
        startAnimation(self, kAnimationTypeKeyboardHide, 220);

        if (self->keyboardWillHideCallback) {
            self->keyboardWillHideCallback(self->okButtonPressed, self->keyboardWillHideCallbackUserdata);
        }
    }
}

static void PDKeyboardSetPlaydateUpdateCallback(PDKeyboard * _Nonnull self, PDCallbackFunction * _Nonnull callback, void * _Nullable userdata) {
    self->playdateUpdate = callback;
    self->playdateUpdateUserdata = userdata;
}
static void PDKeyboardSetRefreshRate(PDKeyboard * _Nonnull self, float refreshRate) {
    const float scrollDelaySeconds = 0.18f;
    self->refreshRate = refreshRate;
    self->frameRateAdjustedScrollRepeatDelay = floorf(scrollDelaySeconds * refreshRate);
}

static void PDKeyboardGetText(PDKeyboard * _Nonnull self, char * _Nonnull * _Nullable text, unsigned int * _Nullable count) {
    const unsigned int charCount = self->text.super.count;
    char *data = playdate->system->realloc(*text, (charCount + 1) * sizeof(char));
    memcpy(data, self->text.super.data, charCount * sizeof(char));
    data[charCount] = '\0';
    *text = data;
    *count = charCount;
}

static int PDKeyboardGetWidth(PDKeyboard * _Nonnull self) {
    return self->keyboardRect.size.width;
}

static int PDKeyboardGetLeft(PDKeyboard * _Nonnull self) {
    return self->keyboardRect.origin.x;
}

static bool_t PDKeyboardIsVisible(PDKeyboard * _Nonnull self) {
    return self->isVisible;
}

static PDKeyboardCapitalization PDKeyboardGetCapitalizationBehavior(PDKeyboard * _Nonnull self) {
    return self->capitalizationBehavior;
}

static void PDKeyboardSetCapitalizationBehavior(PDKeyboard * _Nonnull self, PDKeyboardCapitalization behavior) {
    if (behavior < kCapitalizationNormal || behavior > kCapitalizationSentences) {
        playdate->system->error("Please use one of the following options: playdate.keyboard.kCapitalizationNormal, playdate.keyboard.kCapitalizationWords, playdate.keyboard.kCapitalizationSentences");
    }
    self->capitalizationBehavior = behavior;
}

static void PDKeyboardSetKeyboardDidShowCallback(PDKeyboard * _Nonnull self, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata) {
    self->keyboardDidShowCallback = callback;
    self->keyboardDidShowCallbackUserdata = userdata;
}

static void PDKeyboardSetKeyboardDidHideCallback(PDKeyboard * _Nonnull self, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata) {
    self->keyboardDidHideCallback = callback;
    self->keyboardDidHideCallbackUserdata = userdata;
}

static void PDKeyboardSetKeyboardWillHideCallback(PDKeyboard * _Nonnull self, PDKeyboardWillHideCallback * _Nullable callback, void * _Nullable userdata) {
    self->keyboardWillHideCallback = callback;
    self->keyboardWillHideCallbackUserdata = userdata;
}

static void PDKeyboardSetKeyboardAnimatingCallback(PDKeyboard * _Nonnull self, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata) {
    self->keyboardAnimatingCallback = callback;
    self->keyboardAnimatingCallbackUserdata = userdata;
}

static void PDKeyboardSetTextChangedCallback(PDKeyboard * _Nonnull self, PDKeyboardCallback * _Nullable callback, void * _Nullable userdata) {
    self->textChangedCallback = callback;
    self->textChangedCallbackUserdata = userdata;
}


#pragma mark - API struct.

const struct pd_keyboard keyboardApi = (struct pd_keyboard) {
    .newKeyboard = PDKeyboardNew,
    .freeKeyboard = PDKeyboardFree,

    .setPlaydateUpdateCallback = PDKeyboardSetPlaydateUpdateCallback,
    .setRefreshRate = PDKeyboardSetRefreshRate,

    .show = PDKeyboardShow,
    .hide = PDKeyboardHide,

    .getText = PDKeyboardGetText,

    .isVisible = PDKeyboardIsVisible,
    .getWidth = PDKeyboardGetWidth,
    .getLeft = PDKeyboardGetLeft,

    .getCapitalizationBehavior = PDKeyboardGetCapitalizationBehavior,
    .setCapitalizationBehavior = PDKeyboardSetCapitalizationBehavior,

    .setKeyboardDidShowCallback = PDKeyboardSetKeyboardDidShowCallback,
    .setKeyboardDidHideCallback = PDKeyboardSetKeyboardDidHideCallback,
    .setKeyboardWillHideCallback = PDKeyboardSetKeyboardWillHideCallback,
    .setKeyboardAnimatingCallback = PDKeyboardSetKeyboardAnimatingCallback,
    .setTextChangedCallback = PDKeyboardSetTextChangedCallback,
};
