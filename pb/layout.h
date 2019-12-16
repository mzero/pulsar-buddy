#ifndef _INCLUDE_LAYOUT_H_
#define _INCLUDE_LAYOUT_H_

#include "ui_input.h"


void resetSelection();
void updateSelection(Encoder::Update update);
void clickSelection(ButtonState);

void drawAll(bool refresh);

#endif // _INCLUDE_LAYOUT_H_
