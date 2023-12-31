﻿#include  "stdafx.h"

#include  "ParameterSet.h"


using namespace jbxwl;




void  CParameterSet::init(void)
{
	configFilePath	= makeConfigFilePath();
	configSizePath	= makeConfigSizePath();

	// Parameters
	oarFolder     = GetCurrentFolder();
	daeFolder     = _T(".\\");
	stlFolder     = _T(".\\");
	logFolder     = _T(".\\Log");

	startNum      =  1;
	stopNum       = -1;
	outputDae     = TRUE;
	outputTerrain = TRUE;
	debugMode     = FALSE;
	
	terrainScale  = TRNT_DEFAULT_TEX_SCALE;
	xShift        = 0.0f;
	yShift        = 0.0f;
	zShift        = 0.0f;

	prefixOAR     = _T("OAR_");
	prefixDAE     = _T("DAE_");
	prefixSTL     = _T("STL_");
}



void  CParameterSet::readConfigFile(void)
{
	tList* lt = read_index_tList_file_t(configFilePath, ' ');
	if (lt==NULL) return;

	//
	oarFolder     = get_tstr_param_tList (lt, "oarFolder", (LPCTSTR)oarFolder);
	daeFolder     = get_tstr_param_tList (lt, "daeFolder", (LPCTSTR)daeFolder);
	stlFolder     = get_tstr_param_tList (lt, "stlFolder", (LPCTSTR)stlFolder);
	logFolder     = get_tstr_param_tList (lt, "logFolder", (LPCTSTR)logFolder);

	startNum      = get_int_param_tList  (lt, "startNum", startNum);
	stopNum       = get_int_param_tList  (lt, "stopNum",  stopNum);

	outputTerrain = get_bool_param_tList (lt, "outputTerrain", outputTerrain);
	outputDae     = get_bool_param_tList (lt, "outputDae", outputDae);
	debugMode     = get_bool_param_tList (lt, "debugMode", debugMode);

	terrainScale  = get_float_param_tList(lt, "terrainScale", terrainScale);
	xShift        = get_float_param_tList(lt, "xShift", xShift);
	yShift        = get_float_param_tList(lt, "yShift", yShift);
	zShift        = get_float_param_tList(lt, "zShift", zShift);

	prefixOAR     = get_tstr_param_tList (lt, "prefixOAR", (LPCTSTR)prefixOAR);
	prefixDAE     = get_tstr_param_tList (lt, "prefixDAE", (LPCTSTR)prefixDAE);
	prefixSTL     = get_tstr_param_tList (lt, "prefixSTL", (LPCTSTR)prefixSTL);

	//
	del_all_tList(&lt);
	return;
}



void  CParameterSet::saveConfigFile(void)
{   
	FILE* fp = tfopen(configFilePath, _T("wb"));
	if (fp==NULL) return;

	// CString
	Buffer tmp = make_Buffer(LNAME);
	copy_ts2Buffer(oarFolder, &tmp);
	fprintf(fp, "oarFolder %s\n", (char*)tmp.buf);
	copy_ts2Buffer(daeFolder, &tmp);
	fprintf(fp, "daeFolder %s\n", (char*)tmp.buf);
	copy_ts2Buffer(stlFolder, &tmp);
	fprintf(fp, "stlFolder %s\n", (char*)tmp.buf);
	copy_ts2Buffer(logFolder, &tmp);
	fprintf(fp, "logFolder %s\n", (char*)tmp.buf);
	//
	copy_ts2Buffer(prefixOAR, &tmp);
	fprintf(fp, "prefixOAR %s\n", (char*)tmp.buf);
	copy_ts2Buffer(prefixDAE, &tmp);
	fprintf(fp, "prefixDAE %s\n", (char*)tmp.buf);
	copy_ts2Buffer(prefixSTL, &tmp);
	fprintf(fp, "prefixSTL %s\n", (char*)tmp.buf);
	//
	free_Buffer(&tmp);

	fprintf(fp, "startNum %d\n", startNum);
	fprintf(fp, "stopNum %d\n",  stopNum);
	//
	fprintf(fp, "terrainScale %f\n", terrainScale);
	fprintf(fp, "xShift %f\n", xShift);
	fprintf(fp, "yShift %f\n", yShift);
	fprintf(fp, "zShift %f\n", zShift);
	//
	if (outputTerrain) fprintf(fp, "outputTerrain %s\n", "TRUE");
	else               fprintf(fp, "outputTerrain %s\n", "FALSE");
	if (outputDae) fprintf(fp, "outputDae %s\n", "TRUE");
	else           fprintf(fp, "outputDae %s\n", "FALSE");
	if (debugMode) fprintf(fp, "debugMode %s\n", "TRUE");
	else           fprintf(fp, "debugMode %s\n", "FALSE");

	//
	fflush(fp);
	fclose(fp);
	return;
}




//////////////////////////////////////////////////////////////////
//

void  CParameterSet::readWindowSize(RECT* winsz)
{   
	if (winsz==NULL) return;

	tList* lt = read_index_tList_file_t(configSizePath, ' ');
	if (lt==NULL) return;

	//	
	winsz->left   = (LONG)get_int_param_tList(lt, "windowsPosX",  (int)winsz->left);
	winsz->top	  = (LONG)get_int_param_tList(lt, "windowsPosY",  (int)winsz->top);
	winsz->right  = (LONG)get_int_param_tList(lt, "windowsSizeX", (int)winsz->right);
	winsz->bottom = (LONG)get_int_param_tList(lt, "windowsSizeY", (int)winsz->bottom);

	del_all_tList(&lt);
	return;
}



void  CParameterSet::saveWindowSize(RECT winsz)
{   
	FILE* fp = tfopen(configSizePath, _T("wb"));
	if (fp==NULL) return;

	int wx = winsz.right  - winsz.left;
	int wy = winsz.bottom - winsz.top;
	if (winsz.left>0) fprintf(fp, "windowsPosX  %d\n", winsz.left);
	if (winsz.top>0)  fprintf(fp, "windowsPosY  %d\n", winsz.top);
	if (wx>0)		  fprintf(fp, "windowsSizeX %d\n", wx);
	if (wy>0)		  fprintf(fp, "windowsSizeY %d\n", wy);

	fflush(fp);
	fclose(fp);
	return;
}



CString  CParameterSet::makeConfigFilePath(void)
{
	// Roming
	CString configf = MakeWorkingFolderPath(_T(OARCONV_CONFIG_FILE), FALSE, _T(OARCONV_CONFIG_PATH), TRUE);

	return configf;
}



CString  CParameterSet::makeConfigSizePath(void)
{
	// Roming
	CString configf = MakeWorkingFolderPath(_T(OARCONV_WINSIZE_FILE), FALSE, _T(OARCONV_CONFIG_PATH), TRUE);

	return configf;
}
