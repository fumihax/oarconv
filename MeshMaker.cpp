﻿
#include "MeshMaker.h"



using namespace jbxl;




///////////////////////////////////////////////////////////////////////////////////////////////////
//

/**
PrimBaseShapeデータから Colladaのデータを生成する．@n
PrimBaseShapeデータは jbxl::CreatePrimBaseShapesFromXML() または PrimBaseShape::GenerateParam() で生成する．

@param shape         PrimBaseShape データ
@param resourceList  key部にリソース名，val部に assetリソースのパスを格納したリスト．Sculpted Image, llmeshデータの検索用．
@param useBrep       BREPを使用して頂点を配置する．速度は若干遅くなるが，頂点数（データ量）は減る．
@param addParam      マテリアルのテクスチャ名にパラメータ用文字列を付与するか？（Unity3D用）

@return  MeshObjectData  Colladaデータ

@sa OpenSim/Region/Physics/Meshing/Meshmerizer.cs
*/
MeshObjectData*  jbxl::MeshObjectDataFromPrimShape(PrimBaseShape baseShape, tList* resourceList, bool useBrep, bool addParam)
{
    PrimMeshParam param;
    param.GetParamFromBaseShape(baseShape);

    TriPolyData*    tridata   = NULL;
    FacetBaseData*  facetdata = NULL;
    int tri_num   = 0;
    int facet_num = 0;

    // Mesh
    if ((param.sculptType&0x07)==SCULPT_TYPE_MESH) {
        //DEBUG_MODE PRINT_MESG("JBXL::MeshObjectDataFromPrimShape: Generate LLM Mesh\n");
        char* path = get_resource_path((char*)param.sculptTexture.buf, resourceList);
        tridata = TriPolyDataFromLLMeshFile(path, &facet_num, &tri_num);
    }

    // Sculpted Prim
    else if (param.sculptEntry) {
        //DEBUG_MODE PRINT_MESG("JBXL::MeshObjectDataFromPrimShape: Generate Sculpt Mesh\n");
        char* path = get_resource_path((char*)param.sculptTexture.buf, resourceList);
        //facetdata = FacetBaseDataFromSculptJP2K(path, param.sculptType);
        tridata = TriPolyDataFromSculptJP2K(path, param.sculptType, &tri_num);
        facet_num = 1;
    }

    // Normal Prim
    else {
        //DEBUG_MODE PRINT_MESG("JBXL::MeshObjectDataFromPrimShape: Generate Prim Mesh\n");
        PrimMesh primMesh = GeneratePrimMesh(param);
        tridata = TriPolyDataFromPrimMesh(primMesh, &facet_num, &tri_num);
        primMesh.free();
    }

    if (tridata==NULL && facetdata==NULL) {
        PRINT_MESG("JBXL::MeshObjectDataFromPrimShape: ERROR: get no data! return.\n");
        param.free();
        return NULL;
    }

    MeshObjectData* data = new MeshObjectData((char*)param.objectName.buf);
    data->setAffineTrans(param.affineTrans);

    // ポリゴンデータの追加とTextureデータの設定
    if (tridata!=NULL) {            // for TriPolyData
        for (int i=0; i<facet_num; i++) {
            int facet = i;
            if (facet>=PRIM_MATERIAL_NUM) facet = PRIM_MATERIAL_NUM - 1;
            if (param.materialParam[facet].enable) {
                MaterialParam mparam;
                mparam.dup(param.materialParam[facet]);
                //
                mparam.setAlphaChannel(CheckAlphaChannel(mparam.getTextureName(), resourceList));
                if (mparam.isSetAlpha()) mparam.setTransparent(MTRL_DEFAULT_ALPHA);
                char* paramsname = mparam.getBase64Params('O');
                if (addParam) {
                    if (paramsname!=NULL) {
                        mparam.setAdditionalName(paramsname);
                        ::free(paramsname);
                    }
                }
                //
                data->addData(tridata, tri_num, i, &mparam, useBrep);
                mparam.free();
            }
            else {
                data->addData(tridata, tri_num, i, NULL, useBrep);
            }
        }
    }
    //
    else if (facetdata!=NULL) {     // for FacetBaseData
        if (param.materialParam->enable) {
            MaterialParam mparam;
            mparam.dup(param.materialParam[0]);
            //
            mparam.setAlphaChannel(CheckAlphaChannel(mparam.getTextureName(), resourceList));
            if (mparam.isSetAlpha()) mparam.setTransparent(MTRL_DEFAULT_ALPHA);
            if (addParam) {
                char* paramsname = mparam.getBase64Params('O');
                if (paramsname!=NULL) {
                    mparam.setAdditionalName(paramsname);
                    ::free(paramsname);
                }
            }
            //
            data->addData(facetdata, &mparam);
            mparam.free();
        }
        else {
            data->addData(facetdata, NULL);
        }
    }

    freeTriPolyData(tridata, tri_num);
    freeFacetBaseData(facetdata);
    param.free();

    return data;
}



/**
PrimMeshParamパラメータから PrimMeshデータを生成する．@n

@param param  PrimMeshParamパラメータ
@return  PrimMeshデータ
@sa GenerateCoordsAndFacesFromPrimShapeData() in OpenSim/Region/Physics/Meshing/Meshmerizer.cs
*/
PrimMesh  jbxl::GeneratePrimMesh(PrimMeshParam param)
{
    int   type  = 0;
    int   sides = 4;
    int   hollowSides = 4;

    if (param.profCurve==PRIM_PROF_CIRCLE) {
        sides = 24;
        if (param.pathCurve==PRIM_PATH_CIRCLE) { 
            if (param.sculptEntry) type = PRIM_TYPE_SCULPT;
            else                   type = PRIM_TYPE_TORUS;
        }
        else if (param.pathCurve==PRIM_PATH_LINE) {
            type = PRIM_TYPE_CYLINDER;
        }
    }
    //
    else if (param.profCurve==PRIM_PROF_SQUARE) {
        sides = 4;
        if (param.pathCurve==PRIM_PATH_CIRCLE) { 
            type = PRIM_TYPE_TUBE;
        }
        else if (param.pathCurve==PRIM_PATH_LINE) {
            type = PRIM_TYPE_BOX;
        }
    }
    //
    else if (param.profCurve==PRIM_PROF_EQUALTRIANGLE) {
        sides = 3;
        type  = PRIM_TYPE_RING;
    }
    //
    else if (param.profCurve==PRIM_PROF_HALFCIRCLE) {
        sides = 24;
        type  = PRIM_TYPE_SPHERE;
        param.profBegin = 0.5f*param.profBegin + 0.4999f;
        param.profEnd   = 0.5f*param.profEnd   + 0.4999f;
        if (param.profBegin<0.0f) param.profBegin = 0.0f;
        if (param.profEnd  >1.0f) param.profEnd   = 1.0f;
    }

    // Hollow
    hollowSides = sides;
    if      (param.hollowType==PRIM_HOLE_CIRCLE)   hollowSides = 24;
    else if (param.hollowType==PRIM_HOLE_SQUARE)   hollowSides = 4;
    else if (param.hollowType==PRIM_HOLE_TRIANGLE) hollowSides = 3;

    PrimMesh primMesh(sides, hollowSides, param);
    primMesh.shapeType = type;

    if (param.pathCurve==PRIM_PATH_LINE || param.pathCurve==PRIM_PATH_FLEXIBLE) {
        primMesh.meshParam.pathTwistBegin = primMesh.meshParam.pathTwistBegin/2.0f;
        primMesh.meshParam.pathTwistEnd   = primMesh.meshParam.pathTwistEnd/2.0f;
        primMesh.Extrude(PRIM_PATH_LINE);
    }
    else {
        primMesh.Extrude(PRIM_PATH_CIRCLE);
    }

    return primMesh;
}



/**
PrimMeshデータから三角ポリゴンデータの TriPolyDataを生成する．@n

@param primMesh   PrimMeshデータ
@param[out] fnum  生成したFACETの数
@param[out] pnum  生成した三角ポリゴンデータの数

@return  三角ポリゴンデータの配列へのポインタ
*/
TriPolyData*  jbxl::TriPolyDataFromPrimMesh(PrimMesh primMesh, int* fnum, int* pnum)
{
    if (fnum!=NULL) *fnum = 0;
    if (pnum!=NULL) *pnum = 0;

    int len = (int)primMesh.primTriArray.size();
    TriPolyData* tridata = (TriPolyData*)malloc(len*sizeof(TriPolyData));
    if (tridata==NULL) return NULL;

    for (int i=0; i<len; i++) {
        tridata[i].has_normal = true;
        tridata[i].has_texcrd = true;
        tridata[i].facetNum   = primMesh.primTriArray[i].facetNum;
        tridata[i].vertex[0]  = primMesh.primTriArray[i].v1;
        tridata[i].vertex[1]  = primMesh.primTriArray[i].v2;
        tridata[i].vertex[2]  = primMesh.primTriArray[i].v3;
        tridata[i].normal[0]  = primMesh.primTriArray[i].n1;
        tridata[i].normal[1]  = primMesh.primTriArray[i].n2;
        tridata[i].normal[2]  = primMesh.primTriArray[i].n3;
        tridata[i].texcrd[0]  = primMesh.primTriArray[i].uv1;
        tridata[i].texcrd[1]  = primMesh.primTriArray[i].uv2;
        tridata[i].texcrd[2]  = primMesh.primTriArray[i].uv3;
    }

    if (fnum!=NULL) *fnum = primMesh.numPrimFacets;
    if (pnum!=NULL) *pnum = len;

    return tridata;
}




////////////////////////////////////////////////////////////////////////////////////////////////
// @sa http://wiki.secondlife.com/wiki/Sculpted_Prims:_Technical_Explanation
// 


/**
Sculpted Primのファイル(JPEG 2000) からデータを読み込み，インデックス化された三角ポリゴンデータ FacetBaseDataを生成する．@n

@param jpegfile  Sculpted Primのデータの入った JPEG 2000ファイル名．
@param type      Sculpted Primのタイプ

@return  生成したポリゴンデータへのポインタ．
*/
FacetBaseData*  jbxl::FacetBaseDataFromSculptJP2K(const char* jpegfile, int type)
{
    if (jpegfile==NULL) return NULL;

    DEBUG_MODE PRINT_MESG("JBXL::FacetBaseDataFromSculptJP2K: reading sculpt image file %s\n", jpegfile);

    JPEG2KImage jpg = readJPEG2KFile(jpegfile);
    DEBUG_MODE PRINT_MESG("JBXL::FacetBaseDataFromSculptJP2K: size = (%d, %d, %d), mode = %d\n", jpg.xs, jpg.ys, jpg.col, jpg.cmode);
    if (jpg.isNull() || jpg.col<1) {
        PRINT_MESG("JBXL::FacetBaseDataFromSculptJP2K: WARNING: invalid jpeg 2000 image file! [%s]\n", jpegfile);
        jpg.free();
        return NULL;
    }

    MSGraph<uByte> grd = JPEG2KImage2MSGraph<uByte>(jpg);
    FacetBaseData* facetdata = NULL;

    /*
    if (grd.zs>1) {
    }
    else {
    }
    */
    facetdata = FacetBaseDataFromSculptImage(grd, type);

    grd.free();
    jpg.free();

    return facetdata;
}



/**
Sculpted Primの画像データからインデックス化された三角ポリゴンデータ FacetBaseDataを生成する．@n

@param grd   Sculpted Primのデータの入った画像データ．
@param type  Sculpted Primのタイプ
@return  生成したポリゴンデータへのポインタ．
*/
FacetBaseData*  jbxl::FacetBaseDataFromSculptImage(MSGraph<uByte> grd, int type)
{
    if (grd.isNull()) return NULL;

    // Gray Scale
    if (grd.zs==1){
        if (grd.color==GRAPH_COLOR_MONO) {
            grd.xs = grd.xs/2;
            grd.ys = grd.ys/2;
            grd.zs = 4;
            grd.color = GRAPH_COLOR_RGBA;
        }
        else return NULL;
    }
    //
    SculptMesh sculptMesh(type);

    FacetBaseData* facetdata = NULL;

    try {
        bool ret = sculptMesh.Image2Coords(grd);
        if (!ret) return NULL;
        sculptMesh.GenerateMeshData();

        //
        int inum = (int)sculptMesh.sculptTriIndex.size()*3;
        int cnum = (int)sculptMesh.coords.size();

        facetdata = new FacetBaseData(inum, cnum);
        if (!facetdata->getm()) {
            sculptMesh.free();
            return NULL;
        }

        for (int i=0; i<inum/3; i++) {
            facetdata->index[i*3+0] = sculptMesh.sculptTriIndex[i].v1;
            facetdata->index[i*3+1] = sculptMesh.sculptTriIndex[i].v2;
            facetdata->index[i*3+2] = sculptMesh.sculptTriIndex[i].v3;
        }
        for (int i=0; i<cnum; i++) {
            facetdata->vertex[i] = sculptMesh.coords[i];
            facetdata->normal[i] = sculptMesh.normals[i];
            facetdata->texcrd[i] = sculptMesh.uvs[i];
        }
    }
    catch (const std::bad_alloc&) {
        PRINT_MESG("JBXL::FacetBaseDataFromSculptImage: ERROR: out of memory!!\n");
        if (facetdata!=NULL) {
            freeFacetBaseData(facetdata);
            facetdata = NULL;
        }
    }

    sculptMesh.free();
    return facetdata;
}



/**
Sculpted Primのファイル(JPEG 2000) からデータを読み込み，三角ポリゴンデータ TriPloyDataを生成する．@n

@param jpegfile   Sculpted Primのデータの入った JPEG 2000ファイル名．
@param type       Sculpted Primのタイプ
@param[out] pnum  生成したポリゴンデータ（配列）の数．

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromSculptJP2K(const char* jpegfile, int type, int* pnum)
{
    if (pnum!=NULL) *pnum = 0;
    if (jpegfile==NULL) return NULL;

    DEBUG_MODE PRINT_MESG("JBXL::TriPolyDataFromSculptJP2K: reading sculpt image file %s\n", jpegfile);

    JPEG2KImage jpg = readJPEG2KFile(jpegfile);
    DEBUG_MODE PRINT_MESG("JBXL::TriPolyDataFromSculptJP2K: size = (%d, %d, %d), mode = %d\n", jpg.xs, jpg.ys, jpg.col, jpg.cmode);
    if (jpg.isNull() || jpg.col<1) {
        PRINT_MESG("JBXL::TriPolyDataFromSculptJP2K: ERROR: invalid jpeg 2000 image file! [%s]\n", jpegfile);
        jpg.free();
        return NULL;
    }

    MSGraph<uByte> grd = JPEG2KImage2MSGraph<uByte>(jpg);
    TriPolyData* tridata = NULL;

    if (grd.zs>1) {
        tridata = TriPolyDataFromSculptImage(grd, type, pnum);
    }
    else {
        tridata = GenerateGrayScaleSculpt(pnum);
    }

    grd.free();
    jpg.free();

    return tridata;
}



/**
Sculpted Primの画像データから三角ポリゴンデータ TriPloyDataを生成する．@n

@param grd        Sculpted Primのデータの入った画像データ．
@param type       Sculpted Primのタイプ
@param[out] pnum  生成したポリゴンデータ（配列）の数．

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromSculptImage(MSGraph<uByte> grd, int type, int* pnum)
{
    if (pnum!=NULL) *pnum = 0;
    if (grd.isNull()) return NULL;

    // Gray Scale
    if (grd.zs==1){
        if (grd.color==GRAPH_COLOR_MONO) {
            grd.xs = grd.xs/2;
            grd.ys = grd.ys/2;
            grd.zs = 4;
            grd.color = GRAPH_COLOR_RGBA;
        }
        else return NULL;
    }

    int tnum = 0;
    //
    SculptMesh sculptMesh(type);

    TriPolyData* tridata = NULL;

    try {
        bool ret = sculptMesh.Image2Coords(grd);
        if (!ret) return NULL;
        sculptMesh.GenerateMeshData();

        tnum = (int)sculptMesh.sculptTriArray.size();
        tridata = (TriPolyData*)malloc(tnum*sizeof(TriPolyData));
        if (tridata==NULL) {
            sculptMesh.free();
            return NULL;
        }

        for (int i=0; i<tnum; i++) {
            tridata[i].has_normal = true;
            tridata[i].has_texcrd = true;
            tridata[i].facetNum   = sculptMesh.sculptTriArray[i].facetNum;
            tridata[i].vertex[0]  = sculptMesh.sculptTriArray[i].v1;
            tridata[i].vertex[1]  = sculptMesh.sculptTriArray[i].v2;
            tridata[i].vertex[2]  = sculptMesh.sculptTriArray[i].v3;
            tridata[i].normal[0]  = sculptMesh.sculptTriArray[i].n1;
            tridata[i].normal[1]  = sculptMesh.sculptTriArray[i].n2;
            tridata[i].normal[2]  = sculptMesh.sculptTriArray[i].n3;
            tridata[i].texcrd[0]  = sculptMesh.sculptTriArray[i].uv1;
            tridata[i].texcrd[1]  = sculptMesh.sculptTriArray[i].uv2;
            tridata[i].texcrd[2]  = sculptMesh.sculptTriArray[i].uv3;
        }
    }
    catch (const std::bad_alloc&) {
        PRINT_MESG("JBXL::TriPolyDataFromSculptImage: ERROR: out of memory!!\n");
        if (tridata!=NULL) {
            freeTriPolyData(tridata, tnum);
            tridata = NULL;
        }
        tnum = 0;
    }
    if (pnum!=NULL) *pnum = tnum;

    sculptMesh.free();
    return tridata;
}




////////////////////////////////////////////////////////////////////////////////////////////////
// for LLMesh
//
// @sa http://wiki.secondlife.com/wiki/Mesh/Mesh_Asset_Format
// 

/**
llmeshデータから三角ポリゴンデータ TriPloyDataを生成する．@n

@param mesh       llmeshのデータ．ファイルイメージ．
@param sz         meshのサイズ
@param[out] fnum  生成したFACET（面）の数．
@param[out] pnum  生成したポリゴンデータ（配列）の数．

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromLLMesh(uByte* mesh, int sz, int* fnum, int* pnum)
{
    if (mesh==NULL || fnum==NULL || pnum==NULL) return NULL;

    tXML* xml = GetLLsdXMLFromLLMesh(mesh, sz, "high_lod");
    if (xml==NULL) xml = GetLLsdXMLFromLLMesh(mesh, sz, "medium_lod");
    if (xml==NULL) return NULL;

    /////////////////////////////////////////////////////////
    //DEBUG_MODE print_xml(stderr, xml, XML_INDENT_FORMAT); // llmeshデータ

    // インデックス，座標データ
    tList* lpindex = get_xml_content_list_bystr(xml, "<map><key>TriangleList</key><binary>");
    tList* lppostn = get_xml_content_list_bystr(xml, "<map><key>Position</key><binary>");
    if (lpindex==NULL || lppostn==NULL) {
        if (lpindex!=NULL) del_tList(&lpindex);
        if (lppostn!=NULL) del_tList(&lppostn);
        del_xml(&xml);
        *fnum = 0;
        *pnum = 0;
        return NULL;
    }

    //
    int vertex_num = 0;     // 頂点数
    int facet_idx  = 0;     // インデックスデータ中のFACET数 
    int facet_pos  = 0;     // 座標データ中のFACET数

    // TriangleList (Vertex Indices)
    tList* lpidx = lpindex;
    while (lpidx!=NULL) {
        if (lpidx->altp!=NULL) {
            facet_idx++;
            Buffer dec = decode_base64_Buffer(lpidx->altp->ldat.key);
            vertex_num += dec.vldsz/2;
            free_Buffer(&dec);
        }
        lpidx = lpidx->next;
    }
    // Position
    tList* lppos = lppostn;
    while (lppos!=NULL) {
        if (lppos->altp!=NULL) {
            facet_pos++;
        }
        lppos = lppos->next;
    }

    if (facet_idx!=facet_pos) PRINT_MESG("WARNING: TriPolyDataFromLLMesh: missmatch facet number!\n");
    int facet_num = Min(facet_idx, facet_pos);  // FACET数．通常は一致する．
    int plygn_num = vertex_num/3;               // ポリゴン数

    //
    size_t len = sizeof(TriPolyData)*plygn_num;
    TriPolyData* tridata = (TriPolyData*)malloc(len);
    memset(tridata, 0, len);

    // Option: 法線ベクトル，UVマップ
    tList* lpnorml = get_xml_content_list_bystr(xml, "<map><key>Normal</key><binary>");
    tList* lptxtur = get_xml_content_list_bystr(xml, "<map><key>TexCoord0</key><binary>");

    // Max, Min
    Vector<float>* pos_max = GetLLMeshPositionDomainMax(xml, facet_num);
    Vector<float>* pos_min = GetLLMeshPositionDomainMin(xml, facet_num);
    UVMap<float>*  tex_max = GetLLMeshTexCoordDomainMax(xml, facet_num);
    UVMap<float>*  tex_min = GetLLMeshTexCoordDomainMin(xml, facet_num);

    //
    lpidx = lpindex;
    lppos = lppostn;
    tList*  lpnrm = lpnorml;
    tList*  lptxt = lptxtur;

    Buffer  idx = init_Buffer();
    Buffer  pos = init_Buffer();
    Buffer  nrm = init_Buffer();
    Buffer  tex = init_Buffer();

    int tri_num = 0;
    unsigned short index[3];
    //
    for (int facet=0; facet<facet_num; facet++) {
        idx = decode_base64_Buffer(lpidx->altp->ldat.key);
        pos = decode_base64_Buffer(lppos->altp->ldat.key);
        if (lpnrm!=NULL) nrm = decode_base64_Buffer(lpnrm->altp->ldat.key);
        if (lptxt!=NULL) tex = decode_base64_Buffer(lptxt->altp->ldat.key);

        for (int tri=0; tri<idx.vldsz/6; tri++) {   // ポリゴンループ
            for (int vtx=0; vtx<3; vtx++) index[vtx] = ushort_from_little_endian(idx.buf+6*tri+2*vtx);  // 頂点インデックス

            tridata[tri_num].facetNum   = facet;
            tridata[tri_num].has_normal = true;
            tridata[tri_num].has_texcrd = true;
            for (int vtx=0; vtx<3; vtx++) {
                tridata[tri_num].vertex[vtx].x = LLMeshUint16toFloat(pos.buf+6*index[vtx]+0, pos_max[facet].x, pos_min[facet].x);
                tridata[tri_num].vertex[vtx].y = LLMeshUint16toFloat(pos.buf+6*index[vtx]+2, pos_max[facet].y, pos_min[facet].y);
                tridata[tri_num].vertex[vtx].z = LLMeshUint16toFloat(pos.buf+6*index[vtx]+4, pos_max[facet].z, pos_min[facet].z);
            }
            if (lpnrm!=NULL) {
                for (int vtx=0; vtx<3; vtx++) {
                    tridata[tri_num].normal[vtx].x = (float)ushort_from_little_endian(nrm.buf+6*index[vtx]+0)/SWORDMAX - 1.0f;
                    tridata[tri_num].normal[vtx].y = (float)ushort_from_little_endian(nrm.buf+6*index[vtx]+2)/SWORDMAX - 1.0f;
                    tridata[tri_num].normal[vtx].z = (float)ushort_from_little_endian(nrm.buf+6*index[vtx]+4)/SWORDMAX - 1.0f;
                }
            }
            if (lptxt!=NULL) {
                for (int vtx=0; vtx<3; vtx++) {
                    tridata[tri_num].texcrd[vtx].u = LLMeshUint16toFloat(tex.buf+4*index[vtx]+0, tex_max[facet].u, tex_min[facet].u);
                    tridata[tri_num].texcrd[vtx].v = LLMeshUint16toFloat(tex.buf+4*index[vtx]+2, tex_max[facet].v, tex_min[facet].v);
                }
            }
            tri_num++;
        }

        free_Buffer(&idx);
        free_Buffer(&pos);
        lpidx = lpidx->next;
        lppos = lppos->next;

        if (lpnrm!=NULL) {
            free_Buffer(&nrm);
            lpnrm = lpnrm->next;
        }
        if (lptxt!=NULL) {
            free_Buffer(&tex);
            lptxt = lptxt->next;
        }
    }

    freeNull(pos_max);
    freeNull(pos_min);
    freeNull(tex_max);
    freeNull(tex_min);

    del_tList(&lpindex);
    del_tList(&lppostn);
    if (lpnorml!=NULL) del_tList(&lpnorml);
    if (lptxtur!=NULL) del_tList(&lptxtur);

    del_xml(&xml);

    *fnum = facet_num;
    *pnum = tri_num;
    return tridata;
}



/**
llmeshのファイルからデータを読み込み，三角ポリゴンデータ TriPloyDataを生成する．@n

@param filename   llmeshファイル名．
@param[out] fnum  生成したFACET（面）の数．
@param[out] pnum  生成したポリゴンデータ（配列）の数．

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromLLMeshFile(const char* filename, int* fnum, int* pnum)
{
    if (filename==NULL || fnum==NULL || pnum==NULL) return NULL;
    *fnum = *pnum = 0;

    size_t sz = file_size(filename);
    if (sz<=0) return NULL;

    uByte* buf = (uByte*)malloc(sz);
    if (buf==NULL) return NULL;

    DEBUG_MODE PRINT_MESG("JBXL::TriPolyDataFromLLMeshFile: reading mesh file %s\n", filename);
    FILE* fp = fopen(filename, "rb");
    if (fp==NULL) {
        freeNull(buf);
        return NULL;
    }
    fread(buf, sz, 1, fp);
    fclose(fp);

    int facet_num, tri_num;
    TriPolyData* tridata = TriPolyDataFromLLMesh(buf, (int)sz, &facet_num, &tri_num);

    freeNull(buf);

    *fnum = facet_num;
    *pnum = tri_num;
    return tridata;
}



/**
llmeshファイルのヘッダ部分の keyを参照し，圧縮されたボディデータから該当データを取り出してXML形式に変換する．

@param buf  llmeshファイルのヘッダ部分のバイナリデータ
@param sz   buf のサイズ
@param key  取り出すデータのキー．Ex.) "hight_lod", "medium_lod", "low_lod", "lowest_lod", etc...

@return  指定された keyのデータの XML形式．
*/
tXML*  jbxl::GetLLsdXMLFromLLMesh(uByte* buf, int sz, const char* key)
{
    int hdsz  = llsd_bin_get_length(buf, sz);
    tXML* xml = llsd_bin_parse(buf, hdsz);

    int ofst = -1, size = -1;
    if (llsd_xml_contain_key(xml, key)){
        ofst = llsd_xml_get_content_int(xml, key, "offset");
        size = llsd_xml_get_content_int(xml, key, "size");
    }
    del_xml(&xml);
    if (ofst<0 || size<=0) return NULL;

    //
    Buffer enc = set_Buffer(buf+ofst+hdsz, size);
    Buffer dec = gz_decode_data(enc);

    hdsz = llsd_bin_get_length(dec.buf, dec.vldsz);
    xml  = llsd_bin_parse(dec.buf, hdsz);

    return xml;
}



/**
@brief llmesh のXML形式データから PositionDomainの Max/Minパラメータを得る．

PositionDomainはオプションなので，存在しない場合はデフォルト値を返す．@n
llmeshデータ中の何れかのFACETに PositionDomainが存在した場合は，他の全てのFACETにも PositionDomainが存在しなければならない．

@param xml    llmeshデータの入った XML形式の llsdデータ
@param facet  FACETの数．つまり返すベクトルデータの配列の大きさ．
@param max    true: Maxデータを返す．false: Minデータを返す．

@return  Max/Minデータの入ったベクトルの配列．
*/
Vector<float>*  jbxl::GetLLMeshPositionDomainMaxMin(tXML* xml, int facet, bool max)
{
    if (xml==NULL) return NULL;

    size_t len = sizeof(Vector<float>)*facet;;
    Vector<float>* vect = (Vector<float>*)malloc(len);
    for (int i=0; i<facet; i++) {   // Default値
        if (max) vect[i].set( 5.0,  5.0,  5.0);
        else     vect[i].set(-5.0, -5.0, -5.0);
    }

    Buffer buf = make_Buffer(LDATA);
    cat_s2Buffer("<map><key>PositionDomain</key><map><key>", &buf);
    if (max) cat_s2Buffer("Max", &buf);
    else     cat_s2Buffer("Min", &buf);
    cat_s2Buffer("</key><array><real>", &buf);

    int vnum = 0;
    tList* lptop = get_xml_node_list_bystr(xml, (char*)buf.buf);
    tList* lp = lptop;
    while (lp!=NULL) {
        if (lp->altp!=NULL) vnum++;
        lp = lp->next;
    }

    // PositionDomainが存在しない．
    if (vnum==0) {
        free_Buffer(&buf);
        del_tList(&lptop);
        return vect;        
    }
    else if (vnum!=facet) {
        PRINT_MESG("WARNING: GetLLMeshPositionDomainMaxMin: facet number missmatch\n");
        free_Buffer(&buf);
        del_tList(&lptop);
        return vect;        
    }

    //
    int count = 0;
    lp = lptop;
    while (count<facet && lp!=NULL) {
        if (lp->altp!=NULL) {
            tList* lpreal = lp->altp;
            vect[count].x = (float)atof((char*)lpreal->next->ldat.key.buf);
            vect[count].y = (float)atof((char*)lpreal->ysis->next->ldat.key.buf);
            vect[count].z = (float)atof((char*)lpreal->ysis->ysis->next->ldat.key.buf);
            count++;
        }   
        lp = lp->next;
    }

    del_tList(&lptop);
    free_Buffer(&buf);

    return vect;
}



/**
@brief llmesh のXML形式データから TexCord0Domainの Max/Minパラメータを得る．

TexCoord0Domainは TexCoord0オプションが存在する場合は必須．@n
この辺の取り扱いは複雑なので，オプション扱いにして，TexCoord0Domainが存在しない場合はデフォルト値を返す．@n
ただし，llmeshデータ中の何れかのFACETに TexCoord0Domainが存在した場合は，他の全てのFACETにも TexCoord0Domainが存在しなければならない．

@param xml    llmeshデータの入った XML形式の llsdデータ
@param facet  FACETの数．つまり返すベクトルデータの配列の大きさ．
@param max    true: Maxデータを返す．false: Minデータを返す．

@return  Max/Minデータの入ったUVMapの配列．
*/
UVMap<float>*  jbxl::GetLLMeshTexCoordDomainMaxMin(tXML* xml, int facet, bool max)
{
    if (xml==NULL) return NULL;

    size_t len = sizeof(UVMap<float>)*facet;;
    UVMap<float>* uvmp = (UVMap<float>*)malloc(len);
    for (int i=0; i<facet; i++) {   // Default値
        if (max) uvmp[i].set(1.0, 1.0);
        else     uvmp[i].set(0.0, 0.0);
    }

    Buffer buf = make_Buffer(LDATA);
    cat_s2Buffer("<map><key>TexCoord0Domain</key><map><key>", &buf);
    if (max) cat_s2Buffer("Max", &buf);
    else     cat_s2Buffer("Min", &buf);
    cat_s2Buffer("</key><array><real>", &buf);

    int tnum = 0;
    tList* lptop = get_xml_node_list_bystr(xml, (char*)buf.buf);
    tList* lp = lptop;
    while (lp!=NULL) {
        if (lp->altp!=NULL) tnum++;
        lp = lp->next;
    }

    // TexCoord0Domainが存在しない．
    if (tnum==0) {
        free_Buffer(&buf);
        del_tList(&lptop);
        return uvmp;        
    }
    else if (tnum!=facet) {
        PRINT_MESG("WARNING: GetLLMeshTexCoordDomainMaxMin: facet number missmatch\n");
        free_Buffer(&buf);
        del_tList(&lptop);
        return uvmp;        
    }

    //
    int count = 0;
    lp = lptop;
    while (count<facet && lp!=NULL) {
        if (lp->altp!=NULL) {
            tList* lpreal = lp->altp;
            uvmp[count].u = (float)atof((char*)lpreal->next->ldat.key.buf);
            uvmp[count].v = (float)atof((char*)lpreal->ysis->next->ldat.key.buf);
            count++;
        }   
        lp = lp->next;
    }

    del_tList(&lptop);
    free_Buffer(&buf);

    return uvmp;
}



/**
LLMeshデータ中の unsigned short int型のデータを float型に変換する．@n
変換後の値の範囲は min - max の間となる．

@param ptr  変換対象の unsigned short int を指すポインタ．
@param max  float に変換した場合の最大値．
@param min  float に変換した場合の最小値．

@return  変換結果の doubkle型数値．
*/
float  jbxl::LLMeshUint16toFloat(uByte* ptr, float max, float min)
{
    float val = (float)ushort_from_little_endian(ptr);
    float delta = (max - min)/UWORDMAX;

    val = val*delta + min;
    if (fabs(val)<delta) val = 0.0f;

    return val;
}




////////////////////////////////////////////////////////////////////////////////////////////////
// for Terrain Mesh
//

/**
Terrain用の標高データの入った画像データからインデックス化された三角ポリゴンデータ FacetBaseDataを生成する．@n

@param grd      標高データの入った画像データ．
@param shift    平行移動量．Unity3Dの仕様対策
@param left     左の境界を処理するか？
@param right    右の境界を処理するか？
@param top      上方の境界を処理するか？
@param bottom   下方の境界を処理するか？
@param autosea  海底の深さを自動生成するか？

@return  生成したポリゴンデータへのポインタ．
*/
FacetBaseData*  jbxl::FacetBaseDataFromTerrainImage(MSGraph<float> grd, Vector<float> shift, bool left, bool right, bool top, bool bottom, bool autosea)
{
    if (grd.isNull()) return NULL;

    TerrainMesh terrainMesh;
    bool ret = terrainMesh.Image2Coords(grd);
    if (!ret) return NULL;
    terrainMesh.set_boundary(left, right, top, bottom);
    terrainMesh.GenerateMeshData(shift, autosea);

    //
    int inum = (int)terrainMesh.terrainTriIndex.size()*3;   // index の数
    int cnum = (int)terrainMesh.coords.size();              // データ（頂点）の数

    FacetBaseData* facetdata = new FacetBaseData(inum, cnum);
    if (!facetdata->getm()) {
        terrainMesh.free();
        return NULL;
    }

    for (int i=0; i<inum/3; i++) {
        facetdata->index[i*3+0] = terrainMesh.terrainTriIndex[i].v1;
        facetdata->index[i*3+1] = terrainMesh.terrainTriIndex[i].v2;
        facetdata->index[i*3+2] = terrainMesh.terrainTriIndex[i].v3;
    }

    for (int i=0; i<cnum; i++) {
        facetdata->vertex[i] = terrainMesh.coords[i];
        facetdata->normal[i] = terrainMesh.normals[i];
        facetdata->texcrd[i] = terrainMesh.uvs[i];
    }

    terrainMesh.free();
    return facetdata;
}



/**
Terrainの標高データのファイル(R32) からデータを読み込み，インデックス化された三角ポリゴンデータ FacetBaseDataを生成する．@n

@param r32file  Terrainの標高データの入った R32ファイル名．
@param xsize
@param ysize
@param shift    平行移動量．Unity3Dの仕様対策
@param autosea  海底の深さを自動生成するか？

@return  生成したポリゴンデータへのポインタ．
*/
FacetBaseData*  jbxl::FacetBaseDataFromTerrainR32(char* r32file, int xsize, int ysize, Vector<float> shift, bool autosea)
{
    if (r32file==NULL) return NULL;

    size_t sz = file_size(r32file);
    if ((int)sz!=xsize*ysize*4) {
        PRINT_MESG("JBXL::FacetBaseDataFromTerrainR32: ERROR: file size missmatch %d != %dx%dx4\n", xsize, ysize, sz);
        return NULL;
    }

    DEBUG_MODE PRINT_MESG("JBXL::FacetBaseDataFromTerrainR32: reading terrain r32 file %s\n", r32file);
    FILE* fp = fopen(r32file, "rb");
    if (fp==NULL) return NULL;

    MSGraph<float> grd;
    grd.getm(xsize, ysize);
    if (grd.isNull()) return NULL;

    fread(grd.gp, grd.xs*grd.ys*sizeof(float), 1, fp);
    fclose(fp);

    FacetBaseData* facetdata = FacetBaseDataFromTerrainImage(grd, shift, true, true, true, true, autosea);

    grd.free();

    return facetdata;
}



/**
Terrain用の標高データの入った画像データから三角ポリゴンデータ TriPloyDataを生成する．@n
現在の所，未使用．

@param grd        標高データの入った画像データ．
@param[out] pnum  生成したポリゴンデータ（配列）の数．
@param shift      平行移動量．Unity3Dの仕様対策
@param left       左側の境界を処理するか？
@param right      右側の境界を処理するか？
@param top        上方の境界を処理するか？
@param bottom     下方の境界を処理するか？
@param autosea    海底の深さを自動生成するか？

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromTerrainImage(MSGraph<float> grd, int* pnum, Vector<float> shift, bool left, bool right, bool top, bool bottom, bool autosea)
{
    if (grd.isNull() || pnum==NULL) return NULL;

    *pnum = 0;

    TerrainMesh terrainMesh;
    bool ret = terrainMesh.Image2Coords(grd);
    if (!ret) return NULL;
    terrainMesh.set_boundary(left, right, top, bottom);
    terrainMesh.GenerateMeshData(shift, autosea);

    int tnum = (int)terrainMesh.terrainTriArray.size();
    TriPolyData* tridata = (TriPolyData*)malloc(tnum*sizeof(TriPolyData));
    if (tridata==NULL) {
        terrainMesh.free();
        return NULL;
    }

    for (int i=0; i<tnum; i++) {
        tridata[i].has_normal = true;
        tridata[i].has_texcrd = true;
        tridata[i].facetNum   = terrainMesh.terrainTriArray[i].facetNum;
        tridata[i].vertex[0]  = terrainMesh.terrainTriArray[i].v1;
        tridata[i].vertex[1]  = terrainMesh.terrainTriArray[i].v2;
        tridata[i].vertex[2]  = terrainMesh.terrainTriArray[i].v3;
        tridata[i].normal[0]  = terrainMesh.terrainTriArray[i].n1;
        tridata[i].normal[1]  = terrainMesh.terrainTriArray[i].n2;
        tridata[i].normal[2]  = terrainMesh.terrainTriArray[i].n3;
        tridata[i].texcrd[0]  = terrainMesh.terrainTriArray[i].uv1;
        tridata[i].texcrd[1]  = terrainMesh.terrainTriArray[i].uv2;
        tridata[i].texcrd[2]  = terrainMesh.terrainTriArray[i].uv3;
    }
    *pnum = tnum;

    terrainMesh.free();
    return tridata;
}



/**
Terrainの標高データのファイル(R32) からデータを読み込み，三角ポリゴンデータ TriPloyDataを生成する．@n
現在の所，未使用．

@param r32file    Terrainの標高データの入った R32ファイル名．
@param[out] pnum  生成したポリゴンデータ（配列）の数．
@param xsize
@param ysize
@param shift      平行移動量．Unity3Dの仕様対策
@param autosea    海底の深さを自動生成するか？

@return  生成したポリゴンデータの配列へのポインタ．
*/
TriPolyData*  jbxl::TriPolyDataFromTerrainR32(char* r32file, int* pnum, int xsize, int ysize, Vector<float> shift, bool autosea)
{
    if (r32file==NULL || pnum==NULL) return NULL;
    *pnum = 0;

    size_t sz = file_size(r32file);
    if ((int)sz!=xsize*ysize*4) {
        PRINT_MESG("JBXL::TriPolyDataFromTerrainR32: ERROR: file size missmatch %d != %dx%dx4\n", xsize, ysize, sz);
        return NULL;
    }

    DEBUG_MODE PRINT_MESG("JBXL::TriPolyDataFromTerrainR32: reading terrain r32 file %s\n", r32file);
    FILE* fp = fopen(r32file, "rb");
    if (fp==NULL) return NULL;

    MSGraph<float> grd;
    grd.getm(xsize, ysize);
    if (grd.isNull()) return NULL;

    fread(grd.gp, grd.xs*grd.ys*sizeof(float), 1, fp);
    fclose(fp);

    TriPolyData* tridata = TriPolyDataFromTerrainImage(grd, pnum, shift, true, true, true, true, autosea);

    grd.free();

    return tridata;
}




///////////////////////////////////////////////////////////////////////////////////////////////
//

TriPolyData*  jbxl::GenerateGrayScaleSculpt(int* pnum)
{
    PrimBaseShape bases;
    PrimMeshParam param;
    bases.GenerateBaseParam(PRIM_TYPE_SPHERE);
    param.GetParamFromBaseShape(bases);
    bases.free();

    PrimMesh primMesh = GeneratePrimMesh(param);
    param.free();

    Quaternion<double> rot;
    rot.setExtEulerXYZ(Vector<double>(PI_DIV2_3, PI_DIV2_3, 0.0));
    primMesh.execRotate(rot);
    primMesh.execScale(0.6, 0.6, 0.6);

    TriPolyData* tridata = TriPolyDataFromPrimMesh(primMesh, NULL, pnum);
    primMesh.free();

    return tridata;
}



