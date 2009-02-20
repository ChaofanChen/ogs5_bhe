// DialogGHDB_Connect.cpp : implementation file
//
/*************************************************************************
  ROCKFLOW - Function: GHDB Connection
  Task: read Geodatabase files
  Programming: 10/11/2008 FS
  last modified:11/12/2008 FS
**************************************************************************/

#include "stdafx.h"
#include "GeoSys.h"
#include "dlg_GHDB.h"
#include "MainFrm.h" 


vector<double> GHDBvector_x;
vector<double> GHDBvector_y;
CString GHDBViewTitle;

// CDialogGHDB_Connect dialog

IMPLEMENT_DYNAMIC(CDialogGHDB_Connect, CDialog)

CDialogGHDB_Connect::CDialogGHDB_Connect(CWnd* pParent /*=NULL*/)
	: CDialog(CDialogGHDB_Connect::IDD, pParent)
//	, FromData(_T(""))
//	, Endtime(_T(""))
//, TimeSeries(0)
{
ncolumns=0;

}

CDialogGHDB_Connect::~CDialogGHDB_Connect()
{
}

void CDialogGHDB_Connect::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GHDB_File, edit_file_name);
	DDX_Control(pDX, IDC_LIST1, TableList);

	DDX_Control(pDX, IDC_LIST_Fields, FieldListBox);


	DDX_Control(pDX, IDC_BUTTON2, GetValue);

	DDX_Control(pDX, IDC_COMBO1, Sta_ID);
	DDX_Control(pDX, IDC_BUTTON3, Search);

	DDX_Control(pDX, IDC_LIST2, ResultList);

	DDX_Control(pDX, IDC_EDIT1, From);
	DDX_Control(pDX, IDC_EDIT2, End);

}



BEGIN_MESSAGE_MAP(CDialogGHDB_Connect, CDialog)
	ON_BN_CLICKED(IDC_FILE_GHDB, &CDialogGHDB_Connect::OnBnClickedFileGhdb)
	ON_EN_CHANGE(IDC_GHDB_File, &CDialogGHDB_Connect::OnEnChangeGhdbFile)
    ON_BN_CLICKED(IDC_GHDB_Connect, &CDialogGHDB_Connect::OnBnClickedGhdbConnect)
	ON_BN_CLICKED(IDC_BUTTON1, &CDialogGHDB_Connect::OnBnClickedImporting)

	
	ON_BN_CLICKED(IDCANCEL2, &CDialogGHDB_Connect::OnBnClickedCancel)

	ON_BN_CLICKED(IDC_BUTTON2, &CDialogGHDB_Connect::OnBnClickedGV)
	

	ON_BN_CLICKED(IDC_BUTTON3, &CDialogGHDB_Connect::OnBnClickedButton3)

	ON_CBN_SELCHANGE(IDC_COMBO1, &CDialogGHDB_Connect::OnCbnSelchangeCombo1)
	
	ON_EN_CHANGE(IDC_EDIT1, &CDialogGHDB_Connect::OnEnChangeEdit1)
	ON_EN_CHANGE(IDC_EDIT2, &CDialogGHDB_Connect::OnEnChangeEdit2)
			
	ON_BN_CLICKED(IDC_OK, &CDialogGHDB_Connect::OnBnClickedOk)
END_MESSAGE_MAP()



// CDialogGHDB_Connect message handlers


void CDialogGHDB_Connect::OnBnClickedFileGhdb()
{

	TableList.ResetContent();
	FieldListBox.ResetContent();
	Sta_ID.ResetContent();
	ResultList.ResetContent();
	Sta_ID.ResetContent();
	
	//for(int j=ncolumns-1;j>=0;j--)
	//pro_table.DeleteColumn(j);
	//ncolumns=0;

	// TODO: Load file name.
	CFileDialog fileDlg(TRUE,"mdb",NULL,OFN_ENABLESIZING,"GHDB Files (*.mdb)|*.mdb||");
	if (fileDlg.DoModal()==IDOK) 
   {
	   filename =  fileDlg.GetPathName();
	   edit_file_name.SetWindowTextA(filename);
	   edit_file_name.SetFocus();
	}
}

void CDialogGHDB_Connect::OnEnChangeGhdbFile()
{
	// TODO:  If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	// TODO:  Add your control notification handler code here
}



void CDialogGHDB_Connect::OnBnClickedGhdbConnect()
{
   
	//Open Geodatabase in the TableList.
 
	TableList.ResetContent();
  
	HRESULT hr; 
	try 
	{ 
	  hr = m_pConnection.CreateInstance("ADODB.Connection");   
	  if(SUCCEEDED(hr)) 
	  { 
		CString dd; 
		dd.Format("Provider=Microsoft.Jet.OLEDB.4.0;Data Source=%s",filename); 
		hr = m_pConnection->Open((_bstr_t)dd,"","",adModeUnknown); 
		pSet = m_pConnection->OpenSchema(adSchemaTables); 
		while(!(pSet->adoEOF)) 
		{ 
		  //***get tables
		  _bstr_t table_name = pSet->Fields->GetItem("TABLE_NAME")->Value;

		  //***get tabels types
		 _bstr_t table_type = pSet->Fields->GetItem("TABLE_TYPE")->Value;

		  //***output table name
			  if ( strcmp(((LPCSTR)table_type),"TABLE")==0)
			  {
				CString tt;
				tt.Format("%s",(LPCSTR)table_name);
	//			if((tt.Find("_Index")==-1)&&(tt.Find("GDB_")!=0))
				if(tt.Find( "TimeSeries")!=string::npos)
				  TableList.AddString(tt);	
			  } 
		  pSet->MoveNext(); 
		}
		pSet->Close(); 
	  } 
	  m_pConnection->Close(); 
	}
	catch(_com_error e)//chase errors
		{ 
			CString errormessage; 
			errormessage.Format("It is failed to connectiong database!rn error information:%s",e.ErrorMessage());
			AfxMessageBox(errormessage);
		}
} 
void CDialogGHDB_Connect::OnBnClickedImporting()
{	
	FieldListBox.ResetContent();
	ResultList.ResetContent();
	Sta_ID.ResetContent();

	long i;
	int index;
	
	index = TableList.GetCurSel();
	TableList.GetText(index,tablename);

		//ProListBox.ResetContent();

	//pro_table.DeleteAllItems();
	//for(int j=ncolumns-1;j>=0;j--)
	//  pro_table.DeleteColumn(j);
	//ncolumns=0;

	//*** Write table fields into fieldlistbox.
	HRESULT HRec; 
	try 
		{ 
		m_pConnection.CreateInstance("ADODB.Connection");
		HRec = pSet.CreateInstance("ADODB.Recordset"); 
		if(SUCCEEDED(HRec)) 
			{ 
				CString dd; 
				dd.Format("Provider=Microsoft.Jet.OLEDB.4.0;Data Source=%s",filename); 
				HRec = m_pConnection->Open((_bstr_t)dd,"","",adModeUnknown); 
				if(SUCCEEDED(HRec))
				{
					CString ff;
					ff.Format("SELECT * FROM %s",tablename);
					HRec = pSet->Open((_bstr_t)ff,m_pConnection.GetInterfacePtr(),adOpenStatic,adLockOptimistic,adCmdText);
					
						  
					if(SUCCEEDED(HRec))
						{
	//*** Get records number.
						  rec=0;
						  while(!pSet->adoEOF)  
						  {  					   
							rec++;  
							pSet->MoveNext();  
						  } 
	//*** Initialize list control records.
	//					LVITEM lvItem;
	//					int nItem;
	//					lvItem.mask = LVIF_TEXT;
	//					lvItem.iItem = 0;
	//					lvItem.iSubItem = 0;
	//					lvItem.pszText = "";
	//					for(i=0; i<rec; i++)  
	//					nItem = pro_table.InsertItem(&lvItem);
	//					pSet->MoveFirst(); 
	//*** Get fields name.
						Fields *  fields = NULL;
						HRec = pSet->get_Fields(&fields);	
		 
						if(SUCCEEDED(HRec)) 
							{
								long ColCount;
								fields->get_Count(&ColCount);	
								for(i=0;i< ColCount; i++)
								{															
								   FieldListBox.AddString((LPCSTR)fields->Item[i]->GetName());
								}
							   	
							}
						 pSet->MoveFirst(); 
							// pSet->Close(); 
						}
					
				}
			pSet->Close(); 	
			}
	 //m_pConnection->Close(); 
	}
	catch(_com_error e)//chase errors
		{ 
			CString errormessage; 
			errormessage.Format("It is failed to open the table!rn error information:%s",e.ErrorMessage());
			AfxMessageBox(errormessage);
		}
}



BOOL CDialogGHDB_Connect::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
		
	
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}




 void CDialogGHDB_Connect::OnBnClickedGV()
 {
	 // TODO: Add your control notification handler code here

	Sta_ID.ResetContent();
	HRESULT HRec; 
	int index;
	CString fieldname,strName;
	index = FieldListBox.GetCurSel();
	FieldListBox.GetText(index,fieldname);

	_variant_t var;
	
	CString ws; 					
	try 
	{ 
	ws.Format("SELECT DISTINCT %s FROM %s",fieldname,tablename);
	HRec = pSet->Open((_bstr_t)ws, m_pConnection.GetInterfacePtr(),adOpenStatic,adLockOptimistic,adCmdText);
	
	if(SUCCEEDED(HRec))
	
	{	
		
			while(!(pSet->adoEOF))
			{				
				
	 		var = pSet->GetCollect((_bstr_t)fieldname);
 
			if(var.vt != VT_NULL)
				{ 
				strName = (LPCSTR)_bstr_t(var);
							
					Sta_ID.AddString(strName);
			
				
				}
			pSet->MoveNext();
			
			}

			 
	}
	pSet->Close(); 	

	Sta_ID.SelectString(0,strName);
 }
	catch(_com_error e)//chase errors
		{ 
			CString errormessage; 
			errormessage.Format("Wrong Connection:%s",e.ErrorMessage());
			AfxMessageBox(errormessage);
		}
 }




 void CDialogGHDB_Connect::OnCbnSelchangeCombo1()
 {
	 // TODO: Add your control notification handler code here
 

	//int index1 = Sta_ID.GetCurSel();

	//if( index1 != CB_ERR )
	
	//	Sta_ID.GetLBText(index1, FeatureID);
	
 }


 void CDialogGHDB_Connect::OnEnChangeEdit1()
 {
	 From.GetWindowTextA(Date1);
	 
	// TODO:  If this is a RICHEDIT control, the control will not
	 // send this notification unless you override the CDialog::OnInitDialog()
	 // function and call CRichEditCtrl().SetEventMask()
	 // with the ENM_CHANGE flag ORed into the mask.

	 // TODO:  Add your control notification handler code here
 }

 void CDialogGHDB_Connect::OnEnChangeEdit2()
 {
	 End.GetWindowTextA(Date2);
	 // TODO:  If this is a RICHEDIT control, the control will not
	 // send this notification unless you override the CDialog::OnInitDialog()
	 // function and call CRichEditCtrl().SetEventMask()
	 // with the ENM_CHANGE flag ORed into the mask.

	 // TODO:  Add your control notification handler code here
 }
void CDialogGHDB_Connect::OnBnClickedButton3()
 {
	 GHDBvector_x.clear();
	 GHDBvector_y.clear();

	 // TODO: Add your control notification handler code here
	 ResultList.ResetContent();
	 HRESULT HRec; 
	 _variant_t var;
	 try
	 {

		int index1 = Sta_ID.GetCurSel();

		if( index1 != CB_ERR )
		
			Sta_ID.GetLBText(index1, FID);
			CString SearchCon; 	
			SearchCon.Format("SELECT * FROM %s WHERE FeatureID = %s AND TSDateTime BETWEEN # %s # AND # %s # ",tablename,FID,Date1,Date2);
			
			//SearchCon.Format("SELECT * FROM %s  WHERE TSDateTime BETWEEN to_date('05/01/2000', 'MM/DD/YYYY') AND to_date('05/10/2000', 'MM/DD/YYYY')",tablename);
			//SearchCon.Format("SELECT * FROM %s  Where TSDateTime BETWEEN %s and %s ",tablename,BData,EData);
			//SearchCon.Format("SELECT *FROM %s Where FeatureID = %s ",tablename,FID);

			int i=0;

			HRec = pSet->Open((_bstr_t)SearchCon, m_pConnection.GetInterfacePtr(),adOpenStatic,adLockOptimistic,adCmdText);
		
			if(SUCCEEDED(HRec))
			
			{	
				CString FIDName,DateName,Value;

				while(!(pSet->adoEOF))
				{	
					i++;
					
					var = pSet->GetCollect((_bstr_t)"FeatureID");

					if(var.vt != VT_NULL)
					
						FIDName = (LPCSTR)_bstr_t(var);
										
					
						var = pSet->GetCollect((_bstr_t)"TSDateTime");
					if(var.vt != VT_NULL)
					
						DateName = (LPCSTR)_bstr_t(var);
					
						GHDBvector_x.push_back(i);
											
						var = pSet->GetCollect((_bstr_t)"TSValue");
					if(var.vt != VT_NULL)
					 
						Value = (LPCSTR)_bstr_t(var);
					
						GHDBvector_y.push_back(atof(Value));
						
								
					ResultList.AddString(FIDName + " -- >" + DateName + " -- >" + Value);
					
					pSet->MoveNext();
						
				}

			}
 		pSet->Close(); 	
	 }
	 catch(_com_error e)//chase errors
		{ 
			CString errormessage; 
			errormessage.Format("Wrong search conditions,Please select the right station ID and time.:%s",e.ErrorMessage());
			AfxMessageBox(errormessage);
		}
 }

void CDialogGHDB_Connect::OnBnClickedCancel()
{


	CDialog::OnCancel();
	
}




 void CDialogGHDB_Connect::OnBnClickedOk()
 {
	 if (GHDBvector_y.size() > 0)
	 {
		 GHDBViewTitle.Format(" %s : Station ID = %s , From  %s to  %s ",tablename,FID,Date1,Date2);
	 // TODO: Add your control notification handler code here

		CMainFrame* mainframe = (CMainFrame*)AfxGetMainWnd(); //OK
  //mainframe->m_bIsControlPanelOpen = false; //OK

		mainframe->OnGhdbviewCreate();
		CDialog::OnOK();
	 }
    
	 else
	 {
			CString errormessage; 
			errormessage.Format("Please Set Search Conditions at first,otherwise please close the dialogue.");
			AfxMessageBox(errormessage);
	 }
	   

	
 }
