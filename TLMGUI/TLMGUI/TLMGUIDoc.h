// TLMGUIDoc.h : interface of the CTLMGUIDoc class
//


#pragma once


class CTLMGUIDoc : public CDocument
{
protected: // create from serialization only
	CTLMGUIDoc();
	DECLARE_DYNCREATE(CTLMGUIDoc)

// Attributes
public:

// Operations
public:

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~CTLMGUIDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};


