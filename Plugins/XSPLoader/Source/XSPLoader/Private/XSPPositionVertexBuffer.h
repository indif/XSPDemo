// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"

struct FStaticMeshBuildVertex;

/** A vertex that stores just position. */
struct FXSPPositionVertex
{
	FVector3f	Position;

	friend FArchive& operator<<(FArchive& Ar, FXSPPositionVertex& V)
	{
		Ar << V.Position;
		return Ar;
	}
};

/** A vertex buffer of positions. */
class FXSPPositionVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FXSPPositionVertexBuffer();

	/** Destructor. */
	~FXSPPositionVertexBuffer();

	/** Delete existing resources */
	void CleanUp();

	void Init(uint32 NumVertices, bool bInNeedsCPUAccess = true);

	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}

	/** Create an RHI vertex buffer with CPU data. CPU data may be discarded after creation (see TResourceArray::Discard) */
	FBufferRHIRef CreateRHIBuffer_RenderThread();
	FBufferRHIRef CreateRHIBuffer_Async();

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("PositionOnly Static-mesh vertices"); }

	void BindPositionVertexBuffer(const class FVertexFactory* VertexFactory, struct FStaticMeshDataType& Data) const;

	void* GetVertexData() { return Data; }

private:

	FShaderResourceViewRHIRef PositionComponentSRV;

	/** The vertex data storage type */
	TMemoryImagePtr<class FXSPPositionVertexData> VertexData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	bool bNeedsCPUAccess = true;

	/** Allocates the vertex data storage type. */
	void AllocateData(bool bInNeedsCPUAccess = true);

	template <bool bRenderThread>
	FBufferRHIRef CreateRHIBuffer_Internal();
};

