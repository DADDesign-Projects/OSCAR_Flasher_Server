
// OSCAR_Flasher_ServerDlg.h : fichier d'en-tête
//

#pragma once
#include "cServer.h"

// boîte de dialogue de COSCARFlasherServerDlg
class COSCARFlasherServerDlg : public CDialogEx
{
// Construction
public:
	COSCARFlasherServerDlg(CWnd* pParent = nullptr);	// constructeur standard

// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OSCAR_FLASHER_SERVER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// Prise en charge de DDX/DDV


// Implémentation
protected:
	HICON m_hIcon;
	CWinThread* m_pFlashThread = nullptr;

	// Fonctions générées de la table des messages
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedAddFile();
	afx_msg void OnBnClickedDeleteFile();
	afx_msg void OnBnClickedFlash();
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnLbnSelchangeListCom();
	CEdit m_Edit;
	CStatic m_TitreListeFiles;
	CStatic m_TitreListeCom;
	CListBox m_ListeFiles;
	CListBox m_ListeCom;
	CProgressCtrl m_Progress;
	CButton m_ButtonFlash;
};
