#include "CombinedMeshBuilder.h"
#include "XSPFileUtils.h"
#include "MeshUtils.h"


FCombinedMeshBuilder::FCombinedMeshBuilder()
{
}

FCombinedMeshBuilder::~FCombinedMeshBuilder()
{
}

void FCombinedMeshBuilder::SetSourceMaterial(UMaterialInterface* InSourceMaterial)
{
    SourceMaterial = InSourceMaterial;
}

void FCombinedMeshBuilder::AddBodyData(FLinearColor Material, TSharedPtr<Body_info> BodyData)
{
    //FLinearColor Material(1,1,1,1);
    //GetMaterial(*BodyData.Get(), Material, Material.A);

    if (!DataMap.Contains(Material))
        DataMap.Add(Material);

    DataMap[Material].Add(BodyData);
}

void FCombinedMeshBuilder::BuildMeshes(UObject* Outer, TArray<UStaticMesh*>& StaticMeshArray, TArray<UMaterialInterface*>& MaterialArray, bool bBuildNormal)
{
    for (auto Itr : DataMap)
    {
        TArray<FVector3f> VertexList, NormalList;
        FLinearColor Material = Itr.Key;
        UMaterialInstanceDynamic* MaterialInstance = CreateMaterialInstanceDynamic(SourceMaterial, Material, Material.A);

        TArray<TSharedPtr<Body_info>>& BodyDataArray = Itr.Value;
        for (auto DataPtr : BodyDataArray)
        {
            AppendNodeMesh(*DataPtr.Get(), VertexList, bBuildNormal ? &NormalList : nullptr);
            if (VertexList.Num() >= 1024 * 1024)
            {
                UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Outer);
                BuildStaticMesh(StaticMesh, VertexList, bBuildNormal ? &NormalList : nullptr);
                StaticMeshArray.Add(StaticMesh);
                MaterialArray.Add(MaterialInstance);

                VertexList.Reset();
                if (bBuildNormal)
                    NormalList.Reset();
            }
        }
        if (VertexList.Num() > 0)
        {
            UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Outer);
            BuildStaticMesh(StaticMesh, VertexList, bBuildNormal ? &NormalList : nullptr);
            StaticMeshArray.Add(StaticMesh);
            MaterialArray.Add(MaterialInstance);
        }
    }
}
