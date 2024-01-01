#include "XSPFileUtils.h"

void ReadHeaderInfo(std::fstream& file, Header_info& info)
{
    file.read((char*)&info.empty_fragment, sizeof(info.empty_fragment));
    file.read((char*)&info.parentdbid, sizeof(info.parentdbid));
    file.read((char*)&info.level, sizeof(info.level));
    file.read((char*)&info.startname, sizeof(info.startname));
    file.read((char*)&info.namelength, sizeof(info.namelength));
    file.read((char*)&info.startproperty, sizeof(info.startproperty));
    file.read((char*)&info.propertylength, sizeof(info.propertylength));
    file.read((char*)&info.startmaterial, sizeof(info.startmaterial));
    file.read((char*)&info.startbox, sizeof(info.startbox));
    file.read((char*)&info.startvertices, sizeof(info.startvertices));
    file.read((char*)&info.verticeslength, sizeof(info.verticeslength));
    file.read((char*)&info.offset, sizeof(info.offset));
    file.seekg(16, std::ios::cur);
}

void ReadHeaderInfo(std::fstream& file, int nsize, TArray<Header_info>& header_list) {
    header_list.SetNum(nsize);
    for (int i = 0; i < nsize; i++) {
        ReadHeaderInfo(file, header_list[i]);
    }
}

void ReadNodeData(std::fstream& file, const Header_info& header, FXSPNodeData& NodeData)
{
    NodeData.Level = header.level;
    NodeData.ParentDbid = header.parentdbid;
    NodeData.NumChildren = header.offset - NodeData.Dbid + 1;
    
    //读材质
    file.seekg(header.startmaterial, std::ios::beg);
    file.read((char*)&NodeData.Material, sizeof(NodeData.Material));

    //读fragments
    if (header.verticeslength > 0) 
    {
        file.seekg(header.startvertices, std::ios::beg);
     
        TArray<Header_info> fragment_headerList;
        ReadHeaderInfo(file, header.verticeslength / 60, fragment_headerList);
        int num_fragments = fragment_headerList.Num();
        NodeData.PrimitiveArray.SetNum(num_fragments);
        for (int k = 0; k < num_fragments; k++) 
        {
            if (fragment_headerList[k].verticeslength > 0)
            {
                ReadPrimitiveData(file, fragment_headerList[k], NodeData.PrimitiveArray[k]);
            }
        }
    }
}

static const std::string GXSPPrimitiveTypeStringMesh = "Mesh";
static const std::string GXSPPrimitiveTypeStringElliptical = "Elliptical";
static const std::string GXSPPrimitiveTypeStringCylinder = "Cylinder";
static char NameBuffer[128];
void ReadPrimitiveData(std::fstream& file, const Header_info& header, FXSPPrimitiveData& PrimitiveData)
{
    file.seekg(header.startname, std::ios::beg);
    file.read(NameBuffer, header.namelength);
    NameBuffer[header.namelength] = '\0';
    if (GXSPPrimitiveTypeStringMesh == NameBuffer)
    {
        PrimitiveData.Type = EXSPPrimitiveType::Mesh;
    }
    else if (GXSPPrimitiveTypeStringElliptical == NameBuffer)
    {
        PrimitiveData.Type = EXSPPrimitiveType::Elliptical;
    }
    else if (GXSPPrimitiveTypeStringCylinder == NameBuffer)
    {
        PrimitiveData.Type = EXSPPrimitiveType::Cylinder;
    }

    file.seekg(header.startmaterial, std::ios::beg);
    file.read((char*)&PrimitiveData.Material, sizeof(PrimitiveData.Material));

    //if (header.verticeslength > 0)
    {
        file.seekg(header.startvertices, std::ios::beg);
        if (PrimitiveData.Type == EXSPPrimitiveType::Elliptical || PrimitiveData.Type == EXSPPrimitiveType::Cylinder)
        {
            PrimitiveData.PrimitiveParamsBufferLength = header.verticeslength / 4;
            PrimitiveData.PrimitiveParamsBuffer = new float[PrimitiveData.PrimitiveParamsBufferLength];
            file.read((char*)PrimitiveData.PrimitiveParamsBuffer, sizeof(float) * PrimitiveData.PrimitiveParamsBufferLength);
        }
        else if (PrimitiveData.Type == EXSPPrimitiveType::Mesh)
        {
            PrimitiveData.MeshVertexBufferLength = header.verticeslength / 4;
            PrimitiveData.MeshVertexBuffer = new float[PrimitiveData.MeshVertexBufferLength];
            file.read((char*)PrimitiveData.MeshVertexBuffer, sizeof(float) * PrimitiveData.MeshVertexBufferLength);
            //TODO:MeshNormalBuffer
        }
    }
}