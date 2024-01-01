#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XSPModelActor.generated.h"


UCLASS()
class XSPLOADER_API AXSPModelActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AXSPModelActor();
	~AXSPModelActor();

	//从文件加载模型
	UFUNCTION(BlueprintCallable)
	bool Load(const TArray<FString>& FilePathNameArray, bool bAsyncBuild=false);

	//查询拾取到的节点DBID
	UFUNCTION(BlueprintCallable)
	int32 GetNode(UPrimitiveComponent* Component, int32 FaceIndex);

	//查询节点包围盒
	UFUNCTION(BlueprintCallable)
	FBox3f GetNodeBoundingBox(int32 Dbid);

	//设置渲染指定对象的自定义模板值(以便在后处理材质中做描边等屏幕效果)
	UFUNCTION(BlueprintCallable)
	void SetRenderCustomDepthStencil(int32 Dbid, int32 CustomDepthStencilValue);
	UFUNCTION(BlueprintCallable)
	void SetRenderCustomDepthStencilArray(const TArray<int32>& DbidArray, int32 CustomDepthStencilValue);

	//清除自定义模板值渲染
	UFUNCTION(BlueprintCallable)
	void ClearRenderCustomDepthStencil(int32 Dbid);
	UFUNCTION(BlueprintCallable)
	void ClearRenderCustomDepthStencilArray(const TArray<int32>& DbidArray);

	//设置指定对象的显隐
	UFUNCTION(BlueprintCallable)
	void SetVisibility(int32 Dbid, bool bVisible);
	UFUNCTION(BlueprintCallable)
	void SetVisibilityArray(const TArray<int32>& DbidArray, bool bVisible);

	//设置着色(支持半透明)
	UFUNCTION(BlueprintCallable)
	void SetRenderColor(int32 Dbid, const FLinearColor& Color);
	UFUNCTION(BlueprintCallable)
	void SetRenderColorArray(const TArray<int32>& DbidArray, const FLinearColor& Color);

	//清除着色
	UFUNCTION(BlueprintCallable)
	void ClearRenderColor(int32 Dbid);
	UFUNCTION(BlueprintCallable)
	void ClearRenderColorArray(const TArray<int32>& DbidArray);

public:
	//加载完成事件(参数: 0-初始化加载,1-动态更新)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLoadFinishDelegate, int32);

	FOnLoadFinishDelegate& GetOnLoadFinishDelegate();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	friend class AXSPSubModelActor;
	inline const TArray<struct FXSPNodeData*>& GetNodeDataArray() const { return NodeDataArray; }
	UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(const FLinearColor& BaseColor, float Roughness, const FLinearColor& EmissiveColor);

	bool LoadToDynamicCombinedMesh(const TArray<FString>& FilePathNameArray);
	void TickDynamicCombine(float AvailableSeconds);
	bool FinishLoadNodeData();
	void ComputeBatchParams();
	void InitSubModelActors();
	bool UpdateOperation();

	class AXSPSubModelActor* GetSubModelActor(int32 Dbid);
	TArray<int32> GetSubArray(const TArray<int32>& DbidArray, int32 Start, int32 Num);

private:
	UPROPERTY()
	UMaterialInterface* SourceMaterialOpaque = nullptr;

	UPROPERTY()
	UMaterialInterface* SourceMaterialTranslucent = nullptr;

	UPROPERTY()
	TMap<int32, class AXSPSubModelActor*> SubModelActorMap;

	bool bAsyncBuildWhenInitLoading = true;

	TArray<TSharedPtr<class FXSPFileReader>> FileReaderArray;

	TArray<struct FXSPNodeData*> NodeDataArray;
	TArray<int32> LevelOneNodeIdArray;
	TArray<int32> LeafNodeIdArray;
	int32 NumVerticesTotal = 0;

	enum class EState : uint8
	{
		Empty,			//未初始化
		ReadingFile,	//读文件
		InitLoading,	//初始加载模型
		Finished,		//加载完毕的稳定显示状态
		Updating		//动态更新中
	};
	EState State = EState::Empty;
	FOnLoadFinishDelegate OnLoadFinishDelegate;

	int64 OperationBeginTicks;
};
