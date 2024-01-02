// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <fstream>

//几何体类型
enum class EXSPPrimitiveType : uint8
{
	Unknown,
	Mesh,
	Elliptical,
	Cylinder
};

//从文件读入的原始几何体数据
struct FXSPPrimitiveData
{
	EXSPPrimitiveType Type;

	//读入的原始材质数据
	float Material[4];
	
	//参数化几何体的参数
	float* PrimitiveParamsBuffer;
	uint8 PrimitiveParamsBufferLength;

	//网格体的顶点数据
	float* MeshVertexBuffer;
	float* MeshNormalBuffer;
	int32 MeshVertexBufferLength;

	FXSPPrimitiveData()
		: Type(EXSPPrimitiveType::Unknown)
		, PrimitiveParamsBuffer(nullptr)
		, PrimitiveParamsBufferLength(0)
		, MeshVertexBuffer(nullptr)
		, MeshNormalBuffer(nullptr)
		, MeshVertexBufferLength(0)
	{}
	~FXSPPrimitiveData()
	{
		if (PrimitiveParamsBuffer != nullptr)
			delete[] PrimitiveParamsBuffer;
		if (MeshVertexBuffer != nullptr)
			delete[] MeshVertexBuffer;
		if (MeshNormalBuffer != nullptr)
			delete[] MeshNormalBuffer;
	}
};

//节点数据
struct FXSPNodeData
{
	//节点dbid
	int32 Dbid;

	//父节点dbid
	int32 ParentDbid;

	//节点层级,根节点是0
	int32 Level;

	//子节点数量(子节点包含自己,id编号连续)
	int32 NumChildren;

	//读入的原始材质数据
	float Material[4];

	//读入的原始几何体数据(生成网格数据后就释放掉)
	TArray<FXSPPrimitiveData> PrimitiveArray;

	//生成的材质数据
	FLinearColor MeshMaterial;

	//生成的网格体数据(由节点包含的所有原始几何体合并的)
	TArray<FVector3f> MeshPositionArray;
	TArray<FPackedNormal> MeshNormalArray;

	//包围盒
	FBox3f MeshBoundingBox;

	FXSPNodeData()
		: Dbid(-1)
		, ParentDbid(-1)
		, Level(-1)
		, NumChildren(-1)
		, MeshBoundingBox(ForceInit)
	{
	}
};

struct Header_info
{
	//int dbid;  //结构体的索引就是dbid 从0开始
	short empty_fragment;  //1为有fragment 2为没有fragment
	int parentdbid;      //parent db id
	short level;    //node 所在的节点层级 从0开始
	int startname;   //节点名称开始索引
	int namelength;   //节点名称字符大小
	int startproperty;  //节点属性开始索引
	int propertylength;  //节点属性字符大小
	int startmaterial;  //材质属性开始索引，固定16个字符，一次是R(4) G(4) B(4) roughnessFactor(4)
	int startbox;   //box开始索引
	int startvertices;  //vertices开始索引
	int verticeslength;  //vertices头文件大小
	int offset;
};
