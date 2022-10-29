#pragma once

#include "data.h"

class BlablaCallbacks
{
public:
    virtual void onMessageReceived(MessageType messageType, void* message) = 0;
    
    virtual void onEventStateChange(const Event& event, bool newState) = 0;
};
