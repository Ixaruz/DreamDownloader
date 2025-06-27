#include <helpers/BuildIdHelper.hpp>

#include <stdio.h>

namespace { // Anonymous namespace to limit scope
    
    static const std::vector<u64> bids = {
        0xE4BBD879D326A0AD, //2.0.0
        0x8C81A85AA4C1990B, //2.0.1
        0xE5759E5B7E31411B, //2.0.2
        0x205F55C725C16C6F, //2.0.3
        0x372C5EA461D03A7D, //2.0.4
        0x747A5B4CBC530AED, //2.0.5
        0x15765149DF53BA41, //2.0.6
        0x0948E48778171EE6, //2.0.7
        0xCBF780093C874152, //2.0.8
    };
}

u64 BuildIdHelper::getBid() {
    DmntCheatProcessMetadata metadata;
    u64 bid = 0;
    Result rc = dmntchtGetCheatProcessMetadata(&metadata);
    if (R_FAILED(rc)) {
        printf("Failed to get cheat process metadata.\n");
        printf("Error code: 0x%08x\n", rc);
    }
    memcpy(&bid, metadata.main_nso_build_id, 0x8);
    //fix endianess of hash
    bid = __builtin_bswap64(bid);
    return bid;
}
    
Result BuildIdHelper::verifyBid() {
    u64 bid = getBid();
    printf("Found BID: %016llx\n", bid);
    for (const auto& knownBid : bids) {
        if (bid == knownBid) {
            printf("Build ID verified: %016llx\n", bid);
            return 0; // Success
        }
    }
    return 1; // Failure
}
