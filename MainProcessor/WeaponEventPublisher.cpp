#include "WeaponEventPublisher.h"

WeaponEventPublisher::WeaponEventPublisher()        
    : lastCleanupTime_(std::chrono::steady_clock::now()) {
        LogInfo("WeaponEventPublisher initialized");
}

WeaponEventPublisher::~WeaponEventPublisher() {
        LogInfo("WeaponEventPublisher shutting down");
        LogStatistics();
    }
