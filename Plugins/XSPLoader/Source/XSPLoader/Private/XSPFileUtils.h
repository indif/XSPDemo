// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XSPDataStruct.h"
#include <fstream>


void ReadHeaderInfo(std::fstream& file, Header_info& info);

void ReadHeaderInfo(std::fstream& file, int nsize, TArray<Header_info>& header_list);

void ReadNodeData(std::fstream& file, const Header_info& header, FXSPNodeData& NodeData);

void ReadPrimitiveData(std::fstream& file, const Header_info& header, FXSPPrimitiveData& PrimitiveData);


