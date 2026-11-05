#pragma once

#include "Types.h"

extern ptr libmain;

struct UserInfo : Class {
    Field<0x208, ptr> coins;
    Field<0x210, ptr> cash;
    Field<0xc0,  ptr> DisplayName;
    Field<0xd0,  ptr> loginCountryCode;

    UserInfo(ptr instance = 0) : Class(instance), coins(instance), cash(instance) {}
//Original src For free by @xvaluex : @FiturinEngine.

    operator bool() { return instance && this->isInstanceOf("UserInfo"); }
};

static UserInfo sharedUserInfo;
