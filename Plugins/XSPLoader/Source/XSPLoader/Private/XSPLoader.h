// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IXSPLoader.h"
#include "XSPFileUtils.h"
#include <fstream>


struct FStaticMeshRequest
{
	int32 Dbid;
	float Priority;
	uint64 LastUpdateFrameNumber;
	bool bValid;
	FLinearColor Color;
	float Roughness;
	UStaticMeshComponent* TargetComponent;
	TStrongObjectPtr<UStaticMesh> StaticMesh;
	std::atomic_bool bReleasable;

	FStaticMeshRequest(int32 InDbid, float InPriority, UStaticMeshComponent* InTargetComponent)
		: Dbid(InDbid)
		, Priority(InPriority)
		, LastUpdateFrameNumber(-1)
		, bValid(true)
		, Color(1,1,1)
		, Roughness(1)
		, TargetComponent(InTargetComponent)
		, bReleasable(false)
	{}

	void Invalidate();

	bool IsValid() { return bValid; }

	void SetReleasable() { bReleasable.store(true); }

	void ResetReleasable() { bReleasable.store(false); }

	bool IsReleasable() { return bReleasable; }

	bool IsRequestCurrent(uint64 FrameNumber);
};

struct FSortRequestFunctor
{
	bool operator() (FStaticMeshRequest* Lhs, FStaticMeshRequest* Rhs) const
	{
		if (Lhs->LastUpdateFrameNumber > Rhs->LastUpdateFrameNumber) return true;
		else if (Lhs->LastUpdateFrameNumber < Rhs->LastUpdateFrameNumber) return false;
		else return (Lhs->Priority > Rhs->Priority);
	}
};

struct FRequestQueue
{
public:
	void Add(FStaticMeshRequest* Request);

	void AddNoLock(FStaticMeshRequest* Request);

	void TakeFirst(FStaticMeshRequest*& Request);

	bool IsEmpty();

	void Empty();

	typedef TArray<FStaticMeshRequest*> FRequestList;
	void Swap(FRequestList& RequestList);

	FRequestList RequestList;
	FCriticalSection RequestListCS;

	class FXSPLoader* Loader;
};

class FBuildStaticMeshTask : public FNonAbandonableTask
{
public:
	FBuildStaticMeshTask(FStaticMeshRequest* InRequest, Body_info* InNodeData, FRequestQueue& MergeQueue)
		: Request(InRequest)
		, NodeData(InNodeData)
		, MergeRequestQueue(MergeQueue)
	{
	}

	void DoWork();

	TStatId GetStatId() const
	{
		return TStatId();
	}

private:
	FStaticMeshRequest* Request;
	Body_info* NodeData;
	FRequestQueue& MergeRequestQueue;
};

class FXSPFileLoadRunnalbe : public FRunnable
{
public:
	FXSPFileLoadRunnalbe(class FXSPLoader* Owner, std::fstream& InFileStream, int32 InStartDbid, int32 InCount, FRequestQueue& LoadQueue, FRequestQueue& MergeQueue)
		: Loader(Owner)
		, FileStream(InFileStream)
		, StartDbid(InStartDbid)
		, Count(InCount)
		, LoadRequestQueue(LoadQueue)
		, MergeRequestQueue(MergeQueue)
	{}
	~FXSPFileLoadRunnalbe();

	virtual bool Init() override
	{
		bIsRunning = true;
		return true;
	}
	virtual uint32 Run() override;
	virtual void Stop() override
	{
		bStopRequested = true;
	}
	virtual void Exit() override
	{
		bIsRunning = false;
	}

private:
	TAtomic<bool> bIsRunning = false;
	TAtomic<bool> bStopRequested = false;

	class FXSPLoader* Loader = nullptr;

	std::fstream& FileStream;
	int32 StartDbid = 0;
	int32 Count = 0;
	FRequestQueue& LoadRequestQueue;
	FRequestQueue& MergeRequestQueue;
	TArray<Header_info> HeaderList;
	TMap<int32, Body_info*> BodyMap;
};

class FXSPLoader : public IXSPLoader
{
public:
	FXSPLoader();
	virtual ~FXSPLoader();

	virtual bool Init(const TArray<FString>& FilePathNameArray) override;
	virtual void Reset() override;
	virtual void RequestStaticMesh_GameThread(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent) override;
	virtual void RequestStaticMesh_AnyThread(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent) override;

	void Tick(float DeltaTime);

private:
	void DispatchRequest(FStaticMeshRequest* Request, uint64 InFrameNumber);
	void DispatchNewRequests(uint64 InFrameNumber);
	void ProcessMergeRequests(float AvailableTime);
	void ReleaseRequests();
	void AddToBlacklist(int32 Dbid);
	void ResetInternal();

private:
	bool bInitialized = false;
	std::atomic<uint64> FrameNumber;
	uint64 CurrentFrameNumber;			//gamethread

	//每个源文件对应一个读取线程和一个请求队列
	struct FSourceData
	{
		int32 StartDbid;
		int32 Count;
		std::fstream FileStream;
		FXSPFileLoadRunnalbe* FileLoadRunnable = nullptr;
		FRunnableThread* LoadThread = nullptr;
		FRequestQueue LoadRequestQueue;
	};
	TArray<FSourceData*> SourceDataList;

	// 材质模板
	TStrongObjectPtr<UMaterialInterface> SourceMaterial;

	FRequestQueue MergeRequestQueue;

	/**
	Request的生命周期:
	1.在FXSPLoader::Tick中被创建,根据dbid投入到相应文件对应的LoadRequestQueue	--Game线程
	2.在FXSPFileLoadRunnalbe::Run中被从LoadRequestQueue中取出,(读取节点数据后)填充材质数据,与节点数据一起被封装为一个构建任务分发到线程池	--每个文件对应一个工作线程
	3.在FBuildStaticMeshTask::DoWork中完成网格体构建后,被投入到全局的MergeRequestQueue	--线程池任意线程
	4.在FXSPLoader::Tick中被从MergeRequestQueue中取出,将静态网格设置给组件对象,之后Request被销毁	--Game线程
	在整个声明周期中,无论Request如何流转,AllRequestMap一直持有Request,最终必须确保Request在Game线程释放
	*/

	//全部请求的Map,只在Game线程访问
	TMap<int32, FStaticMeshRequest*> AllRequestMap;

	//没有网格体的节点
	TArray<int32> Blacklist;
	FCriticalSection BlacklistCS;

	//收集新请求的缓存数组,由外部调用线程和Game线程访问
	TArray<FStaticMeshRequest*> CachedRequestArray;
	FCriticalSection CachedRequestArrayCS;

	//所有Request的可更新属性共享同一把锁
	FCriticalSection RequestCS;

	friend struct FRequestQueue;
	friend class FXSPFileLoadRunnalbe;
};
