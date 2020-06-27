#ifndef _INCLUDE_LAYOUT_H_
#define _INCLUDE_LAYOUT_H_

#include "ui_input.h"


void showMain();
void showSetup();

void resetSelection();
void updateSelection(Encoder::Update update);
void clickSelection(Button::State);

bool drawAll(bool refresh);

#endif // _INCLUDE_LAYOUT_H_
