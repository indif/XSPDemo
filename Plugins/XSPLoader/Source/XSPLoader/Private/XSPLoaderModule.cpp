// Copyright Epic Games, Inc. All Rights Reserved.
#include "XSPLoaderModule.h"

#define LOCTEXT_NAMESPACE "FXSPLoaderModule"


bool bXSPAutoComputeBatchParams = true;
int32 XSPMaxNumBatches = 3000;
int32 XSPMaxNumVerticesPerBatch = 65536;      //每个包最大顶点数
int32 XSPMinNumVerticesPerBatch = 1000;	      //每个包最小顶点数
int32 XSPMinNumVerticesUnbatch = 1000;		  //不参与合包的最小顶点数

void FXSPLoaderModule::StartupModule()
{
}

void FXSPLoaderModule::ShutdownModule()
{
}

void FXSPLoaderModule::SetAutoComputeBatchParams(bool bAutoCompute, int32 MaxNumBatches)
{
	bXSPAutoComputeBatchParams = bAutoCompute;
	XSPMaxNumBatches = MaxNumBatches;
}

void FXSPLoaderModule::SetBatchParams(int32 InMaxNumVerticesPerBatch, int32 InMinNumVerticesPerBatch, int32 InMinNumVerticesUnbatch)
{
	XSPMaxNumVerticesPerBatch = FMath::Min(InMaxNumVerticesPerBatch, (int32)MAX_uint16+1);
	XSPMinNumVerticesPerBatch = FMath::Min(InMinNumVerticesPerBatch, InMaxNumVerticesPerBatch);
	XSPMinNumVerticesUnbatch = InMinNumVerticesUnbatch;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FXSPLoaderModule, XSPLoader)