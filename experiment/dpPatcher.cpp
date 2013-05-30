// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include "disasm-lib/disasm.h"
#include <regex>

static size_t CopyInstructions(void *dst, void *src, size_t minlen)
{
    size_t len = 0;
#ifdef _WIN64
    ARCHITECTURE_TYPE arch = ARCH_X64;
#elif defined _WIN32
    ARCHITECTURE_TYPE arch = ARCH_X86;
#else
#   error unsupported platform
#endif
    DISASSEMBLER dis;
    if (InitDisassembler(&dis, arch)) {
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

void dpPatcher::patch(dpPatchData &pi)
{
    // 元コードの退避先
    BYTE *preserved = (BYTE*)m_palloc.allocate(pi.symbol.address);
    BYTE *f = (BYTE*)pi.symbol.address;
    DWORD old;
    ::VirtualProtect(f, 32, PAGE_EXECUTE_READWRITE, &old);

    // 元のコードをコピー & 最後にコピー本へ jmp するコードを付加 (==これを call すれば上書き前の動作をするハズ)
    size_t slice = CopyInstructions(preserved, f, 5);
    preserved[slice]=0xE9; // jmp
    *(DWORD*)(preserved+slice+1) = (DWORD)((ptrdiff_t)(f+slice)-(ptrdiff_t)(preserved+slice)-5);
    for(size_t i=slice+5; i<32; ++i) {
        preserved[i] = 0x90;
    }

    // 関数の先頭を hook 関数への jmp に書き換える
    f[0] = 0xE9; // jmp
    *(DWORD*)(f+1) = (DWORD)((ptrdiff_t)pi.hook-(ptrdiff_t)f - 5);
    ::VirtualProtect(f, 32, old, &old);

    pi.orig = preserved;
    pi.size = slice;
}

void dpPatcher::unpatch(dpPatchData &pi)
{
    DWORD old;
    ::VirtualProtect(pi.symbol.address, 32, PAGE_EXECUTE_READWRITE, &old);
    CopyInstructions(pi.symbol.address, pi.orig, pi.size);
    ::VirtualProtect(pi.symbol.address, 32, old, &old);
    m_palloc.deallocate(pi.orig);
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
        dpPatchData pi = {*sym, nullptr, hook, 0};
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
        dpPatchData pi = {*sym, nullptr, hook, 0};
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
        if(p.symbol.address==addr) { r=&p; }
    });
    return r;
}


dpCLinkage dpAPI dpPatcher* dpCreatedpPatcher()
{
    return new dpPatcher();
}
