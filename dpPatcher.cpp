// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include "disasm-lib/disasm.h"
#include <regex>

static size_t dpCopyInstructions(void *dst, void *src, size_t minlen)
{
    size_t len = 0;
#ifdef _M_X64
    ARCHITECTURE_TYPE arch = ARCH_X64;
#elif defined _M_IX86
    ARCHITECTURE_TYPE arch = ARCH_X86;
#endif
    DISASSEMBLER dis;
    if(InitDisassembler(&dis, arch)) {
        INSTRUCTION* pins = NULL;
        U8* pLoc = (U8*)src;
        U8* pDst = (U8*)dst;
        DWORD dwFlags = DISASM_SUPPRESSERRORS;

        while( len<minlen ) {
            pins = GetInstruction(&dis, (ULONG_PTR)pLoc, pLoc, dwFlags);
            if(!pins) { break; }
            if(pins->Type == ITYPE_RET ) { break; }

            //// todo: call or jmp
            //if(pins->Type == ITYPE_BRANCH	) break;
            //if(pins->Type == ITYPE_BRANCHCC) break;
            //if(pins->Type == ITYPE_CALL	) break;
            //if(pins->Type == ITYPE_CALLCC	) break;

            switch(pLoc[0]) {
            // call & jmp
            case 0xE8:
            case 0xE9:
                {
                    int rva = *(int*)(pLoc+1);
                    pDst[0] = pLoc[0];
                    *(DWORD*)(pDst+1) = (DWORD)((ptrdiff_t)(pLoc+rva)-(ptrdiff_t)(pDst));
                }
                break;
            default:
                memcpy(pDst, pLoc, pins->Length);
                break;
            }

            len  += pins->Length;
            pLoc += pins->Length;
            pDst += pins->Length;
        }

        CloseDisassembler(&dis);
    }
    return len;
}

static BYTE* dpAddJumpInstruction(BYTE* from, BYTE* to)
{
    // 距離が 32bit に収まる範囲であれば、0xe9 RVA
    // そうでない場合、0xff 0x25 [メモリアドレス] + 対象アドレス
    // の形式で jmp する必要がある。
    BYTE* jump_from = from + 5;
    size_t distance = jump_from > to ? jump_from - to : to - jump_from;
    if (distance <= 0x7fff0000) {
        from[0] = 0xe9;
        from += 1;
        *((DWORD*)from) = (DWORD)(to - jump_from);
        from += 4;
    }
    else {
        from[0] = 0xff;
        from[1] = 0x25;
        from += 2;
#ifdef _M_IX86
        *((DWORD*)from) = (DWORD)(from + 4);
#elif defined(_M_X64)
        *((DWORD*)from) = (DWORD)0;
#endif
        from += 4;
        *((DWORD_PTR*)from) = (DWORD_PTR)(to);
        from += 8;
    }
    return from;
}

void dpPatcher::patchImpl(dpPatchData &pi)
{
    // 元コードの退避先
    BYTE *hook = (BYTE*)pi.hook->address;
    BYTE *target = (BYTE*)pi.target->address;
    BYTE *unpatched = (BYTE*)m_talloc.allocate(target);
    DWORD old;
    ::VirtualProtect(target, 32, PAGE_EXECUTE_READWRITE, &old);
    HANDLE proc = ::GetCurrentProcess();

    // 元のコードをコピー & 最後にコピー本へ jmp するコードを付加 (==これを call すれば上書き前の動作をするハズ)
    size_t stab_size = dpCopyInstructions(unpatched, target, 5);
    dpAddJumpInstruction(unpatched+stab_size, target+stab_size);

    // 距離が 32bit に収まらない場合、長距離 jmp で飛ぶコードを挟む。
    // (長距離 jmp は 14byte 必要なので直接書き込もうとすると容量が足りない可能性が出てくる)
    DWORD_PTR dwDistance = hook < target ? target - hook : hook - target;
    if(dwDistance > 0x7fff0000) {
        BYTE *trampoline = (BYTE*)m_talloc.allocate(target);
        dpAddJumpInstruction(trampoline, hook);
        dpAddJumpInstruction(target, trampoline);
        ::FlushInstructionCache(proc, pi.trampoline, 32);
        ::FlushInstructionCache(proc, target, 32);
        pi.trampoline = trampoline;
    }
    else {
        dpAddJumpInstruction(target, hook);
        ::FlushInstructionCache(proc, target, 32);
    }
    ::VirtualProtect(target, 32, old, &old);

    pi.unpatched = unpatched;
    pi.unpatched_size = stab_size;

    if((dpGetConfig().log_flags&dpE_LogDetail)!=0) { // たぶん demangle はそこそこでかい処理なので early out
        char demangled[512];
        dpDemangle(pi.target->name, demangled, sizeof(demangled));
        dpPrintDetail("patch 0x%p -> 0x%p (\"%s\" : \"%s\")\n", pi.target->address, pi.hook->address, demangled, pi.target->name);
    }
}

void dpPatcher::unpatchImpl(const dpPatchData &pi)
{
    DWORD old;
    ::VirtualProtect(pi.target->address, 32, PAGE_EXECUTE_READWRITE, &old);
    dpCopyInstructions(pi.target->address, pi.unpatched, pi.unpatched_size);
    ::VirtualProtect(pi.target->address, 32, old, &old);
    m_talloc.deallocate(pi.unpatched);
    m_talloc.deallocate(pi.trampoline);

    if((dpGetConfig().log_flags&dpE_LogDetail)!=0) {
        char demangled[512];
        dpDemangle(pi.target->name, demangled, sizeof(demangled));
        dpPrintDetail("unpatch 0x%p (\"%s\" : \"%s\")\n", pi.target->address, demangled, pi.target->name);
    }
}


dpPatcher::dpPatcher(dpContext *ctx)
    : m_context(ctx)
{
}

dpPatcher::~dpPatcher()
{
    unpatchAll();
}

void* dpPatcher::patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbolS&)> &condition)
{
    obj->eachSymbols([&](dpSymbol *sym){
        if(dpIsFunction(sym->flags) && condition(sym->simplify())) {
            sym->partialLink();
            patch(dpGetLoader()->findHostSymbolByName(sym->name), sym);
        }
    });
    return nullptr;
}

void* dpPatcher::patch(dpSymbol *target, dpSymbol *hook)
{
    if(!target || !hook) { return nullptr; }
    if(dpIsLinkFailed(target->flags) || dpIsLinkFailed(hook->flags)) { return nullptr; }

    unpatchByAddress(target->address);

    dpPatchData pd;
    pd.target = target;
    pd.hook = hook;
    patchImpl(pd);
    m_patches.insert(pd);
    return pd.unpatched;
}


size_t dpPatcher::unpatchByBinary(dpBinary *obj)
{
    size_t n = 0;
    obj->eachSymbols([&](dpSymbol *sym){
        if(dpIsFunction(sym->flags) && unpatchByAddress(sym->address)) {
            ++n;
        }
    });
    return n;
}

bool dpPatcher::unpatchByAddress(void *addr)
{
    auto p = findPatchByAddressImpl(addr);
    if(p!=m_patches.end()) {
        unpatchImpl(*p);
        m_patches.erase(p);
        return true;
    }
    return false;
}

void dpPatcher::unpatchAll()
{
    dpEach(m_patches, [&](const dpPatchData &p){
        unpatchImpl(p);
    });
    m_patches.clear();
}

dpPatchData* dpPatcher::findPatchByName(const char *name)
{
    auto p = findPatchByNameImpl(name);
    return p==m_patches.end() ? nullptr : const_cast<dpPatchData*>(&*p);
}

dpPatchData* dpPatcher::findPatchByAddress(void *addr)
{
    auto p = findPatchByAddressImpl(addr);
    return p==m_patches.end() ? nullptr : const_cast<dpPatchData*>(&*p);
}

dpPatcher::patch_cont::iterator dpPatcher::findPatchByNameImpl(const char *name)
{
    return dpFind(m_patches, [=](const dpPatchData &pat){
        return strcmp(pat.target->name, name)==0;
    });
}

dpPatcher::patch_cont::iterator dpPatcher::findPatchByAddressImpl(void *addr)
{
    return dpFind(m_patches, [=](const dpPatchData &pat){
        return pat.target->address==addr || pat.hook->address==addr;
    });
}
