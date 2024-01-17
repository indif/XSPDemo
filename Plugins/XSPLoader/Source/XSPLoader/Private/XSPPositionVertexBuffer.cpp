// Copyright Epic Games, Inc. All Rights Reserved.

#include "XSPPositionVertexBuffer.h"

#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"

#include "StaticMeshVertexData.h"
#include "GPUSkinCache.h"

#include "XSPVertexFactory.h"

/*-----------------------------------------------------------------------------
FXSPPositionVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FXSPPositionVertexData :
	public TStaticMeshVertexData<FXSPPositionVertex>
{
public:
	FXSPPositionVertexData(bool InNeedsCPUAccess = false)
		: TStaticMeshVertexData<FXSPPositionVertex>(InNeedsCPUAccess)
	{
	}
};


FXSPPositionVertexBuffer::FXSPPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FXSPPositionVertexBuffer::~FXSPPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FXSPPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = nullptr;
	}
}

void FXSPPositionVertexBuffer::Init(uint32 InNumVertices, bool bInNeedsCPUAccess)
{
	NumVertices = InNumVertices;
	bNeedsCPUAccess = bInNeedsCPUAccess;

	// Allocate the vertex data storage type.
	AllocateData(bInNeedsCPUAccess);

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
}

template <bool bRenderThread>
FBufferRHIRef FXSPPositionVertexBuffer::CreateRHIBuffer_Internal()
{
	return CreateRHIBuffer<bRenderThread>(VertexData, NumVertices, BUF_Static | BUF_ShaderResource, TEXT("FXSPPositionVertexBuffer"));
}

FBufferRHIRef FXSPPositionVertexBuffer::CreateRHIBuffer_RenderThread()
{
	return CreateRHIBuffer_Internal<true>();
}

FBufferRHIRef FXSPPositionVertexBuffer::CreateRHIBuffer_Async()
{
	return CreateRHIBuffer_Internal<false>();
}

void FXSPPositionVertexBuffer::InitRHI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXSPPositionVertexBuffer::InitRHI);

	const bool bHadVertexData = VertexData != nullptr;
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
	// Also check to see whether cpu access has been activated on the vertex data
	if (VertexBufferRHI)
	{
		// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
		bool bSRV = RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable(GMaxRHIShaderPlatform);

		// When bAllowCPUAccess is true, the meshes is likely going to be used for Niagara to spawn particles on mesh surface.
		// And it can be the case for CPU *and* GPU access: no differenciation today. That is why we create a SRV in this case.
		// This also avoid setting lots of states on all the members of all the different buffers used by meshes. Follow up: https://jira.it.epicgames.net/browse/UE-69376.
		bSRV |= (VertexData && VertexData->GetAllowCPUAccess());
		if(bSRV)
		{
			// When VertexData is null, this buffer hasn't been streamed in yet. We still need to create a FRHIShaderResourceView which will be
			// cached in a vertex factory uniform buffer later. The nullptr tells the RHI that the SRV doesn't view on anything yet.
			PositionComponentSRV = RHICreateShaderResourceView(FShaderResourceViewInitializer(bHadVertexData ? VertexBufferRHI : nullptr, PF_R32_FLOAT));
		}
	}
}

void FXSPPositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FXSPPositionVertexBuffer::AllocateData( bool bInNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FXSPPositionVertexData(bInNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FXSPPositionVertexBuffer::BindPositionVertexBuffer(const FVertexFactory* VertexFactory, FXSPDataType& XSPData) const
{
	XSPData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FXSPPositionVertex, Position),
		GetStride(),
		VET_Float3
	);
	XSPData.PositionComponentSRV = PositionComponentSRV;
}
