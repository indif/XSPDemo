#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"

class FMaterial;
class FSceneView;
struct FMeshBatchElement;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FXSPVertexFactoryUniformShaderParameters, )
SHADER_PARAMETER(FIntVector4, VertexFetch_Parameters)
SHADER_PARAMETER(int32, PreSkinBaseVertexIndex)
SHADER_PARAMETER(uint32, LODLightmapDataIndex)
SHADER_PARAMETER_SRV(Buffer<float2>, VertexFetch_TexCoordBuffer)
SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PositionBuffer)
SHADER_PARAMETER_SRV(Buffer<float>, VertexFetch_PreSkinPositionBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_PackedTangentsBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VertexFetch_ColorComponentsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FXSPVertexFactoryLooseParameters, )
SHADER_PARAMETER_SRV(Buffer<float>, GPUSkinPassThroughPreviousPositionBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern TUniformBufferRef<FXSPVertexFactoryUniformShaderParameters> CreateLocalVFUniformBuffer(
	const class FXSPVertexFactory* VertexFactory,
	uint32 LODLightmapDataIndex,
	class FColorVertexBuffer* OverrideColorVertexBuffer,
	int32 BaseVertexIndex,
	int32 PreSkinBaseVertexIndex
);

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
	{
	}

	struct FDataType : public FStaticMeshDataType
	{
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData);

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
		return GNullColorVertexBuffer.VertexBufferSRV.GetReference();
	}

	inline FRHIShaderResourceView* GetTangentsSRV() const
	{
		return Data.TangentsSRV;
	}

	inline FRHIShaderResourceView* GetTextureCoordinatesSRV() const
	{
		return Data.TextureCoordinatesSRV;
	}

	inline FRHIShaderResourceView* GetColorComponentsSRV() const
	{
		return Data.ColorComponentsSRV;
	}

	inline const uint32 GetColorIndexMask() const
	{
		return Data.ColorIndexMask;
	}

	inline const int GetLightMapCoordinateIndex() const
	{
		return Data.LightMapCoordinateIndex;
	}

	inline const int GetNumTexcoords() const
	{
		return Data.NumTexCoords;
	}

	FRHIUniformBuffer* GetUniformBuffer() const
	{
		return UniformBuffer.GetReference();
	}

protected:
	friend class FXSPVertexFactoryShaderParameters;
	friend class FSkeletalMeshSceneProxy;

	const FDataType& GetData() const { return Data; }

	FDataType Data;
	TUniformBufferRef<FXSPVertexFactoryUniformShaderParameters> UniformBuffer;
	TUniformBufferRef<FXSPVertexFactoryLooseParameters> LooseParametersUniformBuffer;

	int32 ColorStreamIndex;

	bool bGPUSkinPassThrough = false;

	//FVertexStreamComponent PositionComponent;
	//FRHIShaderResourceView* PositionComponentSRV = nullptr;
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
	void GetElementShaderBindingsGPUSkinPassThrough(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

	void GetElementShaderBindingsSkinCache(
		class FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory,
		struct FGPUSkinBatchElementUserData* BatchUserData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

	void GetElementShaderBindingsMeshDeformer(
		class FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

	LAYOUT_FIELD(FShaderResourceParameter, GPUSkinCachePositionBuffer);
	LAYOUT_FIELD(FShaderParameter, IsGPUSkinPassThrough);
};