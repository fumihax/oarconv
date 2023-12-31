﻿// OARConvWin.h : OARConvWin アプリケーションのメイン ヘッダー ファイル
//
#pragma once

#ifndef __AFXWIN_H__
	#error このファイルを PCH に含める前に、'stdafx.h' を含めてください
#endif

#include  "resource.h"       // メイン シンボル
#include  "ExClass.h"
#include  "MainFrm.h"

#include  "OARConvWin_config.h"

#include  "MFCLib.h"
#include  "ParameterSet.h"
#include  "MessageBoxDLG.h"
#include  "ProgressBarDLG.h"
#include  "ObjectsListDLG.h"

#include  "OARTool.h"
#pragma comment(lib, "liboarconv.lib")


using namespace jbxl;
using namespace jbxwl;


//
class COARConvWinApp : public CWinApp, public CAppCallBack
{
public:
	COARConvWinApp();
	virtual ~COARConvWinApp();

    CMainFrame*   pMainFrame;
	CLogWndFrame* pLogFrame;
	CLogWndDoc*	  pLogDoc;

	CMultiDocTemplate* pDocTemplLOG;
	CMultiDocTemplate* pDocTemplBREP;

	CMessageBoxDLG*  aboutBox;
	CObjectsListDLG* objListBox;
	
	CParameterSet appParam;
	RECT          windowSize;

public:
	OARTool  oarTool;

	Buffer   homeFolder;
	Buffer   assetsFolder;
	Buffer   comDecomp;

	bool     hasData;
	bool     isConverting;

public:
	void	 setOarFolder(LPCTSTR fldr) { appParam.oarFolder = fldr;}
	void	 setDaeFolder(LPCTSTR fldr) { appParam.daeFolder = fldr;}
	void	 setStlFolder(LPCTSTR fldr) { appParam.stlFolder = fldr;}

	LPCTSTR  getOarFolder(void) { return (LPCTSTR)appParam.oarFolder;}
	LPCTSTR  getDaeFolder(void) { return (LPCTSTR)appParam.daeFolder;}
	LPCTSTR  getStlFolder(void) { return (LPCTSTR)appParam.stlFolder;}

	bool	 fileOpen(CString);
	bool	 fileOpenOAR  (CString);
	bool	 folderOpenOAR(CString);

	//
	void	 convertAllData(void);
	int		 convertAllFiles(void);

	void     convertSelectedData (int selectedNums, int* selectedObjs);
	int      convertSelectedFiles(int selectedNums, int* selectedObjs);

	void     convertOneData(int index, BOOL outputDae);
	int      convertOneFile(int index, BOOL outputDae);

public:
	void     solidOpenBrep(BREP_SOLID* solid, LPCTSTR title, int num);
	//
	void	 showOARInfoDLG(void);
	void     updateMenuBar(CMenu* menu=NULL);
	void     updateStatusBar(CString path);

//
// オーバーライド
public:
	virtual BOOL InitInstance();
	virtual void FrameDestructor(CExTextFrame* frm);
	virtual void ViewDestructor(CExTextView* view);

// 実装
	afx_msg void OnAppAbout();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileOpen();
	afx_msg void OnFolderOpen();
	afx_msg void OnFileOpenQuick();
	afx_msg void OnFolderOpenQuick();
	afx_msg void OnSettingDialog();
	afx_msg void OnLogWindow();
	afx_msg void OnConvertData();
	afx_msg void OnObjectsList();
};




extern COARConvWinApp theApp;



