#include "dmntcht.h"

#include <memory>
#include <cstring>
#include <vector>

class BuildIdHelper {
public:
    static u64 getBid();
    
    static Result verifyBid();
};
