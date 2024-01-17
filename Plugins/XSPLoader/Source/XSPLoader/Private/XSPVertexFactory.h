// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"
#include "GlobalRenderResources.h"

class FMaterial;
class FSceneView;
struct FMeshBatchElement;

/*=============================================================================
	LocalVertexFactory.h: Local vertex factory definitions.
=============================================================================*/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FXSPVertexFactoryUniformShaderParameters,)
	SHADER_PARAMETER(FIntVector4,VertexFetch_Parameters)
	SHADER_PARAMETER(int32, PreSkinBaseVertexIndex)
	SHADER_PARAMETER(uint32,LODLightmapDataIndex)
	SHADER_PARAMETER_SRV(Buffer<float2>, VertexFetch_TexCoordBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PreSkinPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_PackedTangentsBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ColorComponentsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FXSPVertexFactoryLooseParameters,)
	SHADER_PARAMETER(uint32, FrameNumber)
	SHADER_PARAMETER_SRV(Buffer<float>, GPUSkinPassThroughPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, GPUSkinPassThroughPreviousPositionBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

//extern ENGINE_API TUniformBufferRef<FXSPVertexFactoryUniformShaderParameters> CreateLocalVFUniformBuffer(
//	const class FXSPVertexFactory* VertexFactory, 
//	uint32 LODLightmapDataIndex, 
//	class FColorVertexBuffer* OverrideColorVertexBuffer, 
//	int32 BaseVertexIndex,
//	int32 PreSkinBaseVertexIndex
//	);

struct FXSPDataType : public FStaticMeshDataType
{
	FVertexStreamComponent PreSkinPositionComponent;
	FRHIShaderResourceView* PreSkinPositionComponentSRV = nullptr;
#if WITH_EDITORONLY_DATA
	const class UStaticMesh* StaticMesh = nullptr;
	bool bIsCoarseProxy = false;
#endif
};

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class FXSPVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FXSPVertexFactory);
public:

	FXSPVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName)
		: FVertexFactory(InFeatureLevel)
		, ColorStreamIndex(INDEX_NONE)
		, DebugName(InDebugName)
	{
	}

	//struct FDataType : public FStaticMeshDataType
	//{
	//	FVertexStreamComponent PreSkinPositionComponent;
	//	FRHIShaderResourceView* PreSkinPositionComponentSRV = nullptr;
	//#if WITH_EDITORONLY_DATA
	//	const class UStaticMesh* StaticMesh = nullptr;
	//	bool bIsCoarseProxy = false;
	//#endif
	//};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);
	static void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, EVertexInputStreamType InputStreamType, bool bSupportsManualVertexFetch, FXSPDataType& Data, FVertexDeclarationElementList& Elements);

	/** 
	 * Does the platform support GPUSkinPassthrough permutations.
	 * This knowledge can be used to indicate if we need to create SRV for index/vertex buffers.
	 */
	static bool IsGPUSkinPassThroughSupported(EShaderPlatform Platform);

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FXSPDataType& InData);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FXSPVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override
	{
		UniformBuffer.SafeRelease();
		LooseParametersUniformBuffer.SafeRelease();
		FVertexFactory::ReleaseRHI();
	}

	FORCEINLINE_DEBUGGABLE void SetColorOverrideStream(FRHICommandList& RHICmdList, const FVertexBuffer* ColorVertexBuffer) const
	{
		checkf(ColorVertexBuffer->IsInitialized(), TEXT("Color Vertex buffer was not initialized! Name %s"), *ColorVertexBuffer->GetFriendlyName());
		checkf(IsInitialized() && EnumHasAnyFlags(EVertexStreamUsage::Overridden, Data.ColorComponent.VertexStreamUsage) && ColorStreamIndex > 0, TEXT("Per-mesh colors with bad stream setup! Name %s"), *ColorVertexBuffer->GetFriendlyName());
		RHICmdList.SetStreamSource(ColorStreamIndex, ColorVertexBuffer->VertexBufferRHI, 0);
	}

	void GetColorOverrideStream(const FVertexBuffer* ColorVertexBuffer, FVertexInputStreamArray& VertexStreams) const
	{
		checkf(ColorVertexBuffer->IsInitialized(), TEXT("Color Vertex buffer was not initialized! Name %s"), *ColorVertexBuffer->GetFriendlyName());
		checkf(IsInitialized() && EnumHasAnyFlags(EVertexStreamUsage::Overridden, Data.ColorComponent.VertexStreamUsage) && ColorStreamIndex > 0, TEXT("Per-mesh colors with bad stream setup! Name %s"), *ColorVertexBuffer->GetFriendlyName());

		VertexStreams.Add(FVertexInputStream(ColorStreamIndex, 0, ColorVertexBuffer->VertexBufferRHI));
	}

	inline FRHIShaderResourceView* GetPositionsSRV() const
	{
		return Data.PositionComponentSRV;
	}

	inline FRHIShaderResourceView* GetPreSkinPositionSRV() const
	{
		return Data.PreSkinPositionComponentSRV ? Data.PreSkinPositionComponentSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference();
	}

	inline FRHIShaderResourceView* GetTangentsSRV() const
	{
		return Data.TangentsSRV;
	}

	inline FRHIShaderResourceView* GetTextureCoordinatesSRV() const
	{
		return GNullColorVertexBuffer.VertexBufferSRV.GetReference();//Data.TextureCoordinatesSRV;
	}

	inline FRHIShaderResourceView* GetColorComponentsSRV() const
	{
		return GNullColorVertexBuffer.VertexBufferSRV.GetReference();//Data.ColorComponentsSRV;
	}

	inline const uint32 GetColorIndexMask() const
	{
		return 0;//Data.ColorIndexMask;
	}

	inline const int GetLightMapCoordinateIndex() const
	{
		return 0;//Data.LightMapCoordinateIndex;
	}

	inline const int GetNumTexcoords() const
	{
		return 0;// Data.NumTexCoords;
	}

	FRHIUniformBuffer* GetUniformBuffer() const
	{
		return UniformBuffer.GetReference();
	}

#if WITH_EDITORONLY_DATA
	virtual bool IsCoarseProxyMesh() const override { return Data.bIsCoarseProxy; }

	inline const class UStaticMesh* GetStaticMesh() const { return Data.StaticMesh; }
#endif

protected:
	friend class FXSPVertexFactoryShaderParameters;
	friend class FSkeletalMeshSceneProxy;

	const FXSPDataType& GetData() const { return Data; }
	
	static void GetVertexElements(
		ERHIFeatureLevel::Type FeatureLevel, 
		EVertexInputStreamType InputStreamType, 
		bool bSupportsManualVertexFetch,
		FXSPDataType& Data,
		FVertexDeclarationElementList& Elements, 
		FVertexStreamList& InOutStreams, 
		int32& OutColorStreamIndex);

	FXSPDataType Data;
	TUniformBufferRef<FXSPVertexFactoryUniformShaderParameters> UniformBuffer;
	TUniformBufferRef<FXSPVertexFactoryLooseParameters> LooseParametersUniformBuffer;

	int32 ColorStreamIndex;

	bool bGPUSkinPassThrough = false;

	struct FDebugName
	{
		FDebugName(const char* InDebugName)
#if !UE_BUILD_SHIPPING
			: DebugName(InDebugName)
#endif
		{}
	private:
#if !UE_BUILD_SHIPPING
		const char* DebugName;
#endif
	} DebugName;
};

/**
 * Shader parameters for all LocalVertexFactory derived classes.
 */
class FXSPVertexFactoryShaderParametersBase : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FXSPVertexFactoryShaderParametersBase, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	void GetElementShaderBindingsBase(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FRHIUniformBuffer* VertexFactoryUniformBuffer,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
		) const;

	FXSPVertexFactoryShaderParametersBase()
		: bAnySpeedTreeParamIsBound(false)
	{
	}

	// SpeedTree LOD parameter
	LAYOUT_FIELD(FShaderParameter, LODParameter);

	// True if LODParameter is bound, which puts us on the slow path in GetElementShaderBindings
	LAYOUT_FIELD(bool, bAnySpeedTreeParamIsBound);
};

/** Shader parameter class used by FXSPVertexFactory only - no derived classes. */
class FXSPVertexFactoryShaderParameters : public FXSPVertexFactoryShaderParametersBase
{
	DECLARE_TYPE_LAYOUT(FXSPVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const; 

private:
	LAYOUT_FIELD(FShaderParameter, IsGPUSkinPassThrough);
};