#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XSPSubModelActor.generated.h"

UCLASS()
class AXSPSubModelActor : public AActor
{
    GENERATED_BODY()

public:
    AXSPSubModelActor();
    virtual ~AXSPSubModelActor();

    void Init(class AXSPModelActor* Parent, int32 Dbid, int32 Num);

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

	inline const TArray<struct FXSPNodeData*>& GetNodeDataArray() const { return Parent->GetNodeDataArray(); }

	bool TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild);

private:
	class AXSPSubModelMaterialActor* GetOrCreateMaterialActor(const FLinearColor& Material);
	class AXSPSubModelMaterialActor* GetOrCreateStencilActor(int32 CustomDepthStencilValue);
	class AXSPSubModelMaterialActor* GetOrCreateHighlightActor(const FLinearColor& Color);
	TArray<int32> GetChildLeafNodeArray(int32 Dbid);
	TArray<int32> GetChildLeafNodeArray(const TArray<int32>& DbidArray);
	void SetLeafNodeVisibility(int32 Dbid, bool bVisible);

private:
    class AXSPModelActor* Parent = nullptr;

	bool bBuildStaticMeshAsync;

    int32 StartDbid = -1;
    int32 NumNodes = 0;

    //以材质为索引的的图元
    UPROPERTY()
    TMap<FLinearColor, class AXSPSubModelMaterialActor*> MaterialActorMap;

	//以渲染模板值为索引的仅在CustomDepthPass渲染的图元
	UPROPERTY()
	TMap<int32, class AXSPSubModelMaterialActor*> CustomStencilActorMap;

	//以颜色为索引的的高亮着色图元
	UPROPERTY()
	TMap<FLinearColor, class AXSPSubModelMaterialActor*> HighlightActorMap;
};