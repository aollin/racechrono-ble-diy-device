/*
 * PackedIdInfo.h
 */

#ifndef PACKETIDINFO_H_
#define PACKETIDINFO_H_
#ifdef __cplusplus

#include <Arduino.h>

static const uint32_t MAP_MAX_ITEMS = 1024;
static const uint32_t MAP_HASH_BITS = 1;
static const uint32_t MAP_HASH_SIZE = 1 << MAP_HASH_BITS;
static const uint32_t MAP_HASH_BITMASK = MAP_HASH_SIZE - 1;

class PacketIdInfoItem {
public:
    PacketIdInfoItem(uint32_t packetId, uint16_t notifyIntervalMs);
    virtual ~PacketIdInfoItem();
    
    // Find a packet id info
    PacketIdInfoItem* findItem(uint32_t packetId);

    // Add new packet id info
    static void add(PacketIdInfoItem** rootItem, uint32_t packetId, uint16_t notifyIntervalMs);

    // Return true if this packetId should be notified
    bool shouldNotify();
    
    // Mark packet id notified
    void markNotified();
    
    // Get packet id
    uint32_t getPacketId() { return mPacketId; }

    // Set notify interval (0 = no throttling)
    void setNotifyInterval(uint16_t notifyIntervalMs) { mNotifyIntervalMs = notifyIntervalMs; }

private:
    uint32_t mPacketId;
    uint32_t mPreviousNotifyMs;
    PacketIdInfoItem* mNextItem;
    uint16_t mNotifyIntervalMs;
};

class PacketIdInfo {
public:
    PacketIdInfo();
    virtual ~PacketIdInfo();

    // Reset data
    void reset();

    // Create new packetId record
    PacketIdInfoItem* findItem(uint32_t packetId, bool createIfMissing);

    // Add new packet id info with interval
    void setNotifyInterval(uint32_t packetId, uint16_t notifyIntervalMs);

    // Set default notify interval (0 = no throttling)
    void setDefaultNotifyInterval(uint16_t defaultNotifyIntervalMs) { mDefaultNotifyIntervalMs = defaultNotifyIntervalMs; }

private:
    // Get hash value for packet id
    static uint32_t getHashValue(uint32_t packetId);

private:
    uint16_t mItemCount;
    uint16_t mDefaultNotifyIntervalMs;
    PacketIdInfoItem* mGenericItem;
    PacketIdInfoItem* mHashMap[MAP_HASH_SIZE];
};

#endif
#endif
