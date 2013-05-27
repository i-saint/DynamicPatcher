// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"

dpBuilder::dpBuilder()
{
}

dpBuilder::~dpBuilder()
{
}

void dpBuilder::setConfig()
{

}

bool dpBuilder::startAutoCompile()
{
    return false;
}

bool dpBuilder::stopAutoCompile()
{
    return false;
}



dpCLinkage dpAPI dpBuilder* dpCreateBuilder()
{
    return new dpBuilder();
}
