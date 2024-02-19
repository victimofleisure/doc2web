// Copyleft 2005 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      26feb05	initial version
		01		29jan13	in CMD_BEGCHAP case, precede nested <dl> with <dd>
		02		15apr13	add HTML help file generation
		03		01jul14	allow duplicate topic titles
		04		03jul14	add item support
		05		05aug14	allow items to be help targets
		06		27aug14	make topic a class; add topic reference counts
		06		06oct15	in ItemAnchor case, fix missing closing tag
		07		26oct21	bump header level for items in single document
		08		31jan24	support item anchor anywhere in line

		enhanced array with copy ctor, assignment, and fast const access
 
*/

// doc2web.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "doc2web.h"
#include <direct.h>
#include <afxtempl.h>
#include "PathStr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// The one and only application object

CWinApp theApp;

using namespace std;

#define HELP_FILE_ITEM_ALIASES	1	// non-zero to allow items to be help targets

class CDoc2Web : public CObject {
public:
	enum {
		NUM_PREFIX	= 0x01,
		NO_SPACES	= 0x02,
		LOG_REFS	= 0x04,	// output reference counts for each topic to log file
	};
	bool	Process(LPCSTR SrcPath, LPCSTR DstFolder, LPCSTR TocPath,
					LPCSTR DocPath, LPCSTR DocTitle, LPCSTR HelpPath, int Options);
private:
	class CTopic : public CObject {
	public:
		CTopic();
		CTopic(const CTopic& Topic);
		CTopic& operator=(const CTopic& Topic);
		void	Copy(const CTopic& Topic);
		CString	m_Name;
		CString	m_Path;
		int		m_Depth;
		int		m_Refs;
		bool	m_IsItem;
	};

	enum {
		CMD_BEGCHAP,
		CMD_ENDCHAP,
		CMD_TOPIC,
		CMD_ITEM,
		CMD_LINK,
		CMD_SPAN,
		CMD_ENDSPAN,
		CMDS
	};
	enum {
		CMD_PREFIX	= '@'
	};
	static	const LPCSTR	m_CmdToken[CMDS];
	static	const LPCSTR	m_HHHdr;
	static	const LPCSTR	m_HHTocHdr;
	static	const LPCSTR	m_HHFtr;
	static	const LPCSTR	m_HHIdxHdr;

	typedef CArray<CTopic, CTopic&> CTopicArray;
	CTopicArray	m_Topic;
	CString m_TopicPath;

	int		FindTopic(LPCSTR Name) const;
	int		FindTopicIgnoreCase(LPCSTR Name) const;
	bool	FixLinks(LPCSTR TopicPath, int TopicIdx);
	bool	InsertToc(LPCSTR DocPath, LPCSTR TocPath, LPCSTR DocTitle);
	int		GetCmd(LPCSTR Line, CString& Arg);
	bool	UpdateHelpFile(LPCSTR HelpPath) const;
	static	CString	MakeIndent(int Depth, int IndentSize = 3);
	static	void	ReplaceChars(CString& DestStr, LPCSTR FindStr, int ReplChar);
};

inline CDoc2Web::CTopic::CTopic()
{
	m_Depth = 0;
	m_Refs = 0;
	m_IsItem = 0;
}

const LPCSTR CDoc2Web::m_CmdToken[CMDS] = {
	"chapter",
	"endchapter",
	"topic",
	"item",
	"[",	// link
	"{",	// span
	"}"		// endspan
};

const LPCSTR CDoc2Web::m_HHHdr = 
"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n"
"<HTML>\n"
"<HEAD>\n"
"<meta name=\"GENERATOR\" content=\"Microsoft&reg; HTML Help Workshop 4.1\">\n"
"<!-- Sitemap 1.0 -->\n"
"</HEAD>\n"
"<BODY>\n";

const LPCSTR CDoc2Web::m_HHTocHdr = 
"<OBJECT type=\"text/site properties\">\n"
"  <param name=\"Window Styles\" value=\"0x800025\">\n"
"  <param name=\"FrameName\" value=\"right\">\n"
"  <param name=\"comment\" value=\"title:Online Help\">\n"
"  <param name=\"comment\" value=\"base:index.htm\">\n"
"</OBJECT>\n"
"<UL>\n";

const LPCSTR CDoc2Web::m_HHFtr = 
"</UL>\n"
"</BODY>\n"
"</HTML>\n";

const LPCSTR CDoc2Web::m_HHIdxHdr = 
"<OBJECT type=\"text/site properties\">\n"
"  <param name=\"FrameName\" value=\"right\">\n"
"</OBJECT>\n"
"<UL>\n";

inline CDoc2Web::CTopic::CTopic(const CTopic& Topic)
{
	Copy(Topic);
}

inline CDoc2Web::CTopic& CDoc2Web::CTopic::operator=(const CTopic& Topic)
{
	if (&Topic != this)
		Copy(Topic);
	return(*this);
}

void CDoc2Web::CTopic::Copy(const CTopic& Topic)
{
	m_Name = Topic.m_Name;
	m_Path = Topic.m_Path;
	m_Depth = Topic.m_Depth;
	m_Refs = Topic.m_Refs;
	m_IsItem = Topic.m_IsItem; 
}

inline int CDoc2Web::FindTopic(LPCSTR Name) const
{
	int	nTopics = m_Topic.GetSize();
	for (int iTopic = 0; iTopic < nTopics; iTopic++) {
		if (m_Topic[iTopic].m_Name == Name)
			return(iTopic);
	}
	return(-1);
}

inline int CDoc2Web::FindTopicIgnoreCase(LPCSTR Name) const
{
	int	nTopics = m_Topic.GetSize();
	for (int iTopic = 0; iTopic < nTopics; iTopic++) {
		if (!m_Topic[iTopic].m_Name.CompareNoCase(Name))
			return(iTopic);
	}
	return(-1);
}

bool CDoc2Web::UpdateHelpFile(LPCSTR HelpPath) const
{
	CPathStr	HelpRoot(HelpPath);
	HelpRoot.RemoveExtension();
	CStdioFile	fp, fo, fh;
	CString	HHProjPath(HelpRoot + ".hhp");
	if (!fp.Open(HHProjPath, CFile::modeRead)) {
		printf("can't open HTML help project '%s'\n", HHProjPath);
		return(FALSE);
	}
	LPCSTR	TmpPath = "temp.hhp";
	if (!fo.Open(TmpPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't create temp file '%s'\n", TmpPath);
		return(FALSE);
	}
	LPCSTR	HelpIDSuffix = "HelpIDs.h";
	CString	HelpIDPath(HelpRoot + HelpIDSuffix);
	if (!fh.Open(HelpIDPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't create help ID file '%s'\n", HelpIDPath);
		return(FALSE);
	}
	CString	line;
	bool	skip = FALSE;
	while (fp.ReadString(line)) {
		if (skip) {
			if (line.IsEmpty())
				skip = FALSE;
		} else {
			if (line == "[FILES]" || line == "[ALIAS]" || line == "[MAP]") {
				skip = TRUE;
			} else {
				fo.WriteString(line + '\n');
			}
		}
	}
	if (!line.IsEmpty())
		fo.WriteString("\n");
	fo.WriteString("[FILES]\n");
	int	iTopic;
	int	nTopics = m_Topic.GetSize();
	for (iTopic = 0; iTopic < nTopics; iTopic++) {
		const CTopic&	topic = m_Topic[iTopic];
		if (topic.m_IsItem)
			continue;
		fo.WriteString(topic.m_Path + '\n');
	}
	fo.WriteString("\n[ALIAS]\n");
	for (iTopic = 0; iTopic < nTopics; iTopic++) {
		const CTopic&	topic = m_Topic[iTopic];
		if (!HELP_FILE_ITEM_ALIASES && topic.m_IsItem)
			continue;
		CPathStr	path(topic.m_Path);
		CString	sItem;
		int	iItem = path.Find('#');
		if (iItem >= 0)
			sItem = path.Mid(iItem + 1);
		path.RemoveExtension();
		if (iItem >= 0)
			path += '_' + sItem;
		CString	sHID;
		LPCSTR	pSrc = path;
		LPSTR	pDst = sHID.GetBuffer(path.GetLength());
		while (*pSrc) {
			if (*pSrc == '/') {
				*pDst++ = '_';
				pSrc++;
				while (*pSrc && (isdigit(*pSrc) || *pSrc == ' '))
					pSrc++;
			}
			char	c = *pSrc;
			if (!__iscsym(c))
				c = '_';
			*pDst++ = c;
			pSrc++;
		}
		*pDst = 0;
		sHID.ReleaseBuffer();
		sHID.Insert(0, "IDH_");
		fo.WriteString(sHID + " = " + topic.m_Path + '\n');
		line.Format("#define %s %d\n", sHID, iTopic + 1);
		fh.WriteString(line);
	}
	CString	HelpIDFileName(CString(PathFindFileName(HelpRoot)) + HelpIDSuffix);
	fo.WriteString("\n[MAP]\n#include \"" + HelpIDFileName + '\n');
	fp.Close();
	fo.Close();
	if (_unlink(HHProjPath)) {
		printf("can't delete HTML help project '%s'\n", HHProjPath);
		return(FALSE);
	}
	if (rename(TmpPath, HHProjPath)) {
		printf("can't rename '%s' to '%s'\n", TmpPath, HHProjPath);
		return(FALSE);
	}
	return(TRUE);
}

CString CDoc2Web::MakeIndent(int Depth, int IndentSize)
{
	CString	spacer(' ', IndentSize);
	CString	indent;
	for (int iDepth = 0; iDepth < Depth; iDepth++)
		indent += spacer;
	return(indent);
}

int CDoc2Web::GetCmd(LPCSTR Line, CString& Arg)
{
	if (Line[0] == CMD_PREFIX) {
		for (int iCmd = 0; iCmd < CMDS; iCmd++) {
			int	CmdLen = strlen(m_CmdToken[iCmd]);
			if (!strncmp(Line + 1, m_CmdToken[iCmd], CmdLen)) {
				Arg = &Line[CmdLen + 1];
				Arg.TrimLeft();
				Arg.TrimRight();
				return(iCmd);
			}
		}
	}
	return(-1);
}

void CDoc2Web::ReplaceChars(CString& DestStr, LPCSTR FindStr, int ReplChar)
{
	int	pos = 0;
	while ((pos = DestStr.Mid(pos).FindOneOf(FindStr)) >= 0)
		DestStr.SetAt(pos, ReplChar);
}

bool CDoc2Web::FixLinks(LPCSTR Path, int TopicIdx)
{
	CStdioFile	fp, fo;
	LPCSTR	TmpPath = "doc2web.tmp";
	if (!fp.Open(Path, CFile::modeRead)) {
		printf("can't open '%s'\n", Path);
		return(FALSE);
	}
	if (!fo.Open(TmpPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't open '%s'\n", TmpPath);
		return(FALSE);
	}
	CString	Line;
	int	subs = 0;
	while (fp.ReadString(Line)) {
		int	pos = 0;
		while ((pos = Line.Find(CMD_PREFIX, pos)) >= 0) {
			if (pos + 1 < Line.GetLength()) {
				int	cmd = Line[pos + 1];
				switch (cmd) {
				case '[':
					{
						int	end = Line.Find(']', pos + 2);
						if (end < 0 || end - pos < 3) {
							printf("invalid hyperlink in file '%s'\n", Path);
							return(FALSE);
						}
						CString	link = Line.Mid(pos + 2, end - pos - 2);
						CString	disp;
						int	dispidx = link.Find('\\');
						if (dispidx >= 0) {
							disp = link.Mid(dispidx + 1);
							link = link.Left(dispidx);
						} else
							disp = link;
						int iTopic = FindTopicIgnoreCase(link);
						if (iTopic < 0) {
							printf("broken hyperlink '%s' in file '%s'\n", link, Path);
							return(FALSE);
						}
						CTopic&	topic = m_Topic[iTopic];
						Line.Delete(pos, end - pos + 1);
						CString	url;
						if (TopicIdx >= 0) {
							int	depth = m_Topic[TopicIdx].m_Depth;
							for (int iDepth = 0; iDepth < depth; iDepth++)
								url += "../";
							url += topic.m_Path;
							topic.m_Refs++;	// bump reference count
						} else {
							url = "#";
							url += topic.m_Name;
							ReplaceChars(url, "/\\:*?<>|", '-');
							url.Replace(" ", "_");
						}
						CString	s;
						s.Format("<a href=\"%s\">%s</a>", url, disp);
						Line.Insert(pos, s);
						subs++;
						pos = end;
					}
					break;
				case '{':
					{
						int	end = Line.Find('}', pos + 2);
						if (end >= 0) {
							CString SpanClass = Line.Mid(pos + 2, end - pos - 2);
							Line.Delete(pos, end - pos + 1);
							CString	SpanTag = "<span class=\"" + SpanClass + "\">";
							Line.Insert(pos, SpanTag);
							pos += SpanTag.GetLength();
							subs++;
						} else {
							printf("unterminated span in file '%s'\n", Path);
							return(FALSE);
						}
					}
					break;
				case '}':
					{
						Line.Delete(pos, 2);
						Line.Insert(pos, "</span>");
						pos += 7;
						subs++;
					}
					break;
				default:
					printf("invalid command '%c' in file '%s'\n", cmd, Path);
					return(FALSE);
				}
			} else
				pos++;
		}
		fo.WriteString(Line + "\n");
	}
	fp.Close();
	fo.Close();
	if (subs) {
		if (_unlink(Path)) {
			printf("can't delete '%s'\n", Path);
			return(FALSE);
		}
		if (rename(TmpPath, Path)) {
			printf("can't rename '%s' to '%s'\n", TmpPath, Path);
			return(FALSE);
		}
	}
	return(TRUE);
}

bool CDoc2Web::InsertToc(LPCSTR DocPath, LPCSTR TocPath, LPCSTR DocTitle)
{
	CStdioFile	fd, ft, fo;
	LPCSTR	TmpPath = "doc2web.tmp";
	if (!fd.Open(DocPath, CFile::modeRead)) {
		printf("can't open '%s'\n", DocPath);
		return(FALSE);
	}
	if (!ft.Open(TocPath, CFile::modeRead)) {
		printf("can't open '%s'\n", TocPath);
		return(FALSE);
	}
	if (!fo.Open(TmpPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't open '%s'\n", TmpPath);
		return(FALSE);
	}
	CString	Line;
	while (fd.ReadString(Line)) {
		fo.WriteString(Line + "\n");
		if (Line.Find("<body>") >= 0) {
			if (DocTitle != NULL) {
				CString	s;
				s.Format("<h1>%s</h1>\n<h2>Contents</h2>\n\n", DocTitle);
				fo.WriteString(s);
			} else
				fo.WriteString("<h1>Contents</h1>\n\n");
			CString	Tocl;
			while (ft.ReadString(Tocl)) {
				LPCSTR	tag = "<dd><a href=\"";
				int	taglen = strlen(tag);
				int pos = Tocl.Find(tag);
				if (pos >= 0) {
					int slash = Tocl.Find('"', pos + taglen);
					slash -= 4;
					Tocl.Delete(slash, 4);	// remove .htm
					if (slash >= 0) {
						while (slash >= 0 && Tocl[slash] != '/')
							slash--;
						Tocl.Delete(taglen, slash - taglen + 1);
						Tocl.Insert(taglen, "#");
					}
				}
				fo.WriteString(Tocl + "\n");
			}
		}
	}
	fd.Close();
	fo.Close();
	if (_unlink(DocPath)) {
		printf("can't delete '%s'\n", DocPath);
		return(FALSE);
	}
	if (rename(TmpPath, DocPath)) {
		printf("can't rename '%s' to '%s'\n", TmpPath, DocPath);
		return(FALSE);
	}
	return(TRUE);
}

bool CDoc2Web::Process(LPCSTR SrcPath, LPCSTR DstFolder, LPCSTR TocPath,
			 LPCSTR DocPath, LPCSTR DocTitle, LPCSTR HelpPath, int Options)
{
	CStdioFile	fi, fo, fc, fd;
	if (!fi.Open(SrcPath, CFile::modeRead | CFile::shareDenyNone)) {
		printf("can't open %s\n", SrcPath);
		return(FALSE);
	}
	CString	Line;
	int	BlankLines = 0;
	bool	Writing = FALSE;
	if (_chdir(DstFolder)) {
		printf("can't set destination folder\n");
		return(FALSE);
	}
	if (!fc.Open(TocPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't create %s\n", TocPath);
		return(FALSE);
	}
	if (DocPath != NULL && !fd.Open(DocPath, CFile::modeWrite | CFile::modeCreate)) {
		printf("can't create %s\n", DocPath);
		return(FALSE);
	}
	CStdioFile	hhtoc, hhidx;
	if (HelpPath != NULL) {
		CPathStr	HelpRoot(HelpPath);
		HelpRoot.RemoveExtension();
		CString	HHTocPath(HelpRoot + ".hhc");
		if (!hhtoc.Open(HHTocPath, CFile::modeWrite | CFile::modeCreate)) {
			printf("can't create HTML help TOC %s\n", HHTocPath);
			return(FALSE);
		}
		hhtoc.WriteString(m_HHHdr);
		hhtoc.WriteString(m_HHTocHdr);
		CString	HHIdxPath(HelpRoot + ".hhk");
		if (!hhidx.Open(HHIdxPath, CFile::modeWrite | CFile::modeCreate)) {
			printf("can't create HTML help index %s\n", HHIdxPath);
			return(FALSE);
		}
		hhidx.WriteString(m_HHHdr);
		hhidx.WriteString(m_HHIdxHdr);
	}
	CDWordArray		ChapNum;
	CStringArray	ChapName;
	int	Depth = 0;
	ChapNum.SetSize(1);
	ChapName.SetSize(0);
	CString	s;
	if (TocPath != NULL)
		fc.WriteString("<dl>\n");
	if (DocPath != NULL) {
		s.Format("<html><head><title>%s</title></head><body>\n", 
			DocTitle != NULL ? DocTitle : "");
		fd.WriteString(s);
	}
	while (fi.ReadString(Line)) {
		Line.TrimRight();
		Line.TrimLeft();
		if (Line.IsEmpty()) {
			BlankLines++;
			if (Writing) {
				fo.WriteString("\n");
				if (DocPath != NULL)
					fd.WriteString("\n");
			}
		} else {
			bool	IsCmd;
			const int nAnchorTagLen = 3;	
			int	iAnchorName = -1;
			if (Line[0] == CMD_PREFIX || (iAnchorName = Line.Find("@{#")) >= 0) {
				IsCmd = TRUE;
				CString	Arg;
				int	Cmd;
				if (iAnchorName >= 0) {
					Cmd = CMD_ITEM;
					int	iAnchorNameEnd = Line.Find('}', iAnchorName);
					if (iAnchorNameEnd < 0) {
						printf("unterminated anchor name\n");
						return FALSE;
					}
					int	iAnchorNameLen = iAnchorNameEnd - (iAnchorName + nAnchorTagLen);
					Arg = Line.Mid(iAnchorName + nAnchorTagLen, iAnchorNameLen);
					Line.Delete(iAnchorName, iAnchorNameLen + nAnchorTagLen + 1);
				} else {
					Cmd = GetCmd(Line, Arg);
				}
				switch (Cmd) {
				case CMD_BEGCHAP:
					{
						CString	Filename = Arg;
						if (Options & NO_SPACES)
							Filename.Replace(' ', '_');
						_mkdir(Filename);
						if (_chdir(Filename)) {
							printf("can't change folder to '%s'\n", Filename);
							return(FALSE);
						}
						if (TocPath != NULL) {
							if (Depth > 0)
								fc.WriteString("<dd><dl>\n");	// Firefox requires <dd>
							s.Format("<dt>%s\n", Arg);
							fc.WriteString(s);
						}
						Depth++;
						ChapNum.SetSize(Depth + 1);
						ChapName.SetSize(Depth);
						ChapName[Depth - 1] = Filename;
						if (DocPath != NULL) {
							s.Format("<h%d>%s</h%d>\n", Depth, Arg, Depth);
							fd.WriteString(s);
						}
						if (HelpPath != NULL) {
							CString	indent(MakeIndent(Depth));
							hhtoc.WriteString(
								indent + "<LI><OBJECT type=\"text/sitemap\">\n" +
								indent + "   <param name = \"Name\" value=\"" + Arg + "\">\n" +
								indent + "</OBJECT>\n" +
								indent + "<UL>\n"
							);
						}
					}
					break;
				case CMD_ENDCHAP:
					{
						if (!Depth) {
							printf("chapter nesting error\n");
							return(FALSE);
						}
						if (HelpPath != NULL) {
							CString	indent(MakeIndent(Depth));
							hhtoc.WriteString(indent + "</UL>\n");
						}
						Depth--;
						if (_chdir("..")) {
							printf("can't change folder\n");
							return(FALSE);
						}
						ChapNum.SetSize(Depth + 1);
						ChapName.SetSize(Depth);
						if (TocPath != NULL && Depth > 0)
							fc.WriteString("</dl>\n");
					}
					break;
				case CMD_TOPIC:
				case CMD_ITEM:	// item within topic
					{
						bool	IsItem = (Cmd == CMD_ITEM);
						if (Writing && !IsItem) {
							fo.WriteString("</body>\n");
							fo.Close();
						}
						int	iPos = Arg.Find('\\');
						CString	Title;
						if (iPos >= 0) {	// if topic is aliased
							Title = Arg.Mid(iPos + 1);	// after backslash is alias
							Arg = Arg.Left(iPos);	// before backslash is title
						} else
							Title = Arg;
						CString	Anchor, Filename;
						if (Options & NUM_PREFIX) {
							CString NumStr;
							NumStr.Format("%02d", ++ChapNum[Depth]);
							Anchor = NumStr + " " + Arg;
						} else
							Anchor = Arg;
						ReplaceChars(Anchor, "/\\:*?<>|", '-');
						if (Options & NO_SPACES)
							Anchor.Replace(' ', '_');
						Filename = Anchor + ".htm";
						if (!IsItem) {
							if (!fo.Open(Filename, CFile::modeWrite | CFile::modeCreate)) {
								printf("can't create %s\n", Filename);
								return(FALSE);
							}
							Writing = TRUE;
							printf("[%s]\n", Filename);
							fo.WriteString(CString("<title>") + Title + "</title>\n<body>\n\n");
							fo.WriteString(CString("<h3>") + Title + "</h3>\n");
							if (TocPath != NULL) {
								CString	Url;
								for (int iDepth = 0; iDepth < Depth; iDepth++) {
									Url += ChapName[iDepth] + "/";
								}
								Url += Filename;
								Url.Replace(" ", "%20");
								s.Format("<dd><a href=\"%s\">%s</a>\n", Url, Title);
								fc.WriteString(s);
							}
						}
						int iTopic = FindTopic(Arg);
						if (iTopic >= 0) {
							printf("duplicate topic %s\n", Arg);
							return(FALSE);
						}
						CString	Path;
						if (IsItem) {
							CString	ItemAnchor(Arg);
							ReplaceChars(ItemAnchor, "/\\:*?<>|", '-');
							ItemAnchor.Replace("\'", "");
							if (Options & NO_SPACES)
								ItemAnchor.Replace(' ', '_');
							if (iAnchorName >= 0) {
								Line.Insert(iAnchorName, CString("<a name=\"") 
									+ ItemAnchor + "\">" + Title + "</a>");
								fo.WriteString(Line + '\n');
							} else {
								fo.WriteString(CString("<a name=\"") 
									+ ItemAnchor + "\"><p><b>" + Title + "</b></a>");
							}
							Path = m_TopicPath + '#' + ItemAnchor;
						} else {
							for (int j = 0; j < Depth; j++)
								Path += ChapName[j] + "/";
							Path += Filename;
							m_TopicPath = Path;
						}
						CTopic	topic;
						topic.m_Name = Arg;
						topic.m_Path = Path;
						topic.m_Depth = Depth;
						topic.m_IsItem = IsItem;
						m_Topic.Add(topic);
						if (DocPath != NULL) {
							int	nHeadNum = Depth + 1;
							if (IsItem)
								nHeadNum++;
							if (iAnchorName >= 0) {
								fd.WriteString(Line + '\n');
							} else {
								s.Format("<a name=\"%s\"><h%d>%s</h%d></a>\n", 
									Anchor, nHeadNum, Title, nHeadNum);
								fd.WriteString(s);
							}
						}
						if (HelpPath != NULL) {
							CString	indent(MakeIndent(Depth + 1));
							if (!IsItem) {
								hhtoc.WriteString(
									indent + "<LI><OBJECT type=\"text/sitemap\">\n" +
									indent + "   <param name = \"Name\" value=\"" + Title + "\">\n" +
									indent + "   <param name = \"Local\" value=\"" + Path + "\">\n" +
									indent + "</OBJECT>\n"
								);
							}
							hhidx.WriteString(
								"   <LI><OBJECT type=\"text/sitemap\">\n"
								"      <param name = \"Name\" value=\"" + Arg + "\">\n"
								"      <param name = \"Local\" value=\"" + Path + "\">\n"
								"   </OBJECT>\n"
							);
						}
					}
					break;
				case CMD_LINK:
				case CMD_SPAN:
					IsCmd = FALSE;
					break;
				default:
					printf("unknown command %d\n", Cmd);
					return(FALSE);
				}
			} else
				IsCmd = FALSE;
			if (!IsCmd) {
				if (!Writing) {
					printf("text outside a topic\n");
					return(FALSE);
				}
				s.Empty();
				if (Line[0] != '<') {
					if (BlankLines > 0)
						s = "<p>";
					else
						s = "<br>";
				}
				if (!s.IsEmpty()) {
					fo.WriteString(s);
					if (DocPath != NULL)
						fd.WriteString(s);
				}
				BlankLines = 0;
				fo.WriteString(Line + "\n");
				if (DocPath != NULL)
					fd.WriteString(Line + "\n");
			}
		}
	}
	if (Writing) {
		fo.WriteString("</body>\n");
		fo.Close();
	}
	if (DocPath != NULL) {
		fd.WriteString("</body></html>\n");
		fd.Close();
	}
	if (TocPath != NULL) {
		fc.WriteString("</dl>\n\n");
		fc.Close();
	}
	int	nTopics = m_Topic.GetSize();
	for (int iTopic = 0; iTopic < nTopics; iTopic++) {
		const CTopic&	topic = m_Topic[iTopic];
		if (!topic.m_IsItem) {
			if (!FixLinks(topic.m_Path, iTopic))
				return(FALSE);
		}
	}
	if (DocPath != NULL) {
		if (!FixLinks(DocPath, -1))
			return(FALSE);
		if (TocPath != NULL) {
			if (!InsertToc(DocPath, TocPath, DocTitle))
				return(FALSE);
		}
	}
	if (HelpPath != NULL) {
		hhtoc.WriteString(m_HHFtr);
		hhidx.WriteString(m_HHFtr);
		if (!UpdateHelpFile(HelpPath))
			return(FALSE);
	}
	if (Options & LOG_REFS) {
		CStdioFile	log("Doc2WebLog.txt", CFile::modeCreate | CFile::modeWrite);
		CString	line;
		for (int iTopic = 0; iTopic < nTopics; iTopic++) {
			const CTopic&	topic = m_Topic[iTopic];
			line.Format("%s\t%d\n", topic.m_Name, topic.m_Refs);
			log.WriteString(line);
		}
	}
	return(TRUE);
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		cerr << _T("Fatal Error: MFC initialization failed") << endl;
		nRetCode = 1;
	}
	else
	{
		int	Options = 0;
		int	iArg = 1;
		while (argc > 1 && argv[iArg][0] == '/') {
			if (!strcmp(&argv[iArg][1], "numprefix"))
				Options |= CDoc2Web::NUM_PREFIX;
			if (!strcmp(&argv[iArg][1], "nospaces"))
				Options |= CDoc2Web::NO_SPACES;
			if (!strcmp(&argv[iArg][1], "logrefs"))
				Options |= CDoc2Web::LOG_REFS;
			iArg++;
			argc--;
		}
		CDoc2Web	d2w;
		if (argc >= 3) {
			TRY {
				if (!d2w.Process(
					argv[iArg + 0],
					argv[iArg + 1],
					argc > 3 ? argv[iArg + 2] : NULL,
					argc > 4 ? argv[iArg + 3] : NULL,
					argc > 5 ? argv[iArg + 4] : NULL,
					argc > 6 ? argv[iArg + 5] : NULL,
					Options))
					nRetCode = 1;
			}
			CATCH (CFileException, e) {
				e->ReportError();
			}
			END_CATCH
		} else
			printf("doc2web [/numprefix] [/nospaces] [/logrefs] src_path dst_folder [contents_fname] [doc_path] [doc_title] [help_file_path]\n");
	}

	return nRetCode;
}
