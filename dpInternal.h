// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpInternal_h
#define dpInternal_h

#include "DynamicPatcher.h"
#include "dpFoundation.h"
#include "dpBinary.h"
#include "dpObjFile.h"
#include "dpLibFile.h"
#include "dpDllFile.h"
//#include "dpElfFile.h"
#include "dpLoader.h"
#include "dpPatcher.h"
#include "dpBuilder.h"
#include "dpContext.h"
#include "dpConfigFile.h"
#include "dpCommunicator.h"

#define dpGetBuilder()  m_context->getBuilder()
#define dpGetPatcher()  m_context->getPatcher()
#define dpGetLoader()   m_context->getLoader()
#define dpGetFilters()   m_context->getSymbolFilters()

#endif // dpInternal_h
