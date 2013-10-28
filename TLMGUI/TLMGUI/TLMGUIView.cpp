// TLMGUIView.cpp : implementation of the CTLMGUIView class
//

#include "stdafx.h"
#include "TLMGUI.h"

#include "TLMGUIDoc.h"
#include "TLMGUIView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTLMGUIView

IMPLEMENT_DYNCREATE(CTLMGUIView, CFormView)

BEGIN_MESSAGE_MAP(CTLMGUIView, CFormView)
END_MESSAGE_MAP()

// CTLMGUIView construction/destruction

CTLMGUIView::CTLMGUIView()
	: CFormView(CTLMGUIView::IDD)
{
	// TODO: add construction code here

}

CTLMGUIView::~CTLMGUIView()
{
}

void CTLMGUIView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
}

BOOL CTLMGUIView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CFormView::PreCreateWindow(cs);
}

void CTLMGUIView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();
	GetParentFrame()->RecalcLayout();
	ResizeParentToFit();

}


// CTLMGUIView diagnostics

#ifdef _DEBUG
void CTLMGUIView::AssertValid() const
{
	CFormView::AssertValid();
}

void CTLMGUIView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CTLMGUIDoc* CTLMGUIView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CTLMGUIDoc)));
	return (CTLMGUIDoc*)m_pDocument;
}
#endif //_DEBUG


// CTLMGUIView message handlers
