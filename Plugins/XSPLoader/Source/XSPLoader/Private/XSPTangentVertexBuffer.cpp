// Copyright Epic Games, Inc. All Rights Reserved.

#include "XSPTangentVertexBuffer.h"

#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"

#include "StaticMeshVertexData.h"
#include "GPUSkinCache.h"

#include "XSPVertexFactory.h"

/*-----------------------------------------------------------------------------
FXSPTangentVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FXSPTangentVertexData :
	public TStaticMeshVertexData<FXSPTangentVertex>
{
public:
	FXSPTangentVertexData(bool InNeedsCPUAccess = false)
		: TStaticMeshVertexData<FXSPTangentVertex>(InNeedsCPUAccess)
	{
	}
};


FXSPTangentVertexBuffer::FXSPTangentVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FXSPTangentVertexBuffer::~FXSPTangentVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FXSPTangentVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = nullptr;
	}
}

void FXSPTangentVertexBuffer::Init(uint32 InNumVertices, bool bInNeedsCPUAccess)
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
FBufferRHIRef FXSPTangentVertexBuffer::CreateRHIBuffer_Internal()
{
	return CreateRHIBuffer<bRenderThread>(VertexData, NumVertices, BUF_Static | BUF_ShaderResource, TEXT("FXSPTangentVertexBuffer"));
}

FBufferRHIRef FXSPTangentVertexBuffer::CreateRHIBuffer_RenderThread()
{
	return CreateRHIBuffer_Internal<true>();
}

FBufferRHIRef FXSPTangentVertexBuffer::CreateRHIBuffer_Async()
{
	return CreateRHIBuffer_Internal<false>();
}

void FXSPTangentVertexBuffer::InitRHI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXSPTangentVertexBuffer::InitRHI);

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
			TangentComponentSRV = RHICreateShaderResourceView(FShaderResourceViewInitializer(
				bHadVertexData ? VertexBufferRHI : nullptr, PF_R8G8B8A8_SNORM));
		}
	}
}

void FXSPTangentVertexBuffer::ReleaseRHI()
{
	TangentComponentSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FXSPTangentVertexBuffer::AllocateData( bool bInNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FXSPTangentVertexData(bInNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FXSPTangentVertexBuffer::BindTangentVertexBuffer(const FVertexFactory* VertexFactory, FXSPDataType& XSPData) const
{
	//XSPData.XSPTangentXComponent = XSPData.XSPTangentZComponent = FVertexStreamComponent(
	//	this,
	//	STRUCT_OFFSET(FXSPTangentVertex, TangentZ),
	//	GetStride(),
	//	VET_PackedNormal,
	//	EVertexStreamUsage::ManualFetch
	//);
	//XSPData.XSPTangentComponentSRV = TangentComponentSRV;
}
