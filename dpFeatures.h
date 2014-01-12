// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpFeatures_h
#define dpFeatures_h


//// this option makes dpGetUnpatched() work. and makes dependency for disasm.
//// dpGetUnpatched() returns the function that behaves as old (before patch) function.
//#define dpWithTDisasm
//
//#define dpWithCommunicator
//#define dpWithObjFile
//#define dpWithLibFile
//#define dpWithDllFile
//#define dpWithElfFile


#ifdef dpAlcantarea
#   define dpLogHeader "Alcantarea"
#   define dpWithCommunicator
#   define dpWithObjFile
#else  // alcImpl
#   define dpLogHeader "dp"
#   define dpWithTDisasm
#   define dpWithObjFile
#   define dpWithLibFile
#   define dpWithDllFile
#   define dpWithElfFile
#endif // alcImpl

#endif // dpFeatures_h
