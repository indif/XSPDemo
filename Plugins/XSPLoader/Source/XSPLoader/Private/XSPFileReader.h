#pragma once

#include "CoreMinimal.h"
#include <fstream>
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"


class FXSPFileReader : public FRunnable
{
public:
	FXSPFileReader(class AXSPModelActor* Owner);
	~FXSPFileReader();

	int32 Start(const FString& FilePathName, int32 Offset);

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	bool IsRunning() const;
	bool IsComplete() const;

	TArray<struct FXSPNodeData*>& GetNodeDataArray();
	TArray<int32>& GetLevelOneNodeIdArray();
	TArray<int32>& GetLeafNodeIdArray();
	TArray<int32>& GetLackParentNodeIdArray();
	int32 GetNumVerticesTotal();

private:
	class AXSPModelActor* Owner;
    std::fstream FileStream;
	int32 Offset;
	int32 NumNodes;

	FThreadSafeBool bRunning;
	FThreadSafeBool bComplete;
	FRunnableThread* Thread;

    TArray<struct FXSPNodeData*> NodeDataArray;
	TArray<int32> LevelOneNodeIdArray;
	TArray<int32> LeafNodeIdArray;
	TArray<int32> LackParentNodeIdArray;

	int32 NumVerticesTotal = 0;
};

class FResolveNodeDataTask : public FNonAbandonableTask
{
public:
	FResolveNodeDataTask(FXSPNodeData* NodeData, FXSPNodeData* ParentNodeData);

	void DoWork();

	TStatId GetStatId() const
	{
		return TStatId();
	}

private:
	friend class FXSPFileReader;
	FXSPNodeData* NodeData;
	FXSPNodeData* ParentNodeData;
};