#include "MeshUtils.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Math/UnrealMathUtility.h"

#include "MeshBuild.h"
#include "OverlappingCorners.h"
#include "MeshSimplify/MeshSimplify.h"

#include "XSPStat.h"

bool bXSPEnableMeshClean = true;
FAutoConsoleVariableRef CVarXSPEnableMeshClean(
    TEXT("xsp.EnableMeshClean"), 
    bXSPEnableMeshClean,
    TEXT("是否剔除网格数据中的无效三角形，缺省为否")
);

bool bXSPIgnoreRawMesh = false;
FAutoConsoleVariableRef CVarXSPIgnoreRawMesh(
    TEXT("xsp.IgnoreRawMesh"),
    bXSPIgnoreRawMesh,
    TEXT("是否忽略网格体，缺省为否")
);

bool bXSPIgnoreEllipticalMesh = false;
FAutoConsoleVariableRef CVarXSPIgnoreEllipticalMesh(
    TEXT("xsp.IgnoreEllipticalMesh"),
    bXSPIgnoreEllipticalMesh,
    TEXT("是否忽略椭圆形，缺省为否")
);

bool bXSPIgnoreCylinderMesh = false;
FAutoConsoleVariableRef CVarXSPIgnoreCylinderMesh(
    TEXT("xsp.IgnoreCylinderMesh"),
    bXSPIgnoreCylinderMesh,
    TEXT("是否忽略圆柱体，缺省为否")
);

bool bXSPSimplyRawMesh = false;
FAutoConsoleVariableRef CVarXSPSimplyRawMesh(
    TEXT("xsp.SimplyRawMesh"),
    bXSPSimplyRawMesh,
    TEXT("是否简化网格，缺省为否")
);

int32 XSPSimplyRawMeshMinVertices = 1000;
FAutoConsoleVariableRef CVarXSPSimplyRawMeshMinVertices(
    TEXT("xsp.SimplyRawMesh.MinVertices"),
    XSPSimplyRawMeshMinVertices,
    TEXT("执行网格简化的网格最小顶点数，缺省为1000")
);

float XSPSimplyRawMeshPercentTriangles = 0.5f;
FAutoConsoleVariableRef CVarXSPSimplyRawMeshPercentTriangles(
    TEXT("xsp.SimplyRawMesh.PercentTriangles"),
    XSPSimplyRawMeshPercentTriangles,
    TEXT("简化网格的目标三角形数比，缺省为0.5")
);

float XSPSimplyRawMeshPercentVertices = 0.5f;
FAutoConsoleVariableRef CVarXSPSimplyRawMeshPercentVertices(
    TEXT("xsp.SimplyRawMesh.PercentVertices"),
    XSPSimplyRawMeshPercentVertices,
    TEXT("简化网格的目标顶点数比，缺省为0.5")
);

void ComputeNormal(const TArray<FVector3f>& PositionList, TArray<FPackedNormal>& NormalList, int32 Offset)
{
    int32 NumVertices = PositionList.Num()-Offset;

    const int32 NumTris = NumVertices / 3;
    for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
    {
        FVector3f P[3];
        for (int32 CornerIdx = 0; CornerIdx < 3; CornerIdx++)
        {
            int32 VertIdx = (TriIdx * 3) + CornerIdx;
            P[CornerIdx] = PositionList[Offset+VertIdx];
        }

        const FVector3f Edge21 = P[1] - P[2];
        const FVector3f Edge20 = P[0] - P[2];
        FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
        NormalList[Offset + TriIdx * 3 + 0] = TriNormal;
        NormalList[Offset + TriIdx * 3 + 1] = TriNormal;
        NormalList[Offset + TriIdx * 3 + 2] = TriNormal;
    }
}

void ComputeNormal(const TArray<FVector3f>& PositionList, TArray<FVector3f>& NormalList, int32 Offset)
{
    int32 NumVertices = PositionList.Num() - Offset;

    const int32 NumTris = NumVertices / 3;
    for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
    {
        FVector3f P[3];
        for (int32 CornerIdx = 0; CornerIdx < 3; CornerIdx++)
        {
            int32 VertIdx = (TriIdx * 3) + CornerIdx;
            P[CornerIdx] = PositionList[Offset + VertIdx];
        }

        const FVector3f Edge21 = P[1] - P[2];
        const FVector3f Edge20 = P[0] - P[2];
        FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
        NormalList[Offset + TriIdx * 3 + 0] = TriNormal;
        NormalList[Offset + TriIdx * 3 + 1] = TriNormal;
        NormalList[Offset + TriIdx * 3 + 2] = TriNormal;
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
        VertexList[Offset + j].Set(vertices[j * 3 + 1] * 100, vertices[j * 3 + 0] * 100, vertices[j * 3 + 2] * 100);

    if (nullptr != NormalList)
    {
        NormalList->AddUninitialized(NumMeshVertices);
        ComputeNormal(VertexList, *NormalList, Offset);
    }
}

void AppendRawMesh(float* MeshVertexBuffer, float* MeshNormalBuffer, int32 BufferLength, TArray<FVector3f>& PositionList, TArray<FPackedNormal>& NormalList, TArray<uint32>& IndexList, FBox3f& InOutBoundingBox)
{
    if (nullptr == MeshVertexBuffer || BufferLength < 9 || BufferLength % 9 != 0)
    {
        checkNoEntry();
        return;
    }

    int32 PositionOffset = PositionList.Num();
    int32 IndexOffset = IndexList.Num();

    int32 NumMeshVertices = BufferLength / 3;
    int32 NumMeshTriangles = BufferLength / 9;

    if (bXSPSimplyRawMesh && NumMeshVertices >= XSPSimplyRawMeshMinVertices)
    {
        TArray<FVector3f> Positions;
        Positions.SetNumUninitialized(NumMeshVertices);
        for (int32 i = 0; i < NumMeshVertices; i++)
        {
            Positions[i].Set(MeshVertexBuffer[i * 3 + 1] * 100, MeshVertexBuffer[i * 3 + 0] * 100, MeshVertexBuffer[i * 3 + 2] * 100);
            InOutBoundingBox += Positions[i];
        }
        TArray<FVector3f> SimplifiedPositions;
        TArray<FPackedNormal> SimplifiedNormals;
        TArray<uint32> SimplifiedIndices;
        if (SimplyMesh(Positions, XSPSimplyRawMeshPercentTriangles, XSPSimplyRawMeshPercentVertices, SimplifiedPositions, SimplifiedNormals, SimplifiedIndices))
        {
            PositionList.Append(SimplifiedPositions);
            NormalList.Append(SimplifiedNormals);
            IndexList.Append(SimplifiedIndices);
            if (PositionOffset > 0)
            {
                for (int32 i = 0; i < SimplifiedIndices.Num(); i++)
                {
                    IndexList[IndexOffset + i] += PositionOffset;
                }
            }
            
            INC_DWORD_STAT(STAT_XSPLoader_NumRawMeshSimplified);
            INC_DWORD_STAT_BY(STAT_XSPLoader_NumTotalVerticesSimplied, Positions.Num()-SimplifiedPositions.Num());
            return;
        }
    }

    {
        if (bXSPEnableMeshClean)
        {
            int32 Index = PositionOffset;
            for (size_t j = 0; j < NumMeshTriangles; j++)
            {
                FVector3f A(MeshVertexBuffer[j * 9 + 1] * 100, MeshVertexBuffer[j * 9 + 0] * 100, MeshVertexBuffer[j * 9 + 2] * 100);
                FVector3f B(MeshVertexBuffer[j * 9 + 4] * 100, MeshVertexBuffer[j * 9 + 3] * 100, MeshVertexBuffer[j * 9 + 5] * 100);
                FVector3f C(MeshVertexBuffer[j * 9 + 7] * 100, MeshVertexBuffer[j * 9 + 6] * 100, MeshVertexBuffer[j * 9 + 8] * 100);
                if (A == B || A == C || B == C)
                    continue;

                PositionList.Add(A);
                PositionList.Add(B);
                PositionList.Add(C);
                InOutBoundingBox += A;
                InOutBoundingBox += B;
                InOutBoundingBox += C;

                IndexList.Add(Index++);
                IndexList.Add(Index++);
                IndexList.Add(Index++);

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
            PositionList.AddUninitialized(NumMeshVertices);
            IndexList.AddUninitialized(NumMeshVertices);
            for (size_t j = 0; j < NumMeshVertices; j++)
            {
                PositionList[PositionOffset + j].Set(MeshVertexBuffer[j * 3 + 1] * 100, MeshVertexBuffer[j * 3 + 0] * 100, MeshVertexBuffer[j * 3 + 2] * 100);
                InOutBoundingBox += PositionList[PositionOffset + j];
                IndexList[IndexOffset + j] = PositionOffset + j;
            }

            NormalList.AddUninitialized(NumMeshVertices);
            if (nullptr != MeshNormalBuffer)
            {
                for (size_t j = 0; j < NumMeshVertices; j++)
                {
                    NormalList[PositionOffset + j] = FVector3f(MeshNormalBuffer[j * 3 + 1], MeshNormalBuffer[j * 3 + 0], MeshNormalBuffer[j * 3 + 2]);
                }
            }
            else
            {
                ComputeNormal(PositionList, NormalList, PositionOffset);
            }
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

void AppendEllipticalMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& PositionList, TArray<FPackedNormal>& NormalList, TArray<uint32>& IndexList, FBox3f& InOutBoundingBox)
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
    RadialVectors.SetNumUninitialized(NumSegments);
    for (int32 i = 0; i < NumSegments; i++)
    {
        RadialVectors[i] = XVector * Radius * FMath::Sin(DeltaAngle * i) + YVector * Radius * FMath::Cos(DeltaAngle * i);
    }

    //椭圆面
    TArray<FVector3f> EllipticalMeshVertices;
    TArray<FPackedNormal> EllipticalMeshNormals;
    TArray<uint32> EllipticalMeshIndices;
    EllipticalMeshVertices.SetNumUninitialized(NumSegments + 1);
    EllipticalMeshNormals.SetNumUninitialized(NumSegments + 1);
    EllipticalMeshIndices.SetNumUninitialized(NumSegments * 3);
    EllipticalMeshVertices[0] = Origin;
    EllipticalMeshNormals[0] = Normal;
    for (int32 i = 0; i < NumSegments; i++)
    {
        EllipticalMeshVertices[i+1] = Origin + RadialVectors[i];
        EllipticalMeshNormals[i+1] = Normal;
        InOutBoundingBox += EllipticalMeshVertices[i+1];

        EllipticalMeshIndices[i] = 0;
        EllipticalMeshIndices[i + 1] = (i + 2) % NumSegments;
        EllipticalMeshIndices[i + 2] = i + 1;
    }
    PositionList.Append(MoveTemp(EllipticalMeshVertices));
    NormalList.Append(MoveTemp(EllipticalMeshNormals));
    IndexList.Append(MoveTemp(EllipticalMeshIndices));
}

//圆柱体
bool AppendCylinderMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    if (vertices.size() < 13)
    {
        checkNoEntry();
        return false;
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

    return true;
}

bool AppendCylinderMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& PositionList, TArray<FPackedNormal>& NormalList, TArray<uint32>& IndexList, FBox3f& InOutBoundingBox)
{
    if (nullptr == PrimitiveParamsBuffer || BufferLength < 13)
    {
        checkNoEntry();
        return false;
    }

    //[topCenter，bottomCenter，xAxis，yAxis，radius]
    FVector3f TopCenter(PrimitiveParamsBuffer[1] * 100, PrimitiveParamsBuffer[0] * 100, PrimitiveParamsBuffer[2] * 100);
    FVector3f BottomCenter(PrimitiveParamsBuffer[4] * 100, PrimitiveParamsBuffer[3] * 100, PrimitiveParamsBuffer[5] * 100);
    float Radius = PrimitiveParamsBuffer[12] * 100;
    if (Radius < 0.01f)
        return false;

    int32 NumSegments = Radius > 1.f ? (Radius > 4.f ? (Radius > 10.f ? (Radius > 16.f ? 18 : 12) : 9) : 6) : 3;
    float DeltaAngle = UE_TWO_PI / NumSegments;

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
    RadialVectors.SetNumUninitialized(NumSegments);
    for (int32 i = 0; i < NumSegments; i++)
    {
        RadialVectors[i] = RadialDir.RotateAngleAxisRad(DeltaAngle * i, UpDir) * Radius;
    }

    TArray<FVector3f> CylinderMeshVertices;
    TArray<FPackedNormal> CylinderMeshNormals;
    TArray<uint32> CylinderMeshIndices;
    CylinderMeshVertices.SetNumUninitialized(NumSegments * 2);
    CylinderMeshNormals.SetNumUninitialized(NumSegments * 2);
    CylinderMeshIndices.SetNumUninitialized(NumSegments * 6);
    for (int32 i = 0; i < NumSegments; i++)
    {
        CylinderMeshVertices[i * 2] = TopCenter + RadialVectors[i];
        CylinderMeshVertices[i * 2 + 1] = BottomCenter + RadialVectors[i];

        FVector3f Normal = RadialVectors[i].GetSafeNormal();
        Normal.Normalize();
        CylinderMeshNormals[i * 2] = Normal;
        CylinderMeshNormals[i * 2 + 1] = Normal;

        CylinderMeshIndices[i * 6] = i * 2;
        CylinderMeshIndices[i * 6 + 1] = (i + 1) % NumSegments * 2 + 1;
        CylinderMeshIndices[i * 6 + 2] = i * 2 + 1;
        CylinderMeshIndices[i * 6 + 3] = (i + 1) % NumSegments * 2 + 1;
        CylinderMeshIndices[i * 6 + 4] = i * 2;
        CylinderMeshIndices[i * 6 + 5] = (i + 1) % NumSegments * 2;
    }

    FVector3f Normal, Normal1;
    int32 Index = 0;
    for (int32 i = 0; i < NumSegments; i++)
    {
        Normal = RadialVectors[i].GetSafeNormal();
        Normal1 = RadialVectors[i+1].GetSafeNormal();

        CylinderMeshNormals[Index] = Normal;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = Normal;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = Normal1;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = Normal1;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = Normal;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = Normal1;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;
    }

    PositionList.Append(MoveTemp(CylinderMeshVertices));
    NormalList.Append(MoveTemp(CylinderMeshNormals));

    return true;
}

void AppendNodeMesh(const Body_info& Node, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList)
{
    for (int32 i = 0, i_len = Node.fragment.Num(); i < i_len; i++)
    {
        if (Node.fragment[i].name == "Mesh")
        {
            AppendRawMesh(Node.fragment[i].vertices, VertexList, NormalList);
            INC_DWORD_STAT(STAT_XSPLoader_NumRawMesh);
        }
        else if (Node.fragment[i].name == "Elliptical")
        {
            AppendEllipticalMesh(Node.fragment[i].vertices, VertexList, NormalList);
            INC_DWORD_STAT(STAT_XSPLoader_NumEllipticalMesh);
        }
        else if (Node.fragment[i].name == "Cylinder")
        {
            AppendCylinderMesh(Node.fragment[i].vertices, VertexList, NormalList);
            INC_DWORD_STAT(STAT_XSPLoader_NumCylinderMesh);
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
            if (!bXSPIgnoreRawMesh)
            {
                AppendRawMesh(PrimitiveData.MeshVertexBuffer, PrimitiveData.MeshNormalBuffer, PrimitiveData.MeshVertexBufferLength,
                    NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshIndexArray, NodeData.MeshBoundingBox);
                INC_DWORD_STAT(STAT_XSPLoader_NumRawMesh);
            }
            break;
        case EXSPPrimitiveType::Elliptical:
            if (!bXSPIgnoreEllipticalMesh)
            {
                AppendEllipticalMesh(PrimitiveData.PrimitiveParamsBuffer, PrimitiveData.PrimitiveParamsBufferLength,
                    NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshIndexArray, NodeData.MeshBoundingBox);
                INC_DWORD_STAT(STAT_XSPLoader_NumEllipticalMesh);
            }
            break;
        case EXSPPrimitiveType::Cylinder:
            if (!bXSPIgnoreCylinderMesh)
            {
                AppendCylinderMesh(PrimitiveData.PrimitiveParamsBuffer, PrimitiveData.PrimitiveParamsBufferLength,
                    NodeData.MeshPositionArray, NodeData.MeshNormalArray, NodeData.MeshIndexArray, NodeData.MeshBoundingBox);
                INC_DWORD_STAT(STAT_XSPLoader_NumCylinderMesh);
            }
            break;
        }
    }

    //生成网格数据后释放原始Primitive数据
    NodeData.PrimitiveArray.Empty();
}


void CorrectAttributes(float* Attributes)
{
    FVector3f& Normal = *reinterpret_cast<FVector3f*>(Attributes);
    Normal.Normalize();
}

struct FVertSimp
{
    FVector3f			Position;
    FVector3f			Normal;
    bool Equals(const FVertSimp& a) const
    {
        if (!PointsEqual(Position, a.Position) ||
            !NormalsEqual(Normal, a.Normal))
        {
            return false;
        }
        return true;
    }
};

bool SimplyMesh(const TArray<FVector3f>& InPositions, float PercentTriangles, float PercentVertices, TArray<FVector3f>& OutPositions, TArray<FPackedNormal>& OutNormals, TArray<uint32>& OutIndices)
{
    uint32 NumVertices = InPositions.Num();
    TArray<uint32> InIndices;
    InIndices.SetNumUninitialized(NumVertices);
    for (uint32 i = 0; i < NumVertices; i++)
        InIndices[i] = i;

    float OverlappingThreshold = THRESH_POINTS_ARE_SAME;
    FOverlappingCorners OverlappingCorners(InPositions, InIndices, OverlappingThreshold);

    TArray<FVertSimp> Verts;
    TArray<uint32> Indexes;
    int32 NumTriangles = NumVertices / 3;
    int32 NumWedges = NumTriangles * 3;

    TMap<int32, int32> VertsMap;

    float SurfaceArea = 0.0f;
    int32 WedgeIndex = 0;
    for (int32 i = 0; i < NumTriangles; i++)
    {
        FVector3f CornerPositions[3];
        for (int32 TriVert = 0; TriVert < 3; ++TriVert)
        {
            CornerPositions[TriVert] = InPositions[i * 3 + TriVert];
        }

        if (PointsEqual(CornerPositions[0], CornerPositions[1]) ||
            PointsEqual(CornerPositions[0], CornerPositions[2]) ||
            PointsEqual(CornerPositions[1], CornerPositions[2]))
        {
            WedgeIndex += 3;
            continue;
        }

        FVector3f TriNormal;
        {
            const FVector3f Edge21 = CornerPositions[1] - CornerPositions[2];
            const FVector3f Edge20 = CornerPositions[0] - CornerPositions[1];
            TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
            TriNormal.Normalize();
        }

        int32 VertexIndices[3];
        for (int32 TriVert = 0; TriVert < 3; ++TriVert, ++WedgeIndex)
        {
            FVertSimp NewVert;
            NewVert.Position = CornerPositions[TriVert];
            NewVert.Normal = TriNormal;

            const TArray<int32>& DupVerts = OverlappingCorners.FindIfOverlapping(WedgeIndex);

            int32 Index = INDEX_NONE;
            for (int32 k = 0; k < DupVerts.Num(); k++)
            {
                if (DupVerts[k] >= WedgeIndex)
                {
                    break;
                }

                int32* Location = VertsMap.Find(DupVerts[k]);
                if (Location)
                {
                    FVertSimp& FoundVert = Verts[*Location];

                    if (NewVert.Equals(FoundVert))
                    {
                        Index = *Location;
                        break;
                    }
                }
            }
            if (Index == INDEX_NONE)
            {
                Index = Verts.Add(NewVert);
                VertsMap.Add(WedgeIndex, Index);
            }
            VertexIndices[TriVert] = Index;
        }
        if (VertexIndices[0] == VertexIndices[1] ||
            VertexIndices[1] == VertexIndices[2] ||
            VertexIndices[0] == VertexIndices[2])
        {
            continue;
        }

        {
            FVector3f Edge01 = CornerPositions[1] - CornerPositions[0];
            FVector3f Edge20 = CornerPositions[0] - CornerPositions[2];

            float TriArea = 0.5f * (Edge01 ^ Edge20).Size();
            SurfaceArea += TriArea;
        }

        Indexes.Add(VertexIndices[0]);
        Indexes.Add(VertexIndices[1]);
        Indexes.Add(VertexIndices[2]);
    }

    int32 NumVerts = Verts.Num();
    int32 NumIndexes = Indexes.Num();
    int32 NumTris = NumIndexes / 3;

    int32 TargetNumTris = FMath::CeilToInt(NumTris * PercentTriangles);
    int32 TargetNumVerts = FMath::CeilToInt(NumVerts * PercentVertices);

    TargetNumTris = FMath::Max(TargetNumTris, 2);
    TargetNumVerts = FMath::Max(TargetNumVerts, 4);

    {
        if (TargetNumVerts < NumVerts || TargetNumTris < NumTris)
        {
            const uint32 NumAttributes = (sizeof(FVertSimp) - sizeof(FVector3f)) / sizeof(float);
            float AttributeWeights[NumAttributes] =
            {
                16.0f, 16.0f, 16.0f// Normal
            };

            TArray<int32> MaterialIndexes;
            MaterialIndexes.AddZeroed(NumTris);

            FMeshSimplifier Simplifier((float*)Verts.GetData(), Verts.Num(), Indexes.GetData(), Indexes.Num(), MaterialIndexes.GetData(), NumAttributes);

            Simplifier.SetAttributeWeights(AttributeWeights);
            Simplifier.SetCorrectAttributes(CorrectAttributes);
            Simplifier.SetEdgeWeight(512.0f);
            Simplifier.SetLimitErrorToSurfaceArea(false);

            Simplifier.DegreePenalty = 100.0f;
            Simplifier.InversionPenalty = 1000000.0f;

            float MaxErrorSqr = Simplifier.Simplify(TargetNumVerts, TargetNumTris, 0.0f, 4, 2, MAX_flt);

            if (Simplifier.GetRemainingNumVerts() == 0 || Simplifier.GetRemainingNumTris() == 0)
            {
                return false;
            }

            Simplifier.Compact();

            Verts.SetNum(Simplifier.GetRemainingNumVerts());
            Indexes.SetNum(Simplifier.GetRemainingNumTris() * 3);

            NumVerts = Simplifier.GetRemainingNumVerts();
            NumTris = Simplifier.GetRemainingNumTris();
            NumIndexes = NumTris * 3;
        }
        else
        {
            return false;
        }
    }

    OutPositions.SetNumUninitialized(NumVerts);
    OutNormals.SetNumUninitialized(NumVerts);
    for (int32 i = 0; i < NumVerts; i++)
    {
        OutPositions[i] = Verts[i].Position;
        OutNormals[i] = Verts[i].Normal;
    }
    OutIndices = Indexes;

    return true;
}
