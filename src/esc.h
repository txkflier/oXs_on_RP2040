#pragma once
#include "stdio.h"
#include "stdint.h"


void setupEsc();
void handleEsc();
void processEscFrame();
void escPioRxHandlerIrq();
void processHW4Frame();
void processHW3Frame();
void processKontronikFrame();
void processZTW1Frame();
int32_t calcTemp(float tempRaw);