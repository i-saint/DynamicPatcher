// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include <regex>


size_t CopyInstructions(void *dst_, const void *src_, size_t required)
{
    // 不完全につき、未対応の instruction があれば適宜追加すべし
    // 関数の頭 5 byte 以内で実行されるものは多くが mov,sub,push あたりなのでこれだけでも多くに対応はできるハズ
    size_t ret = 0;
    const BYTE *src = (const BYTE*)src_;
    BYTE *dst = (BYTE*)dst_;

    for(; ret<required; ) {
        int size = 0; // instruction size
        bool can_memcpy = true;

        switch(src[ret]) {
            // push
        case 0x55: size=1; break;
        case 0x68:
        case 0x6A: size=5; break;
        case 0xFF:
            switch(src[ret+1]) {
            case 0x74: size=4; break;
            default:   size=3; break;
            }

            // mov
        case 0x8B:
            switch(src[ret+1]) {
            case 0x44: size=4; break;
            case 0x45: size=3; break;
            default:   size=2; break;
            }
            break;
        case 0xB8: size=5; break;

            // sub
        case 0x81: 
            switch(src[ret+1]) {
            case 0xEC: size=6; break;
            default:   size=2; break;
            }
            break;
        case 0x83:
            switch(src[ret+1]) {
            case 0xEC: size=3; break;
            default:   size=2; break;
            }
            break;

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
            break;

        default: size=1; break;
        }

        if(can_memcpy) {
            memcpy(dst+ret, src+ret, size);
        }
        ret += size;
    }

    return ret;
}

void dpPatcher::patch(dpPatchData &pi)
{
    // 元コードの退避先
    BYTE *preserved = (BYTE*)::VirtualAlloc(NULL, 32, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    BYTE *f = (BYTE*)pi.symbol.address;
    DWORD old;
    ::VirtualProtect(f, 32, PAGE_EXECUTE_READWRITE, &old);

    // 元のコードをコピー & 最後にコピー本へ jmp するコードを付加 (==これを call すれば上書き前の動作をするハズ)
    size_t slice = CopyInstructions(preserved, f, 5);
    preserved[slice]=0xE9; // jmp
    *(DWORD*)(preserved+slice+1) = (ptrdiff_t)(f+slice)-(ptrdiff_t)(preserved+slice)-5;

    // 関数の先頭を hook 関数への jmp に書き換える
    f[0] = 0xE9; // jmp
    *(DWORD*)(f+1) = (ptrdiff_t)pi.hook-(ptrdiff_t)f - 5;
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
    ::VirtualFree(pi.orig, 32, MEM_RELEASE);
}


dpPatcher::dpPatcher()
{
}

dpPatcher::~dpPatcher()
{
    unpatchAll();
}

void* dpPatcher::patchByBinary(dpBinary *obj, const char *filter_regex)
{
    std::regex reg(filter_regex);
    obj->eachSymbols([&](const dpSymbol &sym){
        if(std::regex_search(sym.name, reg)) {
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
        patch(pi);
        m_patchers.push_back(pi);
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
        patch(pi);
        m_patchers.push_back(pi);
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
        unpatch(*p);
        m_patchers.erase(m_patchers.begin()+std::distance(&m_patchers[0], p));
        return true;
    }
    return false;
}

bool dpPatcher::unpatchByAddress(void *addr)
{
    if(dpPatchData *p = findPatchByAddress(addr)) {
        unpatch(*p);
        m_patchers.erase(m_patchers.begin()+std::distance(&m_patchers[0], p));
        return true;
    }
    return false;
}

void dpPatcher::unpatchAll()
{
    eachPatchData([&](dpPatchData &p){ unpatch(p); });
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
