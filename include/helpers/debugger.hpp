extern "C"
{
#include <switch.h>
}

// this turned out to be me reimplementing dmnt:cht, as my switch can't initialize the service anymore... maybe an Atmosphere problem?

class Debugger
{
public:
    Debugger();
    ~Debugger();

    struct CheatProcessMetadata
    {
        struct MemoryRegionExtents
        {
            u64 base;
            u64 size;
        };

        u64 process_id;
        u64 program_id;
        MemoryRegionExtents main_nso_extents;
        MemoryRegionExtents heap_extents;
        MemoryRegionExtents alias_extents;
        MemoryRegionExtents aslr_extents;
        u8 main_nso_module_id[0x20];
    };

    Result attachToCurrentProcess();
    Result attachToProcessByProcessId(u64 processId);
    Result attachToProcessByTitleId(u64 titleId);
    Result pause();
    Result resume();
    void detach();
    u64 getAttachedApplicationProcessId() { return m_pid; }
    u64 getAttachedApplicationTitleId() { return m_tid; }
    CheatProcessMetadata getCheatProcessMetadata() { return m_metadata; }
    bool m_attached;
    Result m_rc;
    Handle m_debugHandle; // no action to be taken to attach
    u64 peekMemory(u64 address);
    Result pokeMemory(size_t varSize, u64 address, u64 value);
    MemoryInfo queryMemory(u64 address);
    Result readMemory(u64 address, void *buffer, size_t bufferSize);
    Result writeMemory(u64 address, void *buffer, size_t bufferSize);

private:
    Result attachToProcess_();
    u64 m_tid;
    u64 m_pid;
    CheatProcessMetadata m_metadata;
};
