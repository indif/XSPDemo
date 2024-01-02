// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "IXSPLoader.h"

class FXSPLoaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//设置自动计算动态合并打包参数(缺省为启用自定计算)
	XSPLOADER_API void SetAutoComputeBatchParams(bool bAutoCompute, int32 MaxNumBatches);

	//设置动态合并打包参数：每个包最大顶点数、每个包最小顶点数、不参与合包的最小顶点数
	XSPLOADER_API void SetBatchParams(int32 XSPMaxNumVerticesPerBatch, int32 XSPMinNumVerticesPerBatch, int32 XSPMinNumVerticesUnbatch);

	XSPLOADER_API IXSPLoader& Get() const;

private:
	FTSTicker::FDelegateHandle TickerHandle;
	IXSPLoader* XSPLoader = nullptr;
};
