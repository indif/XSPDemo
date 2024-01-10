// Copyright Epic Games, Inc. All Rights Reserved.
#include "XSPLoaderModule.h"
#include "XSPLoader.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FXSPLoaderModule"


bool bXSPAutoComputeBatchParams = true;
int32 XSPMaxNumBatches = 3000;
int32 XSPMaxNumVerticesPerBatch = 300;      //每个包最大顶点数
int32 XSPMinNumVerticesPerBatch = 100;	    //每个包最小顶点数
int32 XSPMinNumVerticesUnbatch = 100;		//不参与合包的最小顶点数

void FXSPLoaderModule::StartupModule()
{
	FXSPLoader* TempXSPLoader = new FXSPLoader;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("XSPLoader"), 0.0f, [TempXSPLoader](float DeltaTime)
		{
			TempXSPLoader->Tick(DeltaTime);
			return true;
		});
	XSPLoader = TempXSPLoader;

	const FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("XSPLoader"))->GetBaseDir(), TEXT("Shaders"), TEXT("Private"));
	AddShaderSourceDirectoryMapping("/Plugin/XSPLoader", ShaderDir);
}

void FXSPLoaderModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	delete XSPLoader;
	XSPLoader = nullptr;
}

void FXSPLoaderModule::SetAutoComputeBatchParams(bool bAutoCompute, int32 MaxNumBatches)
{
	bXSPAutoComputeBatchParams = bAutoCompute;
	XSPMaxNumBatches = MaxNumBatches;
}

void FXSPLoaderModule::SetBatchParams(int32 InMaxNumVerticesPerBatch, int32 InMinNumVerticesPerBatch, int32 InMinNumVerticesUnbatch)
{
	XSPMaxNumVerticesPerBatch = InMaxNumVerticesPerBatch;
	XSPMinNumVerticesPerBatch = InMinNumVerticesPerBatch;
	XSPMinNumVerticesUnbatch = InMinNumVerticesUnbatch;
}

IXSPLoader& FXSPLoaderModule::Get() const
{
	return *XSPLoader;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FXSPLoaderModule, XSPLoader)