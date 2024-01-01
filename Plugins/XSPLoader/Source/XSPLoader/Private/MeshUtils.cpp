#include "MeshUtils.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Math/UnrealMathUtility.h"

bool bXSPEnableMeshClean = false;
FAutoConsoleVariableRef CVarXSPEnableMeshClean(
    TEXT("xsp.EnableMeshClean"), 
    bXSPEnableMeshClean,
    TEXT("是否剔除网格数据中的无效三角形，缺省为否")
);

void EncodeNormal(const FVector3f& InNormal, FXSPNormalVector& OutNormal)
{
    FLinearColor LinearColor(InNormal);
    FColor Color = LinearColor.ToRGBE();
    OutNormal.X = Color.R;
    OutNormal.Y = Color.G;
    OutNormal.Z = Color.B;
}

void DecodeNormal(const FXSPNormalVector& InNormal, FVector3f& OutNormal)
{
    FLinearColor LinearColor = FLinearColor::FromSRGBColor(FColor(InNormal.X, InNormal.Y, InNormal.Z));
    OutNormal.Set(LinearColor.R, LinearColor.G, LinearColor.B);
}

void ComputeNormal(const TArray<FVector3f>& PositionList, TArray<FXSPNormalVector>& NormalList, int32 Offset)
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
        FXSPNormalVector EncodedNormal;
        EncodeNormal(TriNormal, EncodedNormal);
        NormalList[Offset + TriIdx * 3 + 0] = NormalList[Offset + TriIdx * 3 + 1] = NormalList[Offset + TriIdx * 3 + 2] = EncodedNormal;
    }
}

//网格体
void AppendRawMesh(float* MeshVertexBuffer, float* MeshNormalBuffer, int32 BufferLength, TArray<FVector3f>& PositionList, TArray<FXSPNormalVector>& NormalList, FBox3f& InOutBoundingBox)
{
    if (nullptr == MeshVertexBuffer || BufferLength < 9 || BufferLength % 9 != 0)
    {
        checkNoEntry();
        return;
    }

    int32 Offset = PositionList.Num();

    if (bXSPEnableMeshClean)
    {
        int32 NumTriangles = BufferLength / 9;
        for (size_t j = 0; j < NumTriangles; j++)
        {
            FVector3f A(MeshVertexBuffer[j * 9 + 1] * 100, MeshVertexBuffer[j * 9 + 0] * 100, MeshVertexBuffer[j * 9 + 2] * 100);
            FVector3f B(MeshVertexBuffer[j * 9 + 4] * 100, MeshVertexBuffer[j * 9 + 3] * 100, MeshVertexBuffer[j * 9 + 5] * 100);
            FVector3f C(MeshVertexBuffer[j * 9 + 7] * 100, MeshVertexBuffer[j * 9 + 6] * 100, MeshVertexBuffer[j * 9 + 8] * 100);
            if (A == B || A == C || B == C)
                continue;

            PositionList.Add(A);
            PositionList.Add(B);
            PositionList.Add(C);
            if (nullptr != MeshNormalBuffer)
            {
                FXSPNormalVector EncodedNormal;
                EncodeNormal(FVector3f(MeshNormalBuffer[j * 9 + 1], MeshNormalBuffer[j * 9 + 0], MeshNormalBuffer[j * 9 + 2]), EncodedNormal);
                NormalList.Add(EncodedNormal);
                EncodeNormal(FVector3f(MeshNormalBuffer[j * 9 + 4], MeshNormalBuffer[j * 9 + 3], MeshNormalBuffer[j * 9 + 5]), EncodedNormal);
                NormalList.Add(EncodedNormal);
                EncodeNormal(FVector3f(MeshNormalBuffer[j * 9 + 7], MeshNormalBuffer[j * 9 + 6], MeshNormalBuffer[j * 9 + 8]), EncodedNormal);
                NormalList.Add(EncodedNormal);
            }
            else
            {
                const FVector3f Edge21 = B - C;
                const FVector3f Edge20 = A - C;
                FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();
                FXSPNormalVector EncodedNormal;
                EncodeNormal(TriNormal, EncodedNormal);
                NormalList.Add(EncodedNormal);
                NormalList.Add(EncodedNormal);
                NormalList.Add(EncodedNormal);
            }
        }
    }
    else
    {
        int32 NumMeshVertices = BufferLength / 3;
        PositionList.AddUninitialized(NumMeshVertices);
        for (size_t j = 0; j < NumMeshVertices; j++)
        {
            PositionList[Offset + j].Set(MeshVertexBuffer[j * 3 + 1] * 100, MeshVertexBuffer[j * 3 + 0] * 100, MeshVertexBuffer[j * 3 + 2] * 100);
            InOutBoundingBox += PositionList[Offset + j];
        }

        NormalList.AddUninitialized(NumMeshVertices);
        if (nullptr != MeshNormalBuffer)
        {
            for (size_t j = 0; j < NumMeshVertices; j++)
            {
                EncodeNormal(FVector3f(MeshNormalBuffer[j * 3 + 1], MeshNormalBuffer[j * 3 + 0], MeshNormalBuffer[j * 3 + 2]), NormalList[Offset + j]);
            }
        }
        else
        {
            ComputeNormal(PositionList, NormalList, Offset);
        }
    }
}

//椭圆形
void AppendEllipticalMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& PositionList, TArray<FXSPNormalVector>& NormalList, FBox3f& InOutBoundingBox)
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
    TArray<FVector3f> EllipticalMeshVertices;
    TArray<FXSPNormalVector> EllipticalMeshNormals;
    EllipticalMeshVertices.SetNumUninitialized(NumSegments * 3);
    EllipticalMeshNormals.SetNumUninitialized(NumSegments * 3);
    FXSPNormalVector EncodedNormal;
    EncodeNormal(Normal, EncodedNormal);
    int32 Index = 0;
    for (int32 i = 0; i < NumSegments; i++)
    {
        EllipticalMeshNormals[Index] = EncodedNormal;
        EllipticalMeshVertices[Index] = Origin;
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;

        EllipticalMeshNormals[Index] = EncodedNormal;
        EllipticalMeshVertices[Index] = Origin + RadialVectors[i + 1];
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;
        
        EllipticalMeshNormals[Index] = EncodedNormal;
        EllipticalMeshVertices[Index] = Origin + RadialVectors[i];
        InOutBoundingBox += EllipticalMeshVertices[Index];
        Index++;
    }

    PositionList.Append(MoveTemp(EllipticalMeshVertices));
    NormalList.Append(MoveTemp(EllipticalMeshNormals));
}

//圆柱体
void AppendCylinderMesh(float* PrimitiveParamsBuffer, uint8 BufferLength, TArray<FVector3f>& PositionList, TArray<FXSPNormalVector>& NormalList, FBox3f& InOutBoundingBox)
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

    TArray<FVector3f> CylinderMeshVertices;
    TArray<FXSPNormalVector> CylinderMeshNormals;
    CylinderMeshVertices.SetNumUninitialized(NumSegments * 6);
    CylinderMeshNormals.SetNumUninitialized(NumSegments * 6);
    FXSPNormalVector EncodedNormal, EncodedNormal1;
    int32 Index = 0;
    //侧面
    for (int32 i = 0; i < NumSegments; i++)
    {
        EncodeNormal(RadialVectors[i].GetSafeNormal(), EncodedNormal);
        EncodeNormal(RadialVectors[i+1].GetSafeNormal(), EncodedNormal1);

        CylinderMeshNormals[Index] = EncodedNormal;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = EncodedNormal;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = EncodedNormal1;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = EncodedNormal1;
        CylinderMeshVertices[Index] = BottomCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = EncodedNormal;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;

        CylinderMeshNormals[Index] = EncodedNormal1;
        CylinderMeshVertices[Index] = TopCenter + RadialVectors[i + 1];
        InOutBoundingBox += CylinderMeshVertices[Index];
        Index++;
    }

    PositionList.Append(MoveTemp(CylinderMeshVertices));
    NormalList.Append(MoveTemp(CylinderMeshNormals));
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

void InheritMaterial(FXSPNodeData& Node, FXSPNodeData& ParentNode)
{
    if (IsValidMaterial(ParentNode.Material))
    {
        Node.MeshMaterial = FLinearColor(ParentNode.Material[0], ParentNode.Material[1], ParentNode.Material[2], ParentNode.Material[3]);
    }
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