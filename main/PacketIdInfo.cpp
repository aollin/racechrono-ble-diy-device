/*
 * PacketIdInfo.cpp
 */
#include "PacketIdInfo.h"

PacketIdInfoItem::PacketIdInfoItem(uint32_t packetId, uint16_t notifyIntervalMs) {
    mPacketId = packetId;
    mNextItem = nullptr;
    mPreviousNotifyMs = 0;
    mNotifyIntervalMs = notifyIntervalMs;
}

PacketIdInfoItem::~PacketIdInfoItem() {
    if (mNextItem) {
        delete mNextItem;
        mNextItem = nullptr;
    }
}

PacketIdInfoItem* PacketIdInfoItem::findItem(uint32_t packetId) {
    // Find existing item
    PacketIdInfoItem* current = this;
    PacketIdInfoItem* latest = this;
    while (current) {
        if (current->mPacketId == packetId) {
            return current;
        }
        latest = current;
        current = current->mNextItem;
    }
    return nullptr;
}

void PacketIdInfoItem::add(PacketIdInfoItem** rootItem, uint32_t packetId, uint16_t notifyIntervalMs) {
    PacketIdInfoItem* current = *rootItem;
    PacketIdInfoItem* latest = *rootItem;
    while (current) {
        if (current->mPacketId == packetId) {
            current->mNotifyIntervalMs = notifyIntervalMs;
            return;
        }
        latest = current;
        current = current->mNextItem;
    }

    // Not found, create new item
    PacketIdInfoItem* newItem = new PacketIdInfoItem(packetId, notifyIntervalMs);
    
    // Save new item
    if (latest) {
        // Add to end of chain
        latest->mNextItem = newItem;
    } else {
        // Created root item
        *rootItem = newItem;
    }
}

bool PacketIdInfoItem::shouldNotify() {
    if (mNotifyIntervalMs == 0) {
        return true;
    } else {
        uint32_t ms = millis();
        uint32_t delta = ms - mPreviousNotifyMs;
        //Serial.printf("Packet = %d, interval = %d, delta = %d\n", mPacketId, mNotifyIntervalMs, delta);
        return delta > mNotifyIntervalMs;
    }
}

void PacketIdInfoItem::markNotified() {
    uint32_t ms = millis();
    uint32_t delta = ms - mPreviousNotifyMs;
    if (delta <= 0 || delta > (mNotifyIntervalMs * 6) / 5) {
        // Interval is out of range, set previous notify as current time
        mPreviousNotifyMs = ms;
    } else {
        // Accurate intervals
        mPreviousNotifyMs += mNotifyIntervalMs;
    }
}

PacketIdInfo::PacketIdInfo() {
    mItemCount = 0;  
    mGenericItem = new PacketIdInfoItem(0, 0);
    for (int pos = 0; pos < MAP_HASH_SIZE; pos++) {
        mHashMap[pos] = nullptr;
    }
}

PacketIdInfo::~PacketIdInfo() {
    reset();
}

uint32_t PacketIdInfo::getHashValue(uint32_t packetId) {
    return (packetId & MAP_HASH_BITMASK) ^ ((packetId >> MAP_HASH_BITS) & MAP_HASH_BITMASK);
}

void PacketIdInfo::reset() {
    for (int pos = 0; pos < MAP_HASH_SIZE; pos++) {
        if (mHashMap[pos]) {
            delete mHashMap[pos];
            mHashMap[pos] = nullptr;
        }
    }
}

PacketIdInfoItem* PacketIdInfo::findItem(uint32_t packetId, bool createIfMissing) {
    uint32_t hashValue = getHashValue(packetId);
    PacketIdInfoItem** rootItem = &mHashMap[hashValue];
    PacketIdInfoItem* foundItem = nullptr;
    if (*rootItem) {
        foundItem = (*rootItem)->findItem(packetId);
    }
    if (foundItem) {
        // Found it
        return foundItem;
    } else if (createIfMissing) {
        // Did not find, but do create
        if (mItemCount < MAP_MAX_ITEMS) {
            // Create new item
            foundItem = new PacketIdInfoItem(packetId, mDefaultNotifyIntervalMs);
            PacketIdInfoItem::add(rootItem, packetId, mDefaultNotifyIntervalMs);
            return foundItem;
        } else {
            // Give generic item, as we do not want to run out of memory
            return mGenericItem;
        }
        mItemCount++;
    } else {
        return nullptr;
    }
}

void PacketIdInfo::setNotifyInterval(uint32_t packetId, uint16_t notifyIntervalMs) {
    uint32_t hashValue = getHashValue(packetId);
    PacketIdInfoItem** rootItem = &mHashMap[hashValue];
    PacketIdInfoItem::add(rootItem, packetId, notifyIntervalMs);  
}
