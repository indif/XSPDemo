#include "MeshUtils.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Math/UnrealMathUtility.h"

int32 XSPEnableMeshClean = 1;
FAutoConsoleVariableRef CVarXSPEnableMeshClean(
    TEXT("xsp.EnableMeshClean"), 
    XSPEnableMeshClean,
    TEXT("Enable/Disable mesh cleanup during read.(default:Enable)")
);

void ComputeNormal(const TArray<FVector3f>& VertexList, TArray<FVector3f>& NormalList, int32 Offset)
{
    int32 NumVertices = VertexList.Num()-Offset;

    const int32 NumTris = NumVertices / 3;
    for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
    {
        FVector3f P[3];
        for (int32 CornerIdx = 0; CornerIdx < 3; CornerIdx++)
        {
            int32 VertIdx = (TriIdx * 3) + CornerIdx;
            P[CornerIdx] = VertexList[Offset+VertIdx];
        }

        const FVector3f Edge21 = P[1] - P[2];
        const FVector3f Edge20 = P[0] - P[2];
        FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
        NormalList[Offset + TriIdx * 3 + 0] = NormalList[Offset + TriIdx * 3 + 1] = NormalList[Offset + TriIdx * 3 + 2] = TriNormal;
    }
}

//网格体
void AppendRawMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    if (vertices.size() < 9 || vertices.size() % 9 != 0)
    {
        checkNoEntry();
        return;
    }

    int32 Offset = VertexList.Num();
    int32 NumMeshVertices = vertices.size() / 3;
    VertexList.AddUninitialized(NumMeshVertices);
    for (size_t j = 0; j < NumMeshVertices; j++)
        VertexList[Offset+j].Set(vertices[j*3 + 1] * 100, vertices[j*3 + 0] * 100, vertices[j*3 + 2] * 100);

    if (nullptr != NormalList)
    {
        NormalList->AddUninitialized(NumMeshVertices);
        ComputeNormal(VertexList, *NormalList, Offset);
    }
}

void AppendRawMesh(float* MeshVertexBuffer, float* MeshNormalBuffer, int32 BufferLength, TArray<FVector3f>& VertexList, TArray<FVector3f>& NormalList, FBox3f& InOutBoundingBox)
{
    if (nullptr == MeshVertexBuffer || BufferLength < 9 || BufferLength % 9 != 0)
    {
        checkNoEntry();
        return;
    }

    int32 Offset = VertexList.Num();

    if (XSPEnableMeshClean > 0)
    {
        int32 NumTriangles = BufferLength / 9;
        for (size_t j = 0; j < NumTriangles; j++)
        {
            FVector3f A(MeshVertexBuffer[j * 9 + 1] * 100, MeshVertexBuffer[j * 9 + 0] * 100, MeshVertexBuffer[j * 9 + 2] * 100);
            FVector3f B(MeshVertexBuffer[j * 9 + 4] * 100, MeshVertexBuffer[j * 9 + 3] * 100, MeshVertexBuffer[j * 9 + 5] * 100);
            FVector3f C(MeshVertexBuffer[j * 9 + 7] * 100, MeshVertexBuffer[j * 9 + 6] * 100, MeshVertexBuffer[j * 9 + 8] * 100);
            if (A == B || A == C || B == C)
                continue;

            VertexList.Add(A);
            VertexList.Add(B);
            VertexList.Add(C);
            if (nullptr != MeshNormalBuffer)
            {
                NormalList.Add(FVector3f(MeshNormalBuffer[j * 9 + 1], MeshNormalBuffer[j * 9 + 0], MeshNormalBuffer[j * 9 + 2]));
                NormalList.Add(FVector3f(MeshNormalBuffer[j * 9 + 4], MeshNormalBuffer[j * 9 + 3], MeshNormalBuffer[j * 9 + 5]));
                NormalList.Add(FVector3f(MeshNormalBuffer[j * 9 + 7], MeshNormalBuffer[j * 9 + 6], MeshNormalBuffer[j * 9 + 8]));
            }
            else
            {
                const FVector3f Edge21 = B - C;
                const FVector3f Edge20 = A - C;
                FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
                NormalList.Add(TriNormal);
                NormalList.Add(TriNormal);
                NormalList.Add(TriNormal);
            }
        }
    }
    else
    {
        int32 NumMeshVertices = BufferLength / 3;
        VertexList.AddUninitialized(NumMeshVertices);
        for (size_t j = 0; j < NumMeshVertices; j++)
        {
            VertexList[Offset + j].Set(MeshVertexBuffer[j * 3 + 1] * 100, MeshVertexBuffer[j * 3 + 0] * 100, MeshVertexBuffer[j * 3 + 2] * 100);
            InOutBoundingBox += VertexList[Offset + j];
        }

        if (nullptr != MeshNormalBuffer)
        {
            for (size_t j = 0; j < NumMeshVertices; j++)
                NormalList[Offset + j].Set(MeshNormalBuffer[j * 3 + 1], MeshNormalBuffer[j * 3 + 0], MeshNormalBuffer[j * 3 + 2]);
        }
        else
        {
            NormalList.AddUninitialized(NumMeshVertices);
            ComputeNormal(VertexList, NormalList, Offset);
        }
    }
}

//椭圆形
void AppendEllipticalMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    if (vertices.size() < 10)
    {
        checkNoEntry();
        return;
    }

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[origin，xVector，yVector，radius]
    FVector3f Origin(vertices[1] * 100, vertices[0] * 100, vertices[2] * 100);
    FVector3f XVector(vertices[4], vertices[3], vertices[5]); //单位方向向量?
    FVector3f YVector(vertices[7], vertices[6], vertices[8]);
    float Radius = vertices[9] * 100;

    FVector3f Normal = (XVector ^ YVector).GetSafeNormal();

    //沿径向的一圈向量
    TArray<FVector3f> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = XVector * Radius * FMath::Sin(DeltaAngle * i) + YVector * Radius * FMath::Cos(DeltaAngle * i);
    }

    //椭圆面
    TArray<FVector3f> EllipticalMeshVertices, EllipticalMeshNormals;
    EllipticalMeshVertices.SetNumUninitialized(NumSegments * 3);
    if (nullptr != NormalList)
        EllipticalMeshNormals.SetNumUninitialized(NumSegments * 3);
    int32 Index = 0;
    for (int32 i = 0; i < NumSegments; i++)
    {
        if (nullptr != NormalList)
            EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index++] = Origin;
        if (nullptr != NormalList)
            EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index++] = Origin + RadialVectors[i + 1];
        if (nullptr != NormalList)
            EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index++] = Origin + RadialVectors[i];
        
    }

    VertexList.Append(EllipticalMeshVertices);
    if (nullptr != NormalList)
        NormalList->Append(EllipticalMeshNormals);
}

void AppendEllipticalMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& VertexList, TArray<FVector3f>& NormalList, FBox3f& InOutBoundingBox)
{
    if (nullptr == PrimitiveParamsBuffer || BufferLength < 10)
    {
        checkNoEntry();
        return;
    }

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[origin，xVector，yVector，radius]
    FVector3f Origin(PrimitiveParamsBuffer[1] * 100, PrimitiveParamsBuffer[0] * 100, PrimitiveParamsBuffer[2] * 100);
    FVector3f XVector(PrimitiveParamsBuffer[4], PrimitiveParamsBuffer[3], PrimitiveParamsBuffer[5]); //单位方向向量?
    FVector3f YVector(PrimitiveParamsBuffer[7], PrimitiveParamsBuffer[6], PrimitiveParamsBuffer[8]);
    float Radius = PrimitiveParamsBuffer[9] * 100;

    FVector3f Normal = (XVector ^ YVector).GetSafeNormal();

    //沿径向的一圈向量
    TArray<FVector3f> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = XVector * Radius * FMath::Sin(DeltaAngle * i) + YVector * Radius * FMath::Cos(DeltaAngle * i);
    }

    //椭圆面
    TArray<FVector3f> EllipticalMeshVertices, EllipticalMeshNormals;
    EllipticalMeshVertices.SetNumUninitialized(NumSegments * 3);
    EllipticalMeshNormals.SetNumUninitialized(NumSegments * 3);
    int32 Index = 0;
    for (int32 i = 0; i < NumSegments; i++)
    {
        EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index] = Origin;
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;

        EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index] = Origin + RadialVectors[i + 1];
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;
        
        EllipticalMeshNormals[Index] = Normal;
        EllipticalMeshVertices[Index] = Origin + RadialVectors[i];
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;
    }

    VertexList.Append(MoveTemp(EllipticalMeshVertices));
    NormalList.Append(MoveTemp(EllipticalMeshNormals));
}

//圆柱体
void AppendCylinderMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    if (vertices.size() < 13)
    {
        checkNoEntry();
        return;
    }

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[topCenter，bottomCenter，xAxis，yAxis，radius]
    FVector3f TopCenter(vertices[1] * 100, vertices[0] * 100, vertices[2] * 100);
    FVector3f BottomCenter(vertices[4] * 100, vertices[3] * 100, vertices[5] * 100);
    //FVector3f DirX(vertices[7] * 100, vertices[6] * 100, vertices[8] * 100);
    //FVector3f DirY(vertices[10] * 100, vertices[9] * 100, vertices[11] * 100);
    float Radius = vertices[12] * 100;

    //轴向
    FVector3f UpDir = TopCenter - BottomCenter;
    float Height = UpDir.Length();
    UpDir.Normalize();

    //计算径向
    FVector3f RightDir;
    if (FMath::Abs(UpDir.Z) > UE_SQRT_3 / 3)
        RightDir.Set(1, 0, 0);
    else
        RightDir.Set(0, 0, 1);
    RightDir.Normalize();
    FVector3f RadialDir = FVector3f::CrossProduct(RightDir, UpDir);
    RadialDir.Normalize();

    //沿径向的一圈向量
    TArray<FVector3f> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = RadialDir.RotateAngleAxisRad(DeltaAngle * i, UpDir) * Radius;
    }

    TArray<FVector3f> CylinderMeshVertices, CylinderMeshNormals;
    CylinderMeshVertices.SetNumUninitialized(NumSegments * 6);
    if (nullptr != NormalList)
        CylinderMeshNormals.SetNumUninitialized(NumSegments * 6);
    int32 Index = 0;
    ////顶面
    //for (int32 i = 0; i < NumSegments; i++)
    //{
    //    CylinderMeshVertices[Index++] = TopCenter;
    //    CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i + 1];
    //    CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
    //}
    ////底面
    //for (int32 i = 0; i < NumSegments; i++)
    //{
    //    CylinderMeshVertices[Index++] = BottomCenter;
    //    CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i];
    //    CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
    //}
    //侧面
    for (int32 i = 0; i < NumSegments; i++)
    {
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i];
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
        if (nullptr != NormalList)
            CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i + 1];
    }

    VertexList.Append(CylinderMeshVertices);
    if (nullptr != NormalList)
        NormalList->Append(CylinderMeshNormals);
}

void AppendCylinderMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& VertexList, TArray<FVector3f>& NormalList, FBox3f& InOutBoundingBox)
{
    if (nullptr == PrimitiveParamsBuffer || BufferLength < 13)
    {
        checkNoEntry();
        return;
    }

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[topCenter，bottomCenter，xAxis，yAxis，radius]
    FVector3f TopCenter(PrimitiveParamsBuffer[1] * 100, PrimitiveParamsBuffer[0] * 100, PrimitiveParamsBuffer[2] * 100);
    FVector3f BottomCenter(PrimitiveParamsBuffer[4] * 100, PrimitiveParamsBuffer[3] * 100, PrimitiveParamsBuffer[5] * 100);
    float Radius = PrimitiveParamsBuffer[12] * 100;

    //轴向
    FVector3f UpDir = TopCenter - BottomCenter;
    float Height = UpDir.Length();
    UpDir.Normalize();

    //计算径向
    FVector3f RightDir;
    if (FMath::Abs(UpDir.Z) > UE_SQRT_3 / 3)
        RightDir.Set(1, 0, 0);
    else
        RightDir.Set(0, 0, 1);
    RightDir.Normalize();
    FVector3f RadialDir = FVector3f::CrossProduct(RightDir, UpDir);
    RadialDir.Normalize();

    //沿径向的一圈向量
    TArray<FVector3f> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = RadialDir.RotateAngleAxisRad(DeltaAngle * i, UpDir) * Radius;
    }

    TArray<FVector3f> CylinderMeshVertices, CylinderMeshNormals;
    CylinderMeshVertices.SetNumUninitialized(NumSegments * 6);
    CylinderMeshNormals.SetNumUninitialized(NumSegments * 6);
    int32 Index = 0;
    //侧面
    for (int32 i = 0; i < NumSegments; i++)
    {
        CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = RadialVectors[i].GetSafeNormal();
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = RadialVectors[i + 1].GetSafeNormal();
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;
    }

    VertexList.Append(MoveTemp(CylinderMeshVertices));
    NormalList.Append(MoveTemp(CylinderMeshNormals));
}

void AppendNodeMesh(const Body_info& Node, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    for (int32 i = 0, i_len = Node.fragment.Num(); i < i_len; i++)
    {
        if (Node.fragment[i].name == "Mesh")
        {
            AppendRawMesh(Node.fragment[i].vertices, VertexList, NormalList);
        }
        else if (Node.fragment[i].name == "Elliptical")
        {
            AppendEllipticalMesh(Node.fragment[i].vertices, VertexList, NormalList);
        }
        else if (Node.fragment[i].name == "Cylinder")
        {
            AppendCylinderMesh(Node.fragment[i].vertices, VertexList, NormalList);
        }
    }
}

void BuildStaticMesh(UStaticMesh* StaticMesh, const TArray<FVector3f>& VertexList, const TArray<FVector3f>* NormalList)
{
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial());

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    FMeshDescriptionBuilder MeshDescBuilder;
    MeshDescBuilder.SetMeshDescription(&MeshDesc);
    MeshDescBuilder.EnablePolyGroups();

    int32 NumVertices = VertexList.Num();
    TArray<FVertexInstanceID> VertexInstanceIDs;
    VertexInstanceIDs.SetNum(NumVertices);
    for (int32 i = 0; i < NumVertices; i++)
    {
        FVertexID VertexID = MeshDescBuilder.AppendVertex(FVector(VertexList[i]));
        VertexInstanceIDs[i] = MeshDescBuilder.AppendInstance(VertexID);
        //MeshDescBuilder.SetInstanceColor(VertexInstanceIDs[i], FVector4f(1, 1, 1, 1));
        if (nullptr != NormalList)
            MeshDescBuilder.SetInstanceNormal(VertexInstanceIDs[i], FVector((*NormalList)[i]));
        else
            MeshDescBuilder.SetInstanceNormal(VertexInstanceIDs[i], FVector(0, 0, 1));
        //MeshDescBuilder.SetInstanceUV(VertexInstanceIDs[i], FVector2D(0, 0));
        //MeshDescBuilder.SetInstanceTangentSpace(VertexInstanceIDs[i], FVector3f(), FVector3f(), true);
    }

    FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();
    int32 NumTriangles = NumVertices / 3;
    for (int32 i = 0; i < NumTriangles; i++)
    {
        MeshDescBuilder.AppendTriangle(VertexInstanceIDs[i * 3 + 0], VertexInstanceIDs[i * 3 + 1], VertexInstanceIDs[i * 3 + 2], PolygonGroup);
    }

    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    BuildParams.bMarkPackageDirty = false;
    BuildParams.bBuildSimpleCollision = false;
    BuildParams.bFastBuild = true;
    BuildParams.bCommitMeshDescription = false;
    BuildParams.PerLODOverrides.SetNum(1);
    BuildParams.PerLODOverrides[0].bUseFullPrecisionUVs = false;
    BuildParams.PerLODOverrides[0].bUseHighPrecisionTangentBasis = false;

    MeshDesc.VertexInstanceAttributes().UnregisterAttribute(MeshAttribute::VertexInstance::Color);
    TArray<const FMeshDescription*> MeshDescPtrs;
    MeshDescPtrs.Emplace(&MeshDesc);

    StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);
}

void BuildStaticMesh1(UStaticMesh* StaticMesh, const TArray<FVector3f>& PositionList, const TArray<FVector3f>* NormalList)
{
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial());
    StaticMesh->SetFlags(RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
    StaticMesh->NeverStream = true;

    StaticMesh->SetRenderData(MakeUnique<FStaticMeshRenderData>());
    FStaticMeshRenderData* StaticMeshRenderData = StaticMesh->GetRenderData();
    StaticMeshRenderData->AllocateLODResources(1);
    StaticMeshRenderData->ScreenSize[0].Default = 1.0f;

    int32 NumVertices = PositionList.Num();
    FStaticMeshLODResources& StaticMeshLODResources = StaticMeshRenderData->LODResources[0];

    TArray<FStaticMeshBuildVertex> StaticMeshBuildVertices;
    StaticMeshBuildVertices.SetNum(NumVertices);
    TArray<uint32> IndexArray;
    IndexArray.SetNum(NumVertices);
    FBox3f RenderDataBox;
    RenderDataBox.Init();
    for (int32 i = 0; i < NumVertices; i++)
    {
        RenderDataBox += PositionList[i];
        StaticMeshBuildVertices[i].Position.Set(PositionList[i].X, PositionList[i].Y, PositionList[i].Z);
        if (nullptr != NormalList)
            StaticMeshBuildVertices[i].TangentZ.Set((*NormalList)[i].X, (*NormalList)[i].Y, (*NormalList)[i].Z);
        IndexArray[i] = i;
    }
    StaticMeshLODResources.VertexBuffers.PositionVertexBuffer.Init(StaticMeshBuildVertices);
    StaticMeshLODResources.VertexBuffers.StaticMeshVertexBuffer.Init(StaticMeshBuildVertices, 1);
    StaticMeshLODResources.IndexBuffer.SetIndices(IndexArray, EIndexBufferStride::Type::Force16Bit);

    FStaticMeshSection& Section = StaticMeshLODResources.Sections.AddDefaulted_GetRef();
    Section.bEnableCollision = true;
    Section.NumTriangles = NumVertices / 3;
    Section.FirstIndex = 0;
    Section.MinVertexIndex = 0;
    Section.MaxVertexIndex = NumVertices - 1;
    Section.MaterialIndex = 0;
    Section.bForceOpaque = false;

    StaticMeshLODResources.bHasDepthOnlyIndices = false;
    StaticMeshLODResources.bHasReversedIndices = false;
    StaticMeshLODResources.bHasReversedDepthOnlyIndices = false;

    StaticMeshRenderData->Bounds = FBoxSphereBounds(FBox(RenderDataBox));

    StaticMesh->InitResources();
    StaticMesh->CalculateExtendedBounds();
}

void BuildStaticMesh(UStaticMesh* StaticMesh, const Body_info& Node)
{
    TArray<FVector3f> VertexList, NormalList;
    AppendNodeMesh(Node, VertexList, &NormalList);
    if (VertexList.Num() < 3 || VertexList.Num() != NormalList.Num())
    {
        checkNoEntry();
        return;
    }
    BuildStaticMesh(StaticMesh, VertexList, &NormalList);
}

bool IsValidMaterial(float material[4])
{
    if (!FMath::IsFinite(material[0]) || !FMath::IsFinite(material[1]) || !FMath::IsFinite(material[2]) || !FMath::IsFinite(material[3]))
        return false;
    if (material[0] < 0 || material[0] > 1 || material[1] < 0 || material[1] > 1 || material[2] < 0 || material[2] > 1)
        return false;
    if (FMath::IsNearlyZero(material[0]) && FMath::IsNearlyZero(material[1]) && FMath::IsNearlyZero(material[2]))
        return false;
    if (material[3] < 0 || material[3] > 1)
        return false;
    return true;
}

void GetMaterial(Body_info* ParentNode, Body_info& Node, FLinearColor& Color, float& Roughness)
{
    if (nullptr != ParentNode && IsValidMaterial(ParentNode->material))
    {
        Color = FLinearColor(ParentNode->material[0], ParentNode->material[1], ParentNode->material[2]);
        Roughness = ParentNode->material[3];
        return;
    }

    if (IsValidMaterial(Node.material))
    {
        Color = FLinearColor(Node.material[0], Node.material[1], Node.material[2]);
        Roughness = Node.material[3];
        return;
    }

    for (int32 i = 0, len = Node.fragment.Num(); i < len; i++)
    {
        if (IsValidMaterial(Node.fragment[i].material))
        {
            Color = FLinearColor(Node.fragment[i].material[0], Node.fragment[i].material[1], Node.fragment[i].material[2]);
            Roughness = Node.fragment[i].material[3];
            return;
        }
    }

    Color = FLinearColor(0.078125f, 0.078125f, 0.078125f);
    Roughness = 1.f;
}

void ResolveMaterial(FXSPNodeData& Node, FXSPNodeData* ParentNode)
{
    if (nullptr != ParentNode && IsValidMaterial(ParentNode->Material))
    {
        Node.MeshMaterial = FLinearColor(ParentNode->Material[0], ParentNode->Material[1], ParentNode->Material[2], ParentNode->Material[3]);
        return;
    }

    if (IsValidMaterial(Node.Material))
    {
        Node.MeshMaterial = FLinearColor(Node.Material[0], Node.Material[1], Node.Material[2], Node.Material[3]);
        return;
    }

    for (FXSPPrimitiveData& PrimitiveData : Node.PrimitiveArray)
    {
        if (IsValidMaterial(PrimitiveData.Material))
        {
            Node.MeshMaterial = FLinearColor(PrimitiveData.Material[0], PrimitiveData.Material[1], PrimitiveData.Material[2], PrimitiveData.Material[3]);
            return;
        }
    }

    Node.MeshMaterial = FLinearColor(0.078125f, 0.078125f, 0.078125f, 1.f);
}

void GetMaterial(Body_info& Node, FLinearColor& Color, float& Roughness)
{
    if (IsValidMaterial(Node.material))
    {
        Color = FLinearColor(Node.material[0], Node.material[1], Node.material[2]);
        Roughness = Node.material[3];
        return;
    }

    for (int32 i = 0, len = Node.fragment.Num(); i < len; i++)
    {
        if (IsValidMaterial(Node.fragment[i].material))
        {
            Color = FLinearColor(Node.fragment[i].material[0], Node.fragment[i].material[1], Node.fragment[i].material[2]);
            Roughness = Node.fragment[i].material[3];
            return;
        }
    }

    Color = FLinearColor(0.078125f, 0.078125f, 0.078125f);
    Roughness = 1.f;
}

void InheritMaterial(Body_info& Node, Body_info& Parent)
{
    if (IsValidMaterial(Parent.material))
    {
        Node.material[0] = Parent.material[0];
        Node.material[1] = Parent.material[1];
        Node.material[2] = Parent.material[2];
        Node.material[3] = Parent.material[3];
    }
}

void InheritMaterial(FXSPNodeData& Node, FXSPNodeData& ParentNode)
{
    if (IsValidMaterial(ParentNode.Material))
    {
        Node.MeshMaterial = FLinearColor(ParentNode.Material[0], ParentNode.Material[1], ParentNode.Material[2], ParentNode.Material[3]);
    }
}

UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(UMaterialInterface* SourceMaterial, const FLinearColor& Color, float Roughness)
{
    UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(SourceMaterial, nullptr);
    MaterialInstanceDynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
    MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    return MaterialInstanceDynamic;
}

bool CheckNode(const Body_info& Node)
{
    bool bValid = false;
    for (int32 j = 0; j < Node.fragment.Num(); j++)
    {
        const Body_info& Fragment = Node.fragment[j];
        if ((Fragment.name == "Mesh") ||
            Fragment.name == "Elliptical" ||
            Fragment.name == "Cylinder")
        {
            bValid = true;
        }
    }
    return bValid;
}

void ResolveNodeData(FXSPNodeData& NodeData, FXSPNodeData* ParentNodeData)
{
    check(NodeData.MeshPositionArray.IsEmpty());
    NodeData.MeshBoundingBox.Init();

    ResolveMaterial(NodeData, ParentNodeData);

    for (FXSPPrimitiveData& PrimitiveData : NodeData.PrimitiveArray)
    {
        switch (PrimitiveData.Type)
        {
        case EXSPPrimitiveType::Mesh:
            AppendRawMesh(PrimitiveData.MeshVertexBuffer, PrimitiveData.MeshNormalBuffer, PrimitiveData.MeshVertexBufferLength,
                NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshBoundingBox);
            break;
        case EXSPPrimitiveType::Elliptical:
            AppendEllipticalMesh(PrimitiveData.PrimitiveParamsBuffer, PrimitiveData.PrimitiveParamsBufferLength,
                NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshBoundingBox);
            break;
        case EXSPPrimitiveType::Cylinder:
            AppendCylinderMesh(PrimitiveData.PrimitiveParamsBuffer, PrimitiveData.PrimitiveParamsBufferLength, 
                NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshBoundingBox);
            break;
        }
    }

    //生成网格数据后释放原始Primitive数据
    NodeData.PrimitiveArray.Empty();
}