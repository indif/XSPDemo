#pragma once

#include "CoreMinimal.h"

class AXSPModelActor;
class FXSPSubModelMaterialActor;

class FXSPSubModelActor
{
public:
    FXSPSubModelActor();
    virtual ~FXSPSubModelActor();

    void Init(AXSPModelActor* Owner, int32 Dbid, int32 Num);

	void SetRenderCustomDepthStencil(int32 Dbid, int32 CustomDepthStencilValue);
	void SetRenderCustomDepthStencil(const TArray<int32>& DbidArray, int32 CustomDepthStencilValue);

	void ClearRenderCustomDepthStencil(int32 Dbid);
	void ClearRenderCustomDepthStencil(const TArray<int32>& DbidArray);

	void SetVisibility(int32 Dbid, bool bVisible);
	void SetVisibility(const TArray<int32>& DbidArray, bool bVisible);

	void SetRenderColor(int32 Dbid, const FLinearColor& Color);
	void SetRenderColor(const TArray<int32>& DbidArray, const FLinearColor& Color);

	void ClearRenderColor(int32 Dbid);
	void ClearRenderColor(const TArray<int32>& DbidArray);

	bool TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild);

private:
	FXSPSubModelMaterialActor* GetOrCreateMaterialActor(const FLinearColor& Material);
	FXSPSubModelMaterialActor* GetOrCreateStencilActor(int32 CustomDepthStencilValue);
	FXSPSubModelMaterialActor* GetOrCreateHighlightActor(const FLinearColor& Color);
	TArray<int32> GetChildLeafNodeArray(int32 Dbid);
	TArray<int32> GetChildLeafNodeArray(const TArray<int32>& DbidArray);
	void SetLeafNodeVisibility(int32 Dbid, bool bVisible);

private:
    AXSPModelActor* Owner = nullptr;

    int32 StartDbid = -1;
    int32 NumNodes = 0;

    //以材质为索引的的图元
    TMap<FLinearColor, TSharedPtr<FXSPSubModelMaterialActor>> MaterialActorMap;

	//以渲染模板值为索引的仅在CustomDepthPass渲染的图元
	TMap<int32, TSharedPtr<FXSPSubModelMaterialActor>> CustomStencilActorMap;

	//以颜色为索引的的高亮着色图元
	TMap<FLinearColor, TSharedPtr<FXSPSubModelMaterialActor>> HighlightActorMap;
};