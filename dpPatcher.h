// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpPatcher_h
#define dpPatcher_h

#include "DynamicPatcher.h"
#include "dpFoundation.h"
#include "dpBinary.h"

BYTE* dpAddJumpInstruction(BYTE* from, const BYTE* to);

class dpPatcher
{
public:
    dpPatcher(dpContext *ctx);
    ~dpPatcher();
    void*  patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbolS&)> &condition);
    void*  patch(dpSymbol *target, dpSymbol *hook);
    size_t unpatchByBinary(dpBinary *obj);
    bool   unpatchByAddress(void *patched);
    void   unpatchAll();

    dpPatchData* findPatchByName(const char *name);
    dpPatchData* findPatchByAddress(void *addr);

private:
    typedef std::set<dpPatchData> patch_cont;

    dpContext             *m_context;
    dpTrampolineAllocator m_talloc;
    patch_cont            m_patches;

    void         patchImpl(dpPatchData &pi);
    void         unpatchImpl(const dpPatchData &pi);
    patch_cont::iterator findPatchByNameImpl(const char *name);
    patch_cont::iterator findPatchByAddressImpl(void *addr);
};

#endif // dpPatcher_h
