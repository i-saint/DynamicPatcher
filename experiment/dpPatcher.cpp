// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
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

        while( len<minlen && (pins=GetInstruction(&dis, (ULONG_PTR)pLoc, pLoc, dwFlags)) ) {
            if(pins->Type == ITYPE_RET		) break;

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

void dpPatch(dpPatchData &pi)
{
    // 元コードの退避先
    BYTE *preserved = (BYTE*)::dpAllocate(32, pi.symbol.address);
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

void dpUnpatch(dpPatchData &pi)
{
    DWORD old;
    ::VirtualProtect(pi.symbol.address, 32, PAGE_EXECUTE_READWRITE, &old);
    CopyInstructions(pi.symbol.address, pi.orig, pi.size);
    ::VirtualProtect(pi.symbol.address, 32, old, &old);
    ::VirtualFree(pi.orig, 32, MEM_RELEASE);
}


dpPatcher::dpPatcher()
{
}

dpPatcher::~dpPatcher()
{
    unpatchAll();
}

void* dpPatcher::patchByBinary(dpBinary *obj, const std::function<bool (const char *symname)> &condition)
{
    obj->eachSymbols([&](const dpSymbol &sym){
        if(condition(sym.name)) {
            patchByName(sym.name, sym.address);
        }
    });
    return nullptr;
}

void* dpPatcher::patchByName(const char *symbol_name, void *hook)
{
    dpSymbol sym;
    if(dpLoader::findHostSymbolByName(symbol_name, sym)) {
        unpatchByAddress(sym.address);
        dpPatchData pi = {sym, nullptr, hook, 0};
        dpPatch(pi);
        m_patchers.push_back(pi);
        {
            char demangled[1024];
            dpDemangle(sym.name, demangled, sizeof(demangled));
            dpPrint("dp info: patching %s (%s) succeeded.\n", sym.name, demangled);
        }
        return pi.orig;
    }
    return nullptr;
}

void* dpPatcher::patchByAddress(void *symbol_addr, void *hook)
{
    dpSymbol sym;
    char buf[1024];
    if(dpLoader::findHostSymbolByAddress(symbol_addr, sym, buf, sizeof(buf))) {
        unpatchByAddress(sym.address);
        dpPatchData pi = {sym, nullptr, hook, 0};
        dpPatch(pi);
        m_patchers.push_back(pi);
        {
            char demangled[1024];
            dpDemangle(sym.name, demangled, sizeof(demangled));
            dpPrint("dp info: patching %s (%s) succeeded.\n", sym.name, demangled);
        }
        return pi.orig;
    }
    return nullptr;
}

bool dpPatcher::unpatchByBinary(dpBinary *obj)
{
    obj->eachSymbols([&](const dpSymbol &sym){
        unpatchByName(sym.name);
    });
    return false;
}

bool dpPatcher::unpatchByName(const char *name)
{
    if(dpPatchData *p = findPatchByName(name)) {
        dpUnpatch(*p);
        m_patchers.erase(m_patchers.begin()+std::distance(&m_patchers[0], p));
        return true;
    }
    return false;
}

bool dpPatcher::unpatchByAddress(void *addr)
{
    if(dpPatchData *p = findPatchByAddress(addr)) {
        dpUnpatch(*p);
        m_patchers.erase(m_patchers.begin()+std::distance(&m_patchers[0], p));
        return true;
    }
    return false;
}

void dpPatcher::unpatchAll()
{
    eachPatchData([&](dpPatchData &p){ dpUnpatch(p); });
    m_patchers.clear();
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


dpCLinkage dpAPI dpPatcher* dpCreatePatcher()
{
    return new dpPatcher();
}
