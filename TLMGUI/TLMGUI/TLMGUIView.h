// TLMGUIView.h : interface of the CTLMGUIView class
//


#pragma once


class CTLMGUIView : public CFormView
{
protected: // create from serialization only
	CTLMGUIView();
	DECLARE_DYNCREATE(CTLMGUIView)

public:
	enum{ IDD = IDD_TLMGUI_FORM };

// Attributes
public:
	CTLMGUIDoc* GetDocument() const;

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnInitialUpdate(); // called first time after construct

// Implementation
public:
	virtual ~CTLMGUIView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in TLMGUIView.cpp
inline CTLMGUIDoc* CTLMGUIView::GetDocument() const
   { return reinterpret_cast<CTLMGUIDoc*>(m_pDocument); }
#endif

