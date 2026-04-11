
#include <helpers/debugger.hpp>

#include <cstring>
#include <cstdio>

Debugger::Debugger() : m_attached(false),
                       m_debugHandle(0),
                       m_rc(0),
                       m_tid(0),
                       m_pid(0)
{
    memset(&m_metadata, 0x00, sizeof(m_metadata));
    pmdmntInitialize();
    m_rc = ldrDmntInitialize();
    pminfoInitialize();
}

Debugger::~Debugger()
{
    if (m_attached)
    {
        svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
        svcCloseHandle(m_debugHandle);
    }
    pminfoExit();
    ldrDmntExit();
    pmdmntExit();
}

Result Debugger::attachToProcess_()
{
    if (m_debugHandle == 0 && (envIsSyscallHinted(0x60) == 1))
    {
        m_rc = svcDebugActiveProcess(&m_debugHandle, m_pid);
        if (R_SUCCEEDED(m_rc))
        {
            printf("Attached to process %ld via svc.\n", m_pid);
            m_attached = true;
            m_metadata.process_id = m_pid;
            m_metadata.program_id = m_tid;


            // copied from atmosphere's impl/dmnt_cheat_api.cpp

            // svcGetInfo(&m_metadata.program_id, InfoType_ProgramId, m_debugHandle, 0);
            svcGetInfo(&m_metadata.heap_extents.base,   InfoType_HeapRegionAddress  , m_debugHandle, 0);
            svcGetInfo(&m_metadata.heap_extents.size,   InfoType_HeapRegionSize     , m_debugHandle, 0);
            svcGetInfo(&m_metadata.alias_extents.base,  InfoType_AliasRegionAddress , m_debugHandle, 0);
            svcGetInfo(&m_metadata.alias_extents.size,  InfoType_AliasRegionSize    , m_debugHandle, 0);
            svcGetInfo(&m_metadata.aslr_extents.base,   InfoType_AslrRegionAddress  , m_debugHandle, 0);
            svcGetInfo(&m_metadata.aslr_extents.size,   InfoType_AslrRegionSize     , m_debugHandle, 0);


            /* All applications must have two modules. */

            // (since this is always run in an applet, the attached process can only be an application, so we can skip checks for hbl)
            size_t constexpr max_modules = 2;
            LoaderModuleInfo proc_modules[max_modules] = {0};
            s32 num_modules;
            m_rc = ldrDmntGetProcessModuleInfo(static_cast<u64>(m_metadata.process_id), reinterpret_cast<LoaderModuleInfo *>(proc_modules), max_modules, &num_modules);
            if (R_SUCCEEDED(m_rc))
            {
                LoaderModuleInfo proc_module = {0};
                if (num_modules == max_modules) {
                    proc_module = proc_modules[1];
                }
                m_metadata.main_nso_extents.base = proc_module.base_address;
                m_metadata.main_nso_extents.size = proc_module.size;
                std::memcpy(m_metadata.main_nso_module_id, proc_module.build_id, sizeof(m_metadata.main_nso_module_id));
            }
            else
            {
                printf("Failed to get process module info: %d\n", R_DESCRIPTION(m_rc));
            }
        }
        else
        {
            m_tid = 0;
            m_pid = 0;
        }
        return m_rc;
    }
    return 6500; // Not attached
}

Result Debugger::attachToCurrentProcess()
{
    pmdmntGetApplicationProcessId(&m_pid);
    pminfoGetProgramId(&m_tid, m_pid);
    return attachToProcess_();
}

Result Debugger::attachToProcessByProcessId(u64 processId)
{
    m_pid = processId;
    pminfoGetProgramId(&m_tid, m_pid);
    return attachToProcess_();
}

Result Debugger::attachToProcessByTitleId(u64 titleId)
{
    m_tid = titleId;
    s32 count;
    const u32 max = (2048 * 4) / sizeof(u64);
    u64 pids[max] = {0};
    svcGetProcessList(&count, pids, max);

    for (s32 i = 0; i < count; ++i)
    {
        m_pid = pids[i];
        u64 p_title_id;
        if (R_FAILED(pminfoGetProgramId(&p_title_id, m_pid)))
        {
            continue;
        }
        if (p_title_id == m_tid)
        {
            break;
        }
    }
    if (m_pid < 80)
    {
        printf("m_pid couldn't be determined properly (%ld)", m_pid);
        m_tid = 0;
        m_pid = 0;
        return 6505;
    }
    else
    {
        return attachToProcess_();
    }
}

void Debugger::detach()
{
    if (m_attached)
    {
        svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
        svcCloseHandle(m_debugHandle);
        m_attached = false;
    }
}

u64 Debugger::peekMemory(u64 address)
{
    u64 out = 0;
    if (m_attached)
    {
        svcReadDebugProcessMemory(&out, m_debugHandle, address, sizeof(u64));
    }

    return out;
}

Result Debugger::pokeMemory(size_t varSize, u64 address, u64 value)
{

    if (m_attached)
    {
        return svcWriteDebugProcessMemory(m_debugHandle, &value, address, varSize);
    }
    return 6500; // Not attached
}

Result Debugger::pause()
{
    if (m_attached)
    {
        return svcBreakDebugProcess(m_debugHandle);
    }
    return 6500; // Not attached
}
Result Debugger::resume()
{
    if (m_attached)
    {
        return svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
    }
    return 6500; // Not attached
}
Result Debugger::readMemory(u64 address, void *buffer, size_t bufferSize)
{
    if (m_attached)
    {
        return svcReadDebugProcessMemory(buffer, m_debugHandle, address, bufferSize);
    }
    return 6500; // Not attached
}

Result Debugger::writeMemory(u64 address, void *buffer, size_t bufferSize)
{
    if (m_attached)
    {
        return svcWriteDebugProcessMemory(m_debugHandle, buffer, address, bufferSize);
    }
    return 6500; // Not attached
}

MemoryInfo Debugger::queryMemory(u64 address)
{
    MemoryInfo memInfo = {0};
    if (m_attached)
    {
        u32 pageinfo; // ignored
        svcQueryDebugProcessMemory(&memInfo, &pageinfo, m_debugHandle, address);
    }

    return memInfo;
}
