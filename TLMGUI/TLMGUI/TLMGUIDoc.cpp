// TLMGUIDoc.cpp : implementation of the CTLMGUIDoc class
//

#include "stdafx.h"
#include "TLMGUI.h"

#include "TLMGUIDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTLMGUIDoc

IMPLEMENT_DYNCREATE(CTLMGUIDoc, CDocument)

BEGIN_MESSAGE_MAP(CTLMGUIDoc, CDocument)
END_MESSAGE_MAP()


// CTLMGUIDoc construction/destruction

CTLMGUIDoc::CTLMGUIDoc()
{
	// TODO: add one-time construction code here

}

CTLMGUIDoc::~CTLMGUIDoc()
{
}

BOOL CTLMGUIDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}




// CTLMGUIDoc serialization

void CTLMGUIDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}


// CTLMGUIDoc diagnostics

#ifdef _DEBUG
void CTLMGUIDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CTLMGUIDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CTLMGUIDoc commands
