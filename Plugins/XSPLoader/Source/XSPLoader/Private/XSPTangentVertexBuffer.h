// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "PackedNormal.h"

/** A vertex that stores just position. */
struct FXSPTangentVertex
{
	//FPackedNormal TangentX;
	FPackedNormal TangentZ;

	friend FArchive& operator<<(FArchive& Ar, FXSPTangentVertex& V)
	{
		Ar << /*V.TangentX << */V.TangentZ;
		return Ar;
	}
};

/** A vertex buffer of positions. */
class FXSPTangentVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FXSPTangentVertexBuffer();

	/** Destructor. */
	~FXSPTangentVertexBuffer();

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

	void BindTangentVertexBuffer(const class FVertexFactory* VertexFactory, struct FXSPDataType& Data) const;

	void* GetVertexData() { return Data; }

private:

	FShaderResourceViewRHIRef TangentComponentSRV;

	/** The vertex data storage type */
	TMemoryImagePtr<class FXSPTangentVertexData> VertexData;

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

