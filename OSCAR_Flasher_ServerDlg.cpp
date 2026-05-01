
// OSCAR_Flasher_ServerDlg.cpp : fichier d'implémentation
//

#include "pch.h"
#include "framework.h"
#include "OSCAR_Flasher_Server.h"
#include "OSCAR_Flasher_ServerDlg.h"
#include "afxdialogex.h"
#include "CEnumCOM.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Fonction de Gestion du Thread de transfert des fichiers pour flash
UINT Flash(LPVOID pParam);

// boîte de dialogue CAboutDlg utilisée pour la boîte de dialogue 'À propos de' pour votre application

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

// Implémentation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// boîte de dialogue de COSCARFlasherServerDlg



COSCARFlasherServerDlg::COSCARFlasherServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_OSCAR_FLASHER_SERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void COSCARFlasherServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_Edit);
	DDX_Control(pDX, IDC_LIST_FILE_TITRE, m_TitreListeFiles);
	DDX_Control(pDX, IDC_LIST_COM_TITRE, m_TitreListeCom);
	DDX_Control(pDX, IDC_LIST_FILE, m_ListeFiles);
	DDX_Control(pDX, IDC_LIST_COM, m_ListeCom);
	DDX_Control(pDX, IDC_PROGRESS1, m_Progress);
	DDX_Control(pDX, IDC_FLASH, m_ButtonFlash);
}

BEGIN_MESSAGE_MAP(COSCARFlasherServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_ADD_FILE, &COSCARFlasherServerDlg::OnBnClickedAddFile)
	ON_BN_CLICKED(IDC_DELETE_FILE, &COSCARFlasherServerDlg::OnBnClickedDeleteFile)
	ON_BN_CLICKED(IDC_FLASH, &COSCARFlasherServerDlg::OnBnClickedFlash)
	ON_BN_CLICKED(IDC_REFRESH, &COSCARFlasherServerDlg::OnBnClickedRefresh)
	ON_LBN_SELCHANGE(IDC_LIST_COM, &COSCARFlasherServerDlg::OnLbnSelchangeListCom)
END_MESSAGE_MAP()


// gestionnaires de messages de COSCARFlasherServerDlg

BOOL COSCARFlasherServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Ajouter l'élément de menu "À propos de..." au menu Système.

	// IDM_ABOUTBOX doit se trouver dans la plage des commandes système.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Définir l'icône de cette boîte de dialogue.  L'infrastructure effectue cela automatiquement
	//  lorsque la fenêtre principale de l'application n'est pas une boîte de dialogue
	SetIcon(m_hIcon, TRUE);			// Définir une grande icône
	SetIcon(m_hIcon, FALSE);		// Définir une petite icône

	// TODO: ajoutez ici une initialisation supplémentaire
	OnBnClickedRefresh();
	m_Edit.SetWindowText(L"Choose :\r\n - a communication port,\r\n - the files to be transferred to\r\n    OSCAR's QSPI Flash memory.\r\n\r\nClick Flash");

	return TRUE;  // retourne TRUE, sauf si vous avez défini le focus sur un contrôle
}

void COSCARFlasherServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// Si vous ajoutez un bouton Réduire à votre boîte de dialogue, vous devez utiliser le code ci-dessous
//  pour dessiner l'icône.  Pour les applications MFC utilisant le modèle Document/Vue,
//  cela est fait automatiquement par l'infrastructure.

void COSCARFlasherServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // contexte de périphérique pour la peinture

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Centrer l'icône dans le rectangle client
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Dessiner l'icône
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// Le système appelle cette fonction pour obtenir le curseur à afficher lorsque l'utilisateur fait glisser
//  la fenêtre réduite.
HCURSOR COSCARFlasherServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void COSCARFlasherServerDlg::OnBnClickedAddFile()
{
	// Vérification du nombre maximal de fichiers
	int NbFiles = m_ListeFiles.GetCount();
	if (NbFiles >= DIR_FILE_COUNT) {
		MessageBox(L"Maximum file number size reached", L"Error Add File", MB_OK | MB_ICONWARNING);
		return;
	}

	// Configuration de la boîte de dialogue pour la sélection multiple
	CFileDialog FileDialog(TRUE, NULL, NULL, OFN_ALLOWMULTISELECT | OFN_EXPLORER, L"All Files|*.*||");

	// Allocation d'un buffer pour stocker plusieurs chemins
	const int BufferSize = 4096;
	wchar_t Buffer[BufferSize] = { 0 };
	FileDialog.m_ofn.lpstrFile = Buffer;
	FileDialog.m_ofn.nMaxFile = BufferSize;

	if (IDOK == FileDialog.DoModal()) {
		POSITION pos = FileDialog.GetStartPosition();
		while (pos) {
			CString NomFichier = FileDialog.GetNextPathName(pos);

			// Vérifier si le nombre de fichiers dépasse la limite
			if (m_ListeFiles.GetCount() >= DIR_FILE_COUNT) {
				MessageBox(L"Maximum file number size reached", L"Error Add File", MB_OK | MB_ICONWARNING);
				break;
			}

			// Ajouter le fichier à la liste
			m_ListeFiles.InsertString(m_ListeFiles.GetCount(), NomFichier);
		}
	}
}

void COSCARFlasherServerDlg::OnBnClickedDeleteFile()
{
	int Select = m_ListeFiles.GetCurSel();
	CString NomFichier;
	if (Select != -1) {
		m_ListeFiles.GetText(Select, NomFichier);
		if (IDYES == MessageBox(CString(L"Delete : " + NomFichier + L" ?"), L"Delete file in the list", MB_YESNO | MB_ICONWARNING)) {
			m_ListeFiles.DeleteString(Select);
		}
	}
}

void COSCARFlasherServerDlg::OnBnClickedFlash()
{
	if (m_pFlashThread) {
		// Vérifie si le thread précédent est terminé
		DWORD result = WaitForSingleObject(m_pFlashThread->m_hThread, 0);
		if (result == WAIT_TIMEOUT) {
			return;
		}
	}
	m_ButtonFlash.EnableWindow(FALSE);
	m_pFlashThread = AfxBeginThread(
		Flash,						// Fonction de contrôle du thread
		this,						// Paramètre à passer à la fonction
		THREAD_PRIORITY_HIGHEST,		// Priorité du thread (par défaut normale)
		0);
}

void COSCARFlasherServerDlg::OnBnClickedRefresh()
{
	Dad::cEnumCommPorts EnumPort;
	EnumPort.UpdateCommPorts();
	m_ListeCom.ResetContent();
	uint16_t NbCOM = EnumPort.getNbPortsFound();
	for (uint16_t Index = 0; Index < NbCOM; Index++) {
		uint16_t PortNum = EnumPort.getNumComm(Index + 1);
		CString NomPort;
		NomPort.Format(L"COM%d", PortNum);
		m_ListeCom.InsertString(Index, NomPort);
		m_ListeCom.SetItemData(Index, PortNum);
	}
}

// Fonction de Gestion du Thread de transfert des fichiers pour flash
UINT Flash(LPVOID pParam) {
	COSCARFlasherServerDlg* pDialog = (COSCARFlasherServerDlg*)pParam;
	Dad::cServer Server;
	CString NomFichier;
	CString PathFichier;

	HCURSOR hMemCurseur = GetCursor();
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	int NbFiles = pDialog->m_ListeFiles.GetCount();
	if (NbFiles == 0) {
		MessageBox(GetActiveWindow(), L"Please add a file", L"No file selected", MB_OK | MB_ICONWARNING);
		pDialog->m_ButtonFlash.EnableWindow(TRUE);
		SetCursor(hMemCurseur);
		return -1;
	}

	// Lecture du port sélectionné
	int Select = pDialog->m_ListeCom.GetCurSel();
	if (Select == -1) {
		MessageBox(GetActiveWindow(), L"Please select a COM port", L"No COM port selected", MB_OK | MB_ICONWARNING);
		pDialog->m_ButtonFlash.EnableWindow(TRUE);
		SetCursor(hMemCurseur);
		return -1;
	}
	uint8_t NumPort = pDialog->m_ListeCom.GetItemData(Select);
	Server.Init(NumPort, QSPI_SIZE);

	// Mise en mémoire des fichiers
	for (int Index = 0; Index < NbFiles; Index++) {
		pDialog->m_ListeFiles.GetText(Index, PathFichier);
		NomFichier = PathFindFileName(PathFichier);

		CW2A asciiPathFichier(PathFichier);
		const char* cPathFichier = asciiPathFichier;
		CW2A asciiNomFichier(NomFichier);
		const char* cNomFichier = asciiNomFichier;
		if (false == Server.addFile(std::string(cPathFichier), std::string(cNomFichier))) {
			MessageBox(GetActiveWindow(), CString(L"Error loading " + NomFichier + L" file"), L"File loading error", MB_OK | MB_ICONERROR);
			pDialog->m_Edit.SetWindowText(L"File loading error");
			pDialog->m_ButtonFlash.EnableWindow(TRUE);
			SetCursor(hMemCurseur);
			return -1;
		}
	}

	// Transfert des données
	int16_t NbBloc = Server.getNbBlocs();
	pDialog->m_Progress.SetRange(0, NbBloc);
	pDialog->m_Progress.SetPos(0);
	for (int16_t IndexBloc = 0; IndexBloc < NbBloc; IndexBloc++) {

		// Synchronisation avec Daisy
		int16_t SynchroResult = Server.Synchronize();
		if (-1 == SynchroResult) {
			MessageBox(GetActiveWindow(), L"Synchronization with OSCAR impossible (Maybe Change COM Port?)", L"Transfer error", MB_OK | MB_ICONERROR);
			pDialog->m_Edit.SetWindowText(L"Unable to communicate with OSCAR");
			pDialog->m_ButtonFlash.EnableWindow(TRUE);
			SetCursor(hMemCurseur);
			return -1;
		}
		if (IndexBloc != SynchroResult) {
			MessageBox(GetActiveWindow(), L"Communication problem detected, flashing in progress stopped.", L"Transfer error", MB_OK | MB_ICONERROR);
			pDialog->m_Edit.SetWindowText(L"File transfer or flash\r\nprocedure failed");
			pDialog->m_ButtonFlash.EnableWindow(TRUE);
			SetCursor(hMemCurseur);
			return -1;
		}
		// Transmission d'un bloc
		uint8_t EndTrans = IndexBloc == (NbBloc - 1) ? 1 : 0;
		if (!Server.TransBloc(IndexBloc, EndTrans)) {
			MessageBox(GetActiveWindow(), L"Communication problem detected (COM port inaccessible), flashing in progress stopped.", L"Transfer error", MB_OK | MB_ICONERROR);
			pDialog->m_Edit.SetWindowText(L"File transfer or flash\r\nprocedure failed");
			pDialog->m_ButtonFlash.EnableWindow(TRUE);
			SetCursor(hMemCurseur);
			return -1;
		}
		pDialog->m_Progress.SetPos(IndexBloc + 1);

		CString strMessage;
		int PourCent = ((IndexBloc + 1) * 100) / NbBloc;
		strMessage.Format(_T("Transfer in progress \r\n%d%% completed"), PourCent);
		pDialog->m_Edit.SetWindowText(strMessage);
	}
	pDialog->m_Edit.SetWindowText(L"Transfer and flash completed");
	pDialog->m_ButtonFlash.EnableWindow(TRUE);
	SetCursor(hMemCurseur);
	return 0;
}

void COSCARFlasherServerDlg::OnLbnSelchangeListCom()
{
	HCURSOR hMemCurseur = GetCursor();
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	int Select = m_ListeCom.GetCurSel();
	if (Select != -1) {
		Dad::cServer Server;
		uint8_t NumPort = m_ListeCom.GetItemData(Select);
		if (true == Server.Init(NumPort)) {
			if (-1 == Server.Synchronize()) {
				CString Msg;
				Msg.Format(L"OSCAR not detected on COM%d", NumPort);
				m_Edit.SetWindowText(Msg);
			}
			else {
				CString Msg;
				Msg.Format(L"OK : OSCAR is detected\r\non COM%d", NumPort);
				m_Edit.SetWindowText(Msg);
			}
		}
		else {
			CString Msg;
			Msg.Format(L"COM%d is not accessible", NumPort);
			MessageBox(Msg, L"COM Error", MB_OK | MB_ICONERROR);
		}
	}
	SetCursor(hMemCurseur);
}
