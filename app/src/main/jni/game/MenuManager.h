
#pragma once

#include "Types.h"

struct MenuManager : Class {
    MenuManager(ptr instance = 0) : Class(instance) {}
//Original src For free by @xvaluex : @FiturinEngine.

    int getMenuStateId() { return M(int, libmain + 0x305114c, ptr)(instance); }
    
    bool isInQueue() { return getMenuStateId() == 12; }

    operator bool() { return instance && this->isInstanceOf("MenuManager"); }
};

static MenuManager sharedMenuManager;
