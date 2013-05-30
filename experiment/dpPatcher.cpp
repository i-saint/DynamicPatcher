// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

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
            /*
            // call & jmp
            case 0xE8:
            case 0xE9:
                {
                    can_memcpy = false;
                    int rva = *(int*)(src+1);
                    dst[ret] = src[ret];
                    *(DWORD*)(dst+ret+1) = (ptrdiff_t)(src+ret+rva)-(ptrdiff_t)(dst+ret);
                    ret += 5;
                }
            */

            memcpy(pDst, pLoc, pins->Length);
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

void dpPatcher::patch(dpPatchData &pi)
{
    // 元コードの退避先
    BYTE *hook = (BYTE*)pi.hook;
    BYTE *target = (BYTE*)pi.symbol.address;
    BYTE *preserved = (BYTE*)m_palloc.allocate(target);
    DWORD old;
    ::VirtualProtect(target, 32, PAGE_EXECUTE_READWRITE, &old);
    HANDLE proc = ::GetCurrentProcess();

    // 元のコードをコピー & 最後にコピー本へ jmp するコードを付加 (==これを call すれば上書き前の動作をするハズ)
    size_t slice = dpCopyInstructions(preserved, target, 5);
    dpAddJumpInstruction(preserved+slice, target+slice);

    DWORD_PTR dwDistance = hook < target ? target - hook : hook - target;
    if(dwDistance > 0x7fff0000) {
        BYTE *trampoline = (BYTE*)m_palloc.allocate(target);
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

    pi.orig = preserved;
    pi.hook_size = slice;
}

void dpPatcher::unpatch(dpPatchData &pi)
{
    DWORD old;
    ::VirtualProtect(pi.symbol.address, 32, PAGE_EXECUTE_READWRITE, &old);
    dpCopyInstructions(pi.symbol.address, pi.orig, pi.hook_size);
    ::VirtualProtect(pi.symbol.address, 32, old, &old);
    m_palloc.deallocate(pi.orig);
    m_palloc.deallocate(pi.trampoline);
}


dpPatcher::dpPatcher()
{
}

dpPatcher::~dpPatcher()
{
    unpatchAll();
}

void* dpPatcher::patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbol&)> &condition)
{
    obj->eachSymbols([&](const dpSymbol &sym){
        if(dpIsFunction(sym.flags) && condition(sym)) {
            patchByName(sym.name, sym.address);
        }
    });
    return nullptr;
}

void* dpPatcher::patchByName(const char *name, void *hook)
{
    if(const dpSymbol *sym = dpGetLoader()->findHostSymbolByName(name)) {
        unpatchByAddress(sym->address);

        dpPatchData pi;
        pi.symbol = *sym;
        pi.hook = hook;
        patch(pi);
        m_patches.push_back(pi);
        {
            char demangled[1024];
            dpDemangle(sym->name, demangled, sizeof(demangled));
            dpPrint("dp info: patched %s (%s)\n", sym->name, demangled);
        }
        return pi.orig;
    }
    return nullptr;
}

inline void dpPrintUnpatch(const dpSymbol &sym)
{
    char demangled[1024];
    dpDemangle(sym.name, demangled, sizeof(demangled));
    dpPrint("dp info: unpatched %s (%s)\n", sym.name, demangled);
}

void* dpPatcher::patchByAddress(void *addr, void *hook)
{
    if(const dpSymbol *sym=dpGetLoader()->findHostSymbolByAddress(addr)) {
        unpatchByAddress(sym->address);

        dpPatchData pi;
        pi.symbol = *sym;
        pi.hook = hook;
        patch(pi);
        m_patches.push_back(pi);
        {
            char demangled[1024];
            dpDemangle(sym->name, demangled, sizeof(demangled));
            dpPrint("dp info: patched %s (%s)\n", sym->name, demangled);
        }
        return pi.orig;
    }
    return nullptr;
}

size_t dpPatcher::unpatchByBinary(dpBinary *obj)
{
    size_t n = 0;
    obj->eachSymbols([&](const dpSymbol &sym){
        if(dpPatchData *p = findPatchByName(sym.name)) {
            if(p->hook==sym.address) {
                dpPrintUnpatch(sym);
                unpatch(*p);
                m_patches.erase(m_patches.begin()+std::distance(&m_patches[0], p));
                ++n;
            }
        }
    });
    return n;
}

bool dpPatcher::unpatchByName(const char *name)
{
    if(dpPatchData *p = findPatchByName(name)) {
        dpPrintUnpatch(p->symbol);
        unpatch(*p);
        m_patches.erase(m_patches.begin()+std::distance(&m_patches[0], p));
        return true;
    }
    return false;
}

bool dpPatcher::unpatchByAddress(void *addr)
{
    if(dpPatchData *p = findPatchByAddress(addr)) {
        dpPrintUnpatch(p->symbol);
        unpatch(*p);
        m_patches.erase(m_patches.begin()+std::distance(&m_patches[0], p));
        return true;
    }
    return false;
}

void dpPatcher::unpatchAll()
{
    eachPatchData([&](dpPatchData &p){ unpatch(p); });
    m_patches.clear();
}

dpPatchData* dpPatcher::findPatchByName(const char *name)
{
    dpPatchData *r = nullptr;
    eachPatchData([&](dpPatchData &p){
        if(strcmp(p.symbol.name, name)==0) { r=&p; }
    });
    return r;
}

dpPatchData* dpPatcher::findPatchByAddress(void *addr)
{
    dpPatchData *r = nullptr;
    eachPatchData([&](dpPatchData &p){
        if(p.symbol.address==addr || p.hook==addr) { r=&p; }
    });
    return r;
}
