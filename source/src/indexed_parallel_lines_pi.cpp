/******************************************************************************
 * Project:  OpenCPN
 * Purpose:  Indexed Parallel Navigation plugin
 *
 ***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************
 */

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/dir.h>
#include <wx/fileconf.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/glcanvas.h>
#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/timer.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <map>
#include <string>
#include <vector>

#include "indexed_parallel_lines_pi.h"
#include "version.h"
#include "tpicons.h"
#include "tinyxml.h"

#ifdef __WXMSW__
#include <windows.h>
#endif
#include <GL/gl.h>

#ifndef DECL_EXP
#ifdef __WXMSW__
#define DECL_EXP __declspec(dllexport)
#else
#define DECL_EXP
#endif
#endif

static const double EARTH_RADIUS_NM = 3440.065;
static const wxString CONFIG_GROUP = _T("/PlugIns/IndexedParallelLines");

// The drawn parallel line is this many times the length of its reference
// leg (extended equally beyond each end), so it reads clearly on the chart.
// User-configurable via the Settings dialog; this is only the default.
static const double DEFAULT_LINE_LENGTH_FACTOR = 1.25;

// Hit tolerance, in screen pixels, for clicking an existing indexed line.
// User-configurable via the Settings dialog; this is only the default.
static const double DEFAULT_HIT_TOLERANCE_PX = 8.0;

// GPX 1.1 namespace used for the plugin's own extension elements embedded
// in an exported <rte>, so exported files remain valid, readable GPX.
static const char *GPX_EXTENSION_NS = "https://opencpn.org/indexed_parallel_lines_pi/gpx-extension/v1";

wxString *g_PrivateDataDir;
wxString *g_pData;
wxString *g_SData_Locn;

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin *create_pi(void *ppimgr)
{
    return new indexed_parallel_lines_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin *p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//
//    indexed_parallel_lines PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

indexed_parallel_lines_pi::indexed_parallel_lines_pi(void *ppimgr)
    :opencpn_plugin_118(ppimgr)
{
    wxFileName fn(*GetpPrivateApplicationDataLocation(), wxEmptyString);
    fn.AppendDir(_T("plugins"));
    fn.AppendDir(_T("indexed_parallel_lines_pi"));
    if (!wxDir::Exists(fn.GetPath()))
        wxFileName::Mkdir(fn.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    g_PrivateDataDir = new wxString(fn.GetPath());

    g_pData = new wxString(*g_PrivateDataDir);
    g_pData->Append(wxFileName::GetPathSeparator());
    g_pData->Append(_T("data"));
    if (!wxDir::Exists(*g_pData))
        wxMkdir(*g_pData);

    m_ptpicons = new tpicons();
    m_pDialog = NULL;
    m_pListCtrl = NULL;
    m_pRouteFilterChoice = NULL;
    m_pPickStatusLabel = NULL;
    m_filterRouteGUID = wxEmptyString;
    m_pListRefreshTimer = NULL;
    m_cursor_lat = 0.0;
    m_cursor_lon = 0.0;
    m_pickState = PICK_NONE;
    m_pendingIsPerpendicular = false;
    m_repickInProgress = false;
    m_haveViewport = false;
    m_selectedLineIndex = -1;
    m_hoveredLineIndex = -1;
    m_hoverScreenPos = wxPoint(0, 0);
    m_lineLengthFactor = DEFAULT_LINE_LENGTH_FACTOR;
    m_hitToleranceLevel = DEFAULT_HIT_TOLERANCE_PX;
    m_lineColor = wxColour(255, 0, 0);
    m_selectedLineColor = wxColour(255, 255, 0);
}

indexed_parallel_lines_pi::~indexed_parallel_lines_pi()
{
    delete g_PrivateDataDir;
    delete g_pData;
    delete m_ptpicons;
}

int indexed_parallel_lines_pi::Init(void)
{
    AddLocaleCatalog(PLUGIN_CATALOG_NAME);

    LoadSettings();
    LoadIndexedLines();

#ifdef PLUGIN_USE_SVG
    m_indexed_parallel_lines_button_id = InsertPlugInToolSVG(
        _("Indexed Parallel Navigation"), m_ptpicons->m_s_indexed_parallel_lines_grey_pi,
        m_ptpicons->m_s_indexed_parallel_lines_pi, m_ptpicons->m_s_indexed_parallel_lines_toggled_pi,
        wxITEM_CHECK, _("Indexed Parallel Navigation"), wxS(""), NULL, indexed_parallel_lines_POSITION, 0,
        this);
#else
    m_indexed_parallel_lines_button_id = InsertPlugInTool(
        _("Indexed Parallel Navigation"), &m_ptpicons->m_bm_indexed_parallel_lines_grey_pi,
        &m_ptpicons->m_bm_indexed_parallel_lines_pi, wxITEM_CHECK, _("Indexed Parallel Navigation"),
        wxS(""), NULL, indexed_parallel_lines_POSITION, 0, this);
#endif

    return (WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL |
            WANTS_CURSOR_LATLON | WANTS_MOUSE_EVENTS |
            WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK);
}

bool indexed_parallel_lines_pi::DeInit(void)
{
    SaveIndexedLines();
    SaveSettings();

    if (m_pDialog) {
        m_pDialog->Destroy();
        m_pDialog = NULL;
    }
    return true;
}

int indexed_parallel_lines_pi::GetAPIVersionMajor()
{
    return OCPN_API_VERSION_MAJOR;
}

int indexed_parallel_lines_pi::GetAPIVersionMinor()
{
    return OCPN_API_VERSION_MINOR;
}

int indexed_parallel_lines_pi::GetPlugInVersionMajor()
{
    return PLUGIN_VERSION_MAJOR;
}

int indexed_parallel_lines_pi::GetPlugInVersionMinor()
{
    return PLUGIN_VERSION_MINOR;
}

int indexed_parallel_lines_pi::GetPlugInVersionPatch()
{
    return PLUGIN_VERSION_PATCH;
}

int indexed_parallel_lines_pi::GetPlugInVersionPost()
{
    return PLUGIN_VERSION_TWEAK;
}

wxBitmap *indexed_parallel_lines_pi::GetPlugInBitmap()
{
    return &m_ptpicons->m_bm_indexed_parallel_lines_pi;
}

wxString indexed_parallel_lines_pi::GetCommonName()
{
    return _T(PLUGIN_COMMON_NAME);
}

wxString indexed_parallel_lines_pi::GetShortDescription()
{
    return _(PLUGIN_SHORT_DESCRIPTION);
}

wxString indexed_parallel_lines_pi::GetLongDescription()
{
    return _(PLUGIN_LONG_DESCRIPTION);
}

int indexed_parallel_lines_pi::GetToolbarToolCount(void)
{
    return 1;
}

void indexed_parallel_lines_pi::OnToolbarToolCallback(int id)
{
    if (m_pDialog) {
        m_pDialog->Destroy();
        m_pDialog = NULL;
        m_pListCtrl = NULL;
        m_pRouteFilterChoice = NULL;
        m_pPickStatusLabel = NULL;
        m_pickState = PICK_NONE;
        SetToolbarItemState(m_indexed_parallel_lines_button_id, false);
        return;
    }

    m_pDialog = new wxDialog(GetOCPNCanvasWindow(), wxID_ANY,
                              _("Indexed Parallel Navigation"), wxDefaultPosition,
                              wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(m_pDialog, wxID_ANY, _("Route:")), 0,
                      wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_pRouteFilterChoice = new wxChoice(m_pDialog, wxID_ANY);
    m_pRouteFilterChoice->Bind(wxEVT_CHOICE, &indexed_parallel_lines_pi::OnRouteFilterChoice, this);
    filterSizer->Add(m_pRouteFilterChoice, 1, wxALL | wxEXPAND, 5);
    sizer->Add(filterSizer, 0, wxEXPAND);

    m_pListCtrl = new wxListCtrl(m_pDialog, wxID_ANY, wxDefaultPosition,
                                  wxSize(720, 260), wxLC_REPORT | wxLC_SINGLE_SEL);
    m_pListCtrl->EnableCheckBoxes(true);
    m_pListCtrl->InsertColumn(0, _("Route"), wxLIST_FORMAT_LEFT, 120);
    m_pListCtrl->InsertColumn(1, _("Name"), wxLIST_FORMAT_LEFT, 100);
    m_pListCtrl->InsertColumn(2, _("Course"), wxLIST_FORMAT_LEFT, 80);
    m_pListCtrl->InsertColumn(3, _("Offset"), wxLIST_FORMAT_LEFT, 90);
    m_pListCtrl->InsertColumn(4, _("Side"), wxLIST_FORMAT_LEFT, 90);
    m_pListCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &indexed_parallel_lines_pi::OnListItemSelected, this);
    m_pListCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &indexed_parallel_lines_pi::OnListItemDeselected, this);
    m_pListCtrl->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &indexed_parallel_lines_pi::OnListItemRightClick, this);
    m_pListCtrl->Bind(wxEVT_LIST_ITEM_CHECKED, &indexed_parallel_lines_pi::OnListItemChecked, this);
    m_pListCtrl->Bind(wxEVT_LIST_ITEM_UNCHECKED, &indexed_parallel_lines_pi::OnListItemUnchecked, this);
    sizer->Add(m_pListCtrl, 1, wxALL | wxEXPAND, 10);

    m_pPickStatusLabel = new wxStaticText(m_pDialog, wxID_ANY, wxEmptyString);
    m_pPickStatusLabel->SetForegroundColour(*wxBLUE);
    m_pPickStatusLabel->Hide();
    sizer->Add(m_pPickStatusLabel, 0, wxALIGN_CENTER | wxBOTTOM, 5);

    wxWrapSizer *buttonSizer = new wxWrapSizer(wxHORIZONTAL);

    wxButton *newButton = new wxButton(m_pDialog, wxID_ANY, _("New Indexed Line"));
    newButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnNewIndexedLineButton, this);
    buttonSizer->Add(newButton, 0, wxALL, 5);

    wxButton *newPerpButton = new wxButton(m_pDialog, wxID_ANY, _("New Perpendicular Line"));
    newPerpButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnNewPerpendicularLineButton, this);
    buttonSizer->Add(newPerpButton, 0, wxALL, 5);

    wxButton *deleteButton = new wxButton(m_pDialog, wxID_ANY, _("Delete Selected"));
    deleteButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnDeleteIndexedLineButton, this);
    buttonSizer->Add(deleteButton, 0, wxALL, 5);

    wxButton *changeDistButton = new wxButton(m_pDialog, wxID_ANY, _("Edit Distance..."));
    changeDistButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnChangeDistanceButton, this);
    buttonSizer->Add(changeDistButton, 0, wxALL, 5);

    wxButton *repickDistButton = new wxButton(m_pDialog, wxID_ANY, _("Repick Distance on Chart"));
    repickDistButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnRepickDistanceButton, this);
    buttonSizer->Add(repickDistButton, 0, wxALL, 5);

    wxButton *renameButton = new wxButton(m_pDialog, wxID_ANY, _("Rename..."));
    renameButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnRenameButton, this);
    buttonSizer->Add(renameButton, 0, wxALL, 5);

    sizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxEXPAND);

    wxWrapSizer *buttonSizer2 = new wxWrapSizer(wxHORIZONTAL);

    wxButton *settingsButton = new wxButton(m_pDialog, wxID_ANY, _("Settings..."));
    settingsButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnSettingsButton, this);
    buttonSizer2->Add(settingsButton, 0, wxALL, 5);

    wxButton *exportButton = new wxButton(m_pDialog, wxID_ANY, _("Export"));
    exportButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnExportButton, this);
    buttonSizer2->Add(exportButton, 0, wxALL, 5);

    wxButton *exportAllButton = new wxButton(m_pDialog, wxID_ANY, _("Export All"));
    exportAllButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnExportAllButton, this);
    buttonSizer2->Add(exportAllButton, 0, wxALL, 5);

    wxButton *importButton = new wxButton(m_pDialog, wxID_ANY, _("Import..."));
    importButton->Bind(wxEVT_BUTTON, &indexed_parallel_lines_pi::OnImportButton, this);
    buttonSizer2->Add(importButton, 0, wxALL, 5);

    sizer->Add(buttonSizer2, 0, wxALIGN_CENTER | wxEXPAND);

    m_pDialog->SetSizerAndFit(sizer);
    m_pDialog->SetMinSize(m_pDialog->GetSize());
    m_pDialog->CentreOnScreen();

    m_pDialog->Bind(wxEVT_CLOSE_WINDOW, &indexed_parallel_lines_pi::OnDialogClose, this);
    m_pDialog->Bind(wxEVT_CHAR_HOOK, &indexed_parallel_lines_pi::OnCharHook, this);

    RefreshList();

    m_pDialog->Show();
    m_pDialog->Raise();
    m_pDialog->SetFocus();
    SetToolbarItemState(m_indexed_parallel_lines_button_id, true);

    // Keep the Course column live while the manager window is open, since
    // the underlying route legs can move (be dragged/edited) at any time.
    if (!m_pListRefreshTimer) {
        m_pListRefreshTimer = new wxTimer(m_pDialog);
        m_pDialog->Bind(wxEVT_TIMER, &indexed_parallel_lines_pi::OnListRefreshTimer, this);
    }
    m_pListRefreshTimer->Start(500);
}

void indexed_parallel_lines_pi::OnNewIndexedLineButton(wxCommandEvent &event)
{
    if (m_pickState != PICK_NONE) CancelPick();
    m_pending = IndexedLine();
    m_pendingIsPerpendicular = false;
    m_pickState = PICK_LEG;
    UpdatePickStatusLabel();
}

void indexed_parallel_lines_pi::OnNewPerpendicularLineButton(wxCommandEvent &event)
{
    if (m_pickState != PICK_NONE) CancelPick();
    m_pending = IndexedLine();
    m_pendingIsPerpendicular = true;
    m_pickState = PICK_LEG;
    UpdatePickStatusLabel();
}

void indexed_parallel_lines_pi::OnRepickDistanceButton(wxCommandEvent &event)
{
    if (m_pickState != PICK_NONE) CancelPick();

    if (m_selectedLineIndex >= 0 && m_selectedLineIndex < (int)m_indexLines.size()) {
        BeginRepick((size_t)m_selectedLineIndex);
        return;
    }

    // No line selected yet - let the user click one on the chart to choose
    // which indexed line to repick, instead of requiring a prior selection.
    m_pickState = PICK_REPICK_TARGET;
    UpdatePickStatusLabel();
}

void indexed_parallel_lines_pi::BeginRepick(size_t lineIndex)
{
    // Reuse the same reference leg, but let the user click the chart again to
    // set a new distance: remove the old line and jump straight into the
    // offset-pick step (skipping leg selection, since it's already known).
    // The removed line is kept in m_repickBackup so it can be restored if
    // the repick is cancelled instead of confirmed.
    const IndexedLine &line = m_indexLines[lineIndex];
    m_repickBackup = line;
    m_repickInProgress = true;

    m_pending = IndexedLine();
    m_pending.name = line.name;
    m_pending.routeGUID = line.routeGUID;
    m_pending.wp0GUID = line.wp0GUID;
    m_pending.wp1GUID = line.wp1GUID;
    m_pendingIsPerpendicular = line.isPerpendicular;

    m_indexLines.erase(m_indexLines.begin() + lineIndex);
    m_selectedLineIndex = -1;
    m_pickState = PICK_OFFSET;
    UpdatePickStatusLabel();

    RefreshList();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::OnDeleteIndexedLineButton(wxCommandEvent &event)
{
    if (!m_pListCtrl) return;
    long row = m_pListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row == -1 || row >= (long)m_visibleRowToLineIndex.size()) return;
    size_t lineIdx = m_visibleRowToLineIndex[row];

    wxString lineName = m_indexLines[lineIdx].name;
    if (wxMessageBox(wxString::Format(_("Delete indexed line '%s'?"), lineName.c_str()),
                      _("Confirm Delete"), wxYES_NO | wxICON_QUESTION, m_pDialog) != wxYES)
        return;

    m_indexLines.erase(m_indexLines.begin() + lineIdx);
    if (m_selectedLineIndex == (int)lineIdx)
        m_selectedLineIndex = -1;
    else if (m_selectedLineIndex > (int)lineIdx)
        m_selectedLineIndex--;

    RefreshList();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::OnListRefreshTimer(wxTimerEvent &event)
{
    RefreshList();
}

void indexed_parallel_lines_pi::OnListItemSelected(wxListEvent &event)
{
    long row = event.GetIndex();
    m_selectedLineIndex = (row >= 0 && row < (long)m_visibleRowToLineIndex.size())
                               ? (int)m_visibleRowToLineIndex[row]
                               : -1;
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::OnListItemDeselected(wxListEvent &event)
{
    long row = event.GetIndex();
    if (row >= 0 && row < (long)m_visibleRowToLineIndex.size() &&
        (int)m_visibleRowToLineIndex[row] == m_selectedLineIndex) {
        m_selectedLineIndex = -1;
    }
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::OnListItemRightClick(wxListEvent &event)
{
    long row = event.GetIndex();
    if (row >= 0 && row < (long)m_visibleRowToLineIndex.size()) {
        m_selectedLineIndex = (int)m_visibleRowToLineIndex[row];
        RefreshList();
        if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
    }

    wxMenu menu;
    int idEdit = wxWindow::NewControlId();
    int idRepick = wxWindow::NewControlId();
    int idRename = wxWindow::NewControlId();
    int idDelete = wxWindow::NewControlId();
    menu.Append(idEdit, _("Edit Distance..."));
    menu.Append(idRepick, _("Repick Distance on Chart"));
    menu.Append(idRename, _("Rename..."));
    menu.AppendSeparator();
    menu.Append(idDelete, _("Delete Selected"));
    menu.Bind(wxEVT_MENU, &indexed_parallel_lines_pi::OnChangeDistanceButton, this, idEdit);
    menu.Bind(wxEVT_MENU, &indexed_parallel_lines_pi::OnRepickDistanceButton, this, idRepick);
    menu.Bind(wxEVT_MENU, &indexed_parallel_lines_pi::OnRenameButton, this, idRename);
    menu.Bind(wxEVT_MENU, &indexed_parallel_lines_pi::OnDeleteIndexedLineButton, this, idDelete);
    m_pListCtrl->PopupMenu(&menu);
}

void indexed_parallel_lines_pi::OnChangeDistanceButton(wxCommandEvent &event)
{
    if (m_pickState != PICK_NONE) CancelPick();

    if (m_selectedLineIndex >= 0 && m_selectedLineIndex < (int)m_indexLines.size()) {
        PromptChangeDistanceForIndex((size_t)m_selectedLineIndex);
        return;
    }

    // No line selected yet - let the user click one on the chart to choose
    // which indexed line to edit, instead of requiring a prior selection.
    m_pickState = PICK_EDIT_TARGET;
    UpdatePickStatusLabel();
}

void indexed_parallel_lines_pi::OnRenameButton(wxCommandEvent &event)
{
    PromptRename();
}

void indexed_parallel_lines_pi::OnListItemChecked(wxListEvent &event)
{
    long row = event.GetIndex();
    if (row < 0 || row >= (long)m_visibleRowToLineIndex.size()) return;
    m_indexLines[m_visibleRowToLineIndex[row]].visible = true;
    SaveIndexedLines();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::OnListItemUnchecked(wxListEvent &event)
{
    long row = event.GetIndex();
    if (row < 0 || row >= (long)m_visibleRowToLineIndex.size()) return;
    m_indexLines[m_visibleRowToLineIndex[row]].visible = false;
    SaveIndexedLines();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::PromptChangeDistanceForIndex(size_t lineIndex)
{
    if (lineIndex >= m_indexLines.size()) return;

    IndexedLine &line = m_indexLines[lineIndex];

    if (line.isPerpendicular) {
        // A perpendicular line has two independent distances - where along
        // the leg it crosses, and how far it extends to each side - so both
        // need their own field rather than a single "offset" prompt.
        wxDialog dlg(m_pDialog, wxID_ANY, _("Edit Distance"), wxDefaultPosition,
                     wxDefaultSize, wxDEFAULT_DIALOG_STYLE);

        wxBoxSizer *outer = new wxBoxSizer(wxVERTICAL);
        wxFlexGridSizer *grid = new wxFlexGridSizer(2, 8, 10);
        grid->AddGrowableCol(1);

        grid->Add(new wxStaticText(&dlg, wxID_ANY,
                                    _("Along-track position from leg start, nm\n(may be negative, or beyond the leg's end):")),
                  0, wxALIGN_CENTER_VERTICAL);
        wxTextCtrl *alongCtrl = new wxTextCtrl(
            &dlg, wxID_ANY, wxString::Format(_T("%.2f"), line.alongTrackNM));
        grid->Add(alongCtrl, 1, wxEXPAND);

        grid->Add(new wxStaticText(&dlg, wxID_ANY,
                                    _("Half-length - extends this far to each side, nm:")),
                  0, wxALIGN_CENTER_VERTICAL);
        wxTextCtrl *lengthCtrl = new wxTextCtrl(
            &dlg, wxID_ANY, wxString::Format(_T("%.2f"), line.offsetNM));
        grid->Add(lengthCtrl, 1, wxEXPAND);

        outer->Add(grid, 1, wxALL | wxEXPAND, 10);
        outer->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxALL, 5);
        dlg.SetSizerAndFit(outer);

        if (dlg.ShowModal() != wxID_OK) return;

        double alongTrack, halfLength;
        bool alongOk = alongCtrl->GetValue().ToDouble(&alongTrack);
        bool lengthOk = lengthCtrl->GetValue().ToDouble(&halfLength) && halfLength >= 0.0;
        if (!alongOk || !lengthOk) {
            wxMessageBox(_("Enter a valid along-track position and a valid, "
                            "non-negative half-length, both in nautical miles."),
                         _("Indexed Parallel Navigation"), wxOK | wxICON_ERROR);
            return;
        }

        line.alongTrackNM = alongTrack;
        line.offsetNM = halfLength;
    } else {
        wxString input = wxGetTextFromUser(
            _("New offset distance from the reference leg, in nautical miles:"),
            _("Edit Distance"), wxString::Format(_T("%.2f"), line.offsetNM),
            m_pDialog);
        if (input.IsEmpty()) return;

        double newOffset;
        if (!input.ToDouble(&newOffset) || newOffset < 0.0) {
            wxMessageBox(_("Enter a valid, non-negative distance in nautical miles."),
                         _("Indexed Parallel Navigation"), wxOK | wxICON_ERROR);
            return;
        }

        line.offsetNM = newOffset;
    }

    SaveIndexedLines();
    RefreshList();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::PromptRename(void)
{
    if (m_selectedLineIndex < 0 || m_selectedLineIndex >= (int)m_indexLines.size()) {
        wxMessageBox(_("Select an indexed line first."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION);
        return;
    }

    IndexedLine &line = m_indexLines[m_selectedLineIndex];
    wxString input = wxGetTextFromUser(_("New name for this indexed line:"),
                                        _("Rename"), line.name, m_pDialog);
    if (input.IsEmpty()) return;

    line.name = input;
    SaveIndexedLines();
    RefreshList();
}

void indexed_parallel_lines_pi::CancelPick(void)
{
    if (m_repickInProgress) {
        // Cancelling a repick must not lose the line it was repicking -
        // restore it exactly as it was before the repick started.
        m_indexLines.push_back(m_repickBackup);
        m_repickInProgress = false;
        RefreshList();
    }
    m_pickState = PICK_NONE;
    m_pending = IndexedLine();
    UpdatePickStatusLabel();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

void indexed_parallel_lines_pi::UpdatePickStatusLabel(void)
{
    if (!m_pPickStatusLabel) return;

    if (m_pickState == PICK_NONE) {
        m_pPickStatusLabel->SetLabel(wxEmptyString);
        m_pPickStatusLabel->Hide();
    } else {
        wxString text;
        if (m_pickState == PICK_LEG) {
            text = _("Picking reference leg on chart... (Esc to cancel)");
        } else if (m_pickState == PICK_REPICK_TARGET) {
            text = _("Click the indexed line to repick... (Esc to cancel)");
        } else if (m_pickState == PICK_EDIT_TARGET) {
            text = _("Click the indexed line to edit its distance... (Esc to cancel)");
        } else if (m_pendingIsPerpendicular) {
            text = _("Picking crossing point and length on chart... (Esc to cancel)");
        } else {
            text = _("Picking offset distance on chart... (Esc to cancel)");
        }
        m_pPickStatusLabel->SetLabel(text);
        m_pPickStatusLabel->Show();
    }
    if (m_pDialog) m_pDialog->Layout();
}

void indexed_parallel_lines_pi::OnCharHook(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_ESCAPE && m_pickState != PICK_NONE) {
        CancelPick();
        return;
    }
    event.Skip();
}

void indexed_parallel_lines_pi::OnSettingsButton(wxCommandEvent &event)
{
    wxDialog dlg(m_pDialog, wxID_ANY, _("Settings"), wxDefaultPosition,
                 wxDefaultSize, wxDEFAULT_DIALOG_STYLE);

    wxBoxSizer *outer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, 8, 10);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Parallel line length factor:")),
              0, wxALIGN_CENTER_VERTICAL);
    wxTextCtrl *lengthCtrl = new wxTextCtrl(
        &dlg, wxID_ANY, wxString::Format(_T("%.2f"), m_lineLengthFactor));
    grid->Add(lengthCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Chart click hit tolerance (px):")),
              0, wxALIGN_CENTER_VERTICAL);
    wxTextCtrl *toleranceCtrl = new wxTextCtrl(
        &dlg, wxID_ANY, wxString::Format(_T("%.1f"), m_hitToleranceLevel));
    grid->Add(toleranceCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Line color:")),
              0, wxALIGN_CENTER_VERTICAL);
    wxColourPickerCtrl *lineColorCtrl =
        new wxColourPickerCtrl(&dlg, wxID_ANY, m_lineColor);
    grid->Add(lineColorCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Selected line color:")),
              0, wxALIGN_CENTER_VERTICAL);
    wxColourPickerCtrl *selColorCtrl =
        new wxColourPickerCtrl(&dlg, wxID_ANY, m_selectedLineColor);
    grid->Add(selColorCtrl, 1, wxEXPAND);

    outer->Add(grid, 1, wxALL | wxEXPAND, 10);
    outer->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTER | wxALL, 5);
    dlg.SetSizerAndFit(outer);

    if (dlg.ShowModal() != wxID_OK) return;

    double lengthFactor;
    if (lengthCtrl->GetValue().ToDouble(&lengthFactor) && lengthFactor >= 1.0)
        m_lineLengthFactor = lengthFactor;

    double tolerance;
    if (toleranceCtrl->GetValue().ToDouble(&tolerance) && tolerance > 0.0)
        m_hitToleranceLevel = tolerance;

    m_lineColor = lineColorCtrl->GetColour();
    m_selectedLineColor = selColorCtrl->GetColour();

    SaveSettings();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
}

wxArrayString indexed_parallel_lines_pi::GetRoutesWithLines(void)
{
    wxArrayString result;
    for (size_t i = 0; i < m_indexLines.size(); i++) {
        if (result.Index(m_indexLines[i].routeGUID) == wxNOT_FOUND)
            result.Add(m_indexLines[i].routeGUID);
    }
    return result;
}

void indexed_parallel_lines_pi::OnExportButton(wxCommandEvent &event)
{
    wxArrayString routeGUIDs;
    if (!m_filterRouteGUID.IsEmpty())
        routeGUIDs.Add(m_filterRouteGUID);
    else
        routeGUIDs = GetRoutesWithLines();

    if (routeGUIDs.IsEmpty()) {
        wxMessageBox(_("No indexed lines to export for the current route filter."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION, m_pDialog);
        return;
    }

    wxFileDialog saveDlg(m_pDialog, _("Export Indexed Lines"), wxEmptyString,
                          _T("indexed_lines.gpx"), _("GPX files (*.gpx)|*.gpx"),
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() != wxID_OK) return;

    ExportToGpx(saveDlg.GetPath(), routeGUIDs);
}

void indexed_parallel_lines_pi::OnExportAllButton(wxCommandEvent &event)
{
    wxArrayString routeGUIDs = GetRoutesWithLines();
    if (routeGUIDs.IsEmpty()) {
        wxMessageBox(_("No indexed lines to export."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION, m_pDialog);
        return;
    }

    wxFileDialog saveDlg(m_pDialog, _("Export All Indexed Lines"), wxEmptyString,
                          _T("indexed_lines_all.gpx"), _("GPX files (*.gpx)|*.gpx"),
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() != wxID_OK) return;

    ExportToGpx(saveDlg.GetPath(), routeGUIDs);
}

void indexed_parallel_lines_pi::OnImportButton(wxCommandEvent &event)
{
    wxFileDialog openDlg(m_pDialog, _("Import Indexed Lines"), wxEmptyString,
                          wxEmptyString, _("GPX files (*.gpx)|*.gpx"), wxFD_OPEN);
    if (openDlg.ShowModal() != wxID_OK) return;

    ImportFromGpx(openDlg.GetPath());
}

void indexed_parallel_lines_pi::SortIndexLines(void)
{
    if (m_indexLines.size() < 2) return;

    struct SortEntry {
        wxString routeName;
        int legIndex;
        size_t origIndex;
    };

    std::vector<SortEntry> entries;
    entries.reserve(m_indexLines.size());
    for (size_t i = 0; i < m_indexLines.size(); i++) {
        const IndexedLine &line = m_indexLines[i];
        SortEntry e;
        e.origIndex = i;
        e.legIndex = INT_MAX;
        e.routeName = _("~ (route not found)");

        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(line.routeGUID);
        if (route) {
            e.routeName = route->m_NameString;
            if (route->pWaypointList) {
                // Match either waypoint order: a whole-route reversal keeps
                // the two waypoints adjacent but swaps which comes first.
                int legIdx = 0;
                wxPlugin_WaypointListNode *node = route->pWaypointList->GetFirst();
                while (node && node->GetNext()) {
                    const wxString &a = node->GetData()->m_GUID;
                    const wxString &b = node->GetNext()->GetData()->m_GUID;
                    if ((a == line.wp0GUID && b == line.wp1GUID) ||
                        (a == line.wp1GUID && b == line.wp0GUID)) {
                        e.legIndex = legIdx;
                        break;
                    }
                    legIdx++;
                    node = node->GetNext();
                }
            }
        }
        entries.push_back(e);
    }

    std::stable_sort(entries.begin(), entries.end(),
                      [](const SortEntry &a, const SortEntry &b) {
                          int cmp = a.routeName.CmpNoCase(b.routeName);
                          if (cmp != 0) return cmp < 0;
                          return a.legIndex < b.legIndex;
                      });

    std::vector<IndexedLine> sorted;
    sorted.reserve(m_indexLines.size());
    int newSelected = -1;
    for (size_t i = 0; i < entries.size(); i++) {
        sorted.push_back(m_indexLines[entries[i].origIndex]);
        if ((int)entries[i].origIndex == m_selectedLineIndex) newSelected = (int)i;
    }
    m_indexLines = sorted;
    m_selectedLineIndex = newSelected;
}

void indexed_parallel_lines_pi::RefreshRouteFilterChoice(void)
{
    if (!m_pRouteFilterChoice) return;

    wxArrayString guids = GetRouteGUIDArray();

    // Only rebuild if the set/order of routes actually changed, so the
    // dropdown doesn't disturb the user's current pick on every tick.
    bool changed = (m_filterChoiceGUIDs.GetCount() != guids.GetCount() + 1);
    if (!changed) {
        for (size_t i = 0; i < guids.GetCount(); i++) {
            if (m_filterChoiceGUIDs[i + 1] != guids[i]) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return;

    m_filterChoiceGUIDs.Clear();
    m_filterChoiceGUIDs.Add(wxEmptyString);

    wxArrayString labels;
    labels.Add(_("All Routes"));
    for (size_t i = 0; i < guids.GetCount(); i++) {
        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(guids[i]);
        labels.Add(route ? route->m_NameString : guids[i]);
        m_filterChoiceGUIDs.Add(guids[i]);
    }

    m_pRouteFilterChoice->Clear();
    m_pRouteFilterChoice->Append(labels);

    int sel = m_filterChoiceGUIDs.Index(m_filterRouteGUID);
    if (sel == wxNOT_FOUND) {
        sel = 0;
        m_filterRouteGUID = wxEmptyString;
    }
    m_pRouteFilterChoice->SetSelection(sel);
}

void indexed_parallel_lines_pi::OnRouteFilterChoice(wxCommandEvent &event)
{
    int sel = m_pRouteFilterChoice->GetSelection();
    if (sel < 0 || sel >= (int)m_filterChoiceGUIDs.GetCount()) return;
    m_filterRouteGUID = m_filterChoiceGUIDs[sel];
    RefreshList();
}

void indexed_parallel_lines_pi::SetItemIfChanged(long row, int col, const wxString &text)
{
    if (m_pListCtrl->GetItemText(row, col) != text)
        m_pListCtrl->SetItem(row, col, text);
}

void indexed_parallel_lines_pi::RefreshList(void)
{
    if (!m_pListCtrl) return;

    SortIndexLines();
    RefreshRouteFilterChoice();

    m_visibleRowToLineIndex.clear();
    for (size_t i = 0; i < m_indexLines.size(); i++) {
        if (!m_filterRouteGUID.IsEmpty() && m_indexLines[i].routeGUID != m_filterRouteGUID)
            continue;
        m_visibleRowToLineIndex.push_back(i);
    }

    // Only delete/reinsert rows when the count actually changed (add/delete/
    // filter change); otherwise update columns in place via SetItem, which
    // does not disturb the user's current selection the way
    // DeleteAllItems()+InsertItem() does.
    if (m_pListCtrl->GetItemCount() != (long)m_visibleRowToLineIndex.size()) {
        m_pListCtrl->DeleteAllItems();
        for (size_t row = 0; row < m_visibleRowToLineIndex.size(); row++)
            m_pListCtrl->InsertItem((long)row, wxEmptyString);
    }

    // Fetch each distinct route at most once per refresh, instead of once per
    // line (twice, counting GetLiveLeg's own lookup) - with several lines on
    // the same route this cuts repeated GetRoute_Plugin() calls significantly
    // on every ~500ms tick.
    std::map<wxString, std::unique_ptr<PlugIn_Route>> routeCache;

    for (size_t row = 0; row < m_visibleRowToLineIndex.size(); row++) {
        size_t i = m_visibleRowToLineIndex[row];
        const IndexedLine &line = m_indexLines[i];
        long idx = (long)row;

        std::unique_ptr<PlugIn_Route> &cachedRoute = routeCache[line.routeGUID];
        if (!cachedRoute) cachedRoute = GetRoute_Plugin(line.routeGUID);
        PlugIn_Route *route = cachedRoute.get();

        SetItemIfChanged(idx, 0, route ? route->m_NameString : _("route not found"));
        SetItemIfChanged(idx, 1, line.name);

        double lat0, lon0, lat1, lon1, course, legDist;
        bool reversedInRoute = false;
        bool legFound = GetLiveLegFromRoute(route, line.wp0GUID, line.wp1GUID,
                                             &lat0, &lon0, &lat1, &lon1, &course,
                                             &legDist, &reversedInRoute);
        if (legFound) {
            // If the route's traversal order for this leg has flipped (the
            // whole route was reversed), display the course and side as
            // sailed in the new direction, not the leg's fixed original
            // definition - the drawn line itself does not move.
            double displayCourse = reversedInRoute ? fmod(course + 180.0, 360.0) : course;
            SetItemIfChanged(idx, 2, wxString::Format(_T("%03.0f\u00B0"), displayCourse));
        } else {
            SetItemIfChanged(idx, 2, _("route leg not found"));
        }
        SetItemIfChanged(idx, 3, wxString::Format(_T("%.2f nm"), line.offsetNM));
        if (line.isPerpendicular) {
            wxString posLabel = _("Perpendicular");
            if (legFound) {
                bool ahead = line.alongTrackNM > legDist;
                bool astern = line.alongTrackNM < 0.0;
                if (reversedInRoute) std::swap(ahead, astern);
                if (ahead) posLabel = _("Ahead");
                else if (astern) posLabel = _("Astern");
            }
            SetItemIfChanged(idx, 4, posLabel);
        } else {
            bool displaySide = reversedInRoute ? !line.starboardSide : line.starboardSide;
            SetItemIfChanged(idx, 4, displaySide ? _("Starboard") : _("Port"));
        }

        bool wantSelected = ((int)i == m_selectedLineIndex);
        bool isSelected = (m_pListCtrl->GetItemState(idx, wxLIST_STATE_SELECTED) &
                            wxLIST_STATE_SELECTED) != 0;
        if (wantSelected != isSelected) {
            m_pListCtrl->SetItemState(idx, wantSelected ? wxLIST_STATE_SELECTED : 0,
                                       wxLIST_STATE_SELECTED);
        }

        if (m_pListCtrl->IsItemChecked(idx) != line.visible)
            m_pListCtrl->CheckItem(idx, line.visible);
    }
}

void indexed_parallel_lines_pi::SetCursorLatLon(double lat, double lon)
{
    m_cursor_lat = lat;
    m_cursor_lon = lon;
}

bool indexed_parallel_lines_pi::MouseEventHook(wxMouseEvent &event)
{
    // Selecting an existing indexed line directly on the chart, independent
    // of the new-line pick flow below.
    if (m_pickState == PICK_NONE) {
        if (event.LeftDown()) {
            size_t idx;
            bool hit = FindLineNear(m_cursor_lat, m_cursor_lon, &idx);
            m_selectedLineIndex = hit ? (int)idx : -1;
            RefreshList();
            if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
            return hit;  // only consume the click if it actually hit a line
        }

        if (event.Moving()) {
            size_t idx;
            bool hit = FindLineNear(m_cursor_lat, m_cursor_lon, &idx);
            int newHovered = hit ? (int)idx : -1;
            wxPoint newPos = event.GetPosition();
            if (newHovered != m_hoveredLineIndex ||
                (newHovered >= 0 && newPos != m_hoverScreenPos)) {
                m_hoveredLineIndex = newHovered;
                m_hoverScreenPos = newPos;
                if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
            }
        }
        return false;
    }

    // A pick is in progress - suppress the hover info box so it doesn't
    // fight with the pick-preview line.
    m_hoveredLineIndex = -1;

    if (m_pickState == PICK_OFFSET && (event.Moving() || event.Dragging())) {
        if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
        return false;
    }

    if (!event.LeftDown()) return false;

    if (m_pickState == PICK_REPICK_TARGET) {
        size_t idx;
        if (!FindLineNear(m_cursor_lat, m_cursor_lon, &idx)) {
            wxMessageBox(_("No indexed line found near that point"),
                         _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION);
            m_pickState = PICK_NONE;
            UpdatePickStatusLabel();
            return true;
        }
        BeginRepick(idx);
        return true;
    }

    if (m_pickState == PICK_EDIT_TARGET) {
        size_t idx;
        if (!FindLineNear(m_cursor_lat, m_cursor_lon, &idx)) {
            wxMessageBox(_("No indexed line found near that point"),
                         _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION);
            m_pickState = PICK_NONE;
            UpdatePickStatusLabel();
            return true;
        }
        m_pickState = PICK_NONE;
        UpdatePickStatusLabel();
        m_selectedLineIndex = (int)idx;
        PromptChangeDistanceForIndex(idx);
        RefreshList();
        if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
        return true;
    }

    if (m_pickState == PICK_LEG) {
        wxString routeGUID, wp0GUID, wp1GUID;
        if (!FindNearestLeg(m_cursor_lat, m_cursor_lon, &routeGUID, &wp0GUID,
                             &wp1GUID)) {
            wxMessageBox(_("No route leg found near that point"),
                         _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION);
            m_pickState = PICK_NONE;
            UpdatePickStatusLabel();
            return true;
        }
        m_pending.routeGUID = routeGUID;
        m_pending.wp0GUID = wp0GUID;
        m_pending.wp1GUID = wp1GUID;
        m_pickState = PICK_OFFSET;
        UpdatePickStatusLabel();
        return true;
    }

    if (m_pickState == PICK_OFFSET) {
        double lat0, lon0, lat1, lon1, course, legDist;
        if (!GetLiveLeg(m_pending.routeGUID, m_pending.wp0GUID,
                         m_pending.wp1GUID, &lat0, &lon0, &lat1, &lon1,
                         &course, &legDist, NULL)) {
            // The reference leg vanished (route/waypoint deleted) while
            // picking - cancel via CancelPick() so a repick-in-progress
            // restores its original line instead of losing it.
            CancelPick();
            return true;
        }

        m_pending.isPerpendicular = m_pendingIsPerpendicular;

        if (m_pendingIsPerpendicular) {
            double crossTrackNM, alongTrackNM;
            TrackOffsets(lat0, lon0, lat1, lon1, m_cursor_lat, m_cursor_lon,
                         &crossTrackNM, &alongTrackNM);
            // Not clamped to [0, legDist]: the crossing point may fall
            // beyond either end of the leg (ahead of wp1 or astern of wp0).

            m_pending.alongTrackNM = alongTrackNM;
            m_pending.offsetNM = fabs(crossTrackNM);  // half-length, nm
            m_pending.starboardSide = true;            // unused for this type
        } else {
            double signedDistNM;
            CrossTrackDistance(lat0, lon0, lat1, lon1, m_cursor_lat, m_cursor_lon,
                                &signedDistNM);

            m_pending.offsetNM = fabs(signedDistNM);
            m_pending.starboardSide = (signedDistNM > 0.0);
        }

        if (m_pending.name.IsEmpty())
            m_pending.name = wxString::Format(_("Index %d"), (int)m_indexLines.size() + 1);

        m_indexLines.push_back(m_pending);
        m_pickState = PICK_NONE;
        m_repickInProgress = false;  // committed - nothing left to restore
        UpdatePickStatusLabel();

        RefreshList();
        if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());
        return true;
    }

    return false;
}

bool indexed_parallel_lines_pi::FindNearestLeg(double lat, double lon,
                                                wxString *outRouteGUID,
                                                wxString *outWp0GUID,
                                                wxString *outWp1GUID)
{
    wxArrayString guids = GetRouteGUIDArray();
    if (guids.IsEmpty()) return false;

    bool found = false;
    double bestAbsXt = -1.0;

    for (size_t i = 0; i < guids.GetCount(); i++) {
        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(guids[i]);
        if (!route || !route->pWaypointList || route->pWaypointList->GetCount() < 2)
            continue;

        wxPlugin_WaypointListNode *node = route->pWaypointList->GetFirst();
        while (node && node->GetNext()) {
            PlugIn_Waypoint *wp0 = node->GetData();
            PlugIn_Waypoint *wp1 = node->GetNext()->GetData();

            double legBrg, legDist;
            DistanceBearingMercator_Plugin(wp0->m_lat, wp0->m_lon, wp1->m_lat,
                                            wp1->m_lon, &legBrg, &legDist);

            double toClickBrg, toClickDist;
            DistanceBearingMercator_Plugin(wp0->m_lat, wp0->m_lon, lat, lon,
                                            &toClickBrg, &toClickDist);

            double d13 = toClickDist / EARTH_RADIUS_NM;
            double xt = asin(sin(d13) * sin((toClickBrg - legBrg) * M_PI / 180.0)) *
                        EARTH_RADIUS_NM;
            double at = acos(cos(d13) / cos(xt / EARTH_RADIUS_NM)) * EARTH_RADIUS_NM;

            double tolerance = legDist * 0.15 + 0.05;  // small slack past endpoints
            if (at >= -tolerance && at <= legDist + tolerance) {
                if (bestAbsXt < 0.0 || fabs(xt) < bestAbsXt) {
                    bestAbsXt = fabs(xt);
                    *outRouteGUID = guids[i];
                    *outWp0GUID = wp0->m_GUID;
                    *outWp1GUID = wp1->m_GUID;
                    found = true;
                }
            }
            node = node->GetNext();
        }
    }

    return found;
}

bool indexed_parallel_lines_pi::GetLiveLeg(const wxString &routeGUID,
                                            const wxString &wp0GUID,
                                            const wxString &wp1GUID,
                                            double *lat0, double *lon0,
                                            double *lat1, double *lon1,
                                            double *course, double *legDistNM,
                                            bool *reversedInRoute)
{
    std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(routeGUID);
    if (!route) return false;
    return GetLiveLegFromRoute(route.get(), wp0GUID, wp1GUID, lat0, lon0, lat1,
                                lon1, course, legDistNM, reversedInRoute);
}

bool indexed_parallel_lines_pi::GetLiveLegFromRoute(PlugIn_Route *route,
                                                     const wxString &wp0GUID,
                                                     const wxString &wp1GUID,
                                                     double *lat0, double *lon0,
                                                     double *lat1, double *lon1,
                                                     double *course,
                                                     double *legDistNM,
                                                     bool *reversedInRoute)
{
    if (!route || !route->pWaypointList) return false;

    // The two waypoints defining this leg are matched by identity (GUID),
    // not by their current position in the route: if the user reverses the
    // whole route, the waypoints stay adjacent but their list order swaps.
    // Match either order so the leg (and any indexed line based on it)
    // doesn't vanish, and report the swap via *reversedInRoute so callers
    // can flip the displayed course/side to match the new travel direction.
    wxPlugin_WaypointListNode *node = route->pWaypointList->GetFirst();
    while (node && node->GetNext()) {
        PlugIn_Waypoint *a = node->GetData();
        PlugIn_Waypoint *b = node->GetNext()->GetData();

        bool forward = (a->m_GUID == wp0GUID && b->m_GUID == wp1GUID);
        bool reversed = (a->m_GUID == wp1GUID && b->m_GUID == wp0GUID);
        if (forward || reversed) {
            PlugIn_Waypoint *wp0 = forward ? a : b;
            PlugIn_Waypoint *wp1 = forward ? b : a;
            *lat0 = wp0->m_lat;
            *lon0 = wp0->m_lon;
            *lat1 = wp1->m_lat;
            *lon1 = wp1->m_lon;
            if (reversedInRoute) *reversedInRoute = reversed;
            // DistanceBearingMercator_Plugin(A, B, ...) actually returns the
            // bearing from B back to A (its own core implementation binds
            // its first point pair to the "destination" side of the
            // formula), so add 180 degrees to get the true forward bearing
            // from wp0 to wp1.
            double rawBrg;
            DistanceBearingMercator_Plugin(*lat0, *lon0, *lat1, *lon1, &rawBrg,
                                            legDistNM);
            *course = fmod(rawBrg + 180.0 + 360.0, 360.0);
            return true;
        }
        node = node->GetNext();
    }

    return false;
}

void indexed_parallel_lines_pi::CrossTrackDistance(double lat0, double lon0,
                                                     double lat1, double lon1,
                                                     double lat, double lon,
                                                     double *signedDistNM)
{
    double legBrg, legDist;
    DistanceBearingMercator_Plugin(lat0, lon0, lat1, lon1, &legBrg, &legDist);

    double toPtBrg, toPtDist;
    DistanceBearingMercator_Plugin(lat0, lon0, lat, lon, &toPtBrg, &toPtDist);

    double d13 = toPtDist / EARTH_RADIUS_NM;
    *signedDistNM =
        asin(sin(d13) * sin((toPtBrg - legBrg) * M_PI / 180.0)) * EARTH_RADIUS_NM;
}

void indexed_parallel_lines_pi::TrackOffsets(double lat0, double lon0,
                                              double lat1, double lon1,
                                              double lat, double lon,
                                              double *crossTrackNM,
                                              double *alongTrackNM)
{
    double legBrg, legDist;
    DistanceBearingMercator_Plugin(lat0, lon0, lat1, lon1, &legBrg, &legDist);

    double toPtBrg, toPtDist;
    DistanceBearingMercator_Plugin(lat0, lon0, lat, lon, &toPtBrg, &toPtDist);

    double d13 = toPtDist / EARTH_RADIUS_NM;
    double bearingDiffRad = (toPtBrg - legBrg) * M_PI / 180.0;
    double xt = asin(sin(d13) * sin(bearingDiffRad)) * EARTH_RADIUS_NM;
    // acos() only ever returns a magnitude; determine whether the point
    // projects ahead of wp0 (toward wp1) or behind it (astern) from the
    // bearing difference, so a point behind wp0 gets a negative along-track
    // distance instead of being folded onto the positive side.
    double atMag = acos(cos(d13) / cos(xt / EARTH_RADIUS_NM)) * EARTH_RADIUS_NM;
    double sign = (cos(bearingDiffRad) >= 0.0) ? 1.0 : -1.0;

    *crossTrackNM = xt;
    *alongTrackNM = sign * atMag;
}

bool indexed_parallel_lines_pi::GetOffsetEndpoints(const IndexedLine &line,
                                                     double *lat0, double *lon0,
                                                     double *lat1, double *lon1)
{
    double refLat0, refLon0, refLat1, refLon1, course, legDist;
    if (!GetLiveLeg(line.routeGUID, line.wp0GUID, line.wp1GUID, &refLat0,
                     &refLon0, &refLat1, &refLon1, &course, &legDist, NULL))
        return false;

    if (line.isPerpendicular) {
        // Find the point along the leg (from wp0) where the line crosses,
        // then extend perpendicular to the leg's course to each side by
        // offsetNM (the line's half-length). Not clamped to [0, legDist]:
        // the crossing point may fall beyond either end of the leg (ahead
        // of wp1 or astern of wp0).
        double crossLat, crossLon;
        if (line.alongTrackNM >= 0.0) {
            PositionBearingDistanceMercator_Plugin(refLat0, refLon0, course,
                                                    line.alongTrackNM, &crossLat,
                                                    &crossLon);
        } else {
            PositionBearingDistanceMercator_Plugin(refLat0, refLon0, course + 180.0,
                                                    -line.alongTrackNM, &crossLat,
                                                    &crossLon);
        }

        PositionBearingDistanceMercator_Plugin(crossLat, crossLon, course + 90.0,
                                                line.offsetNM, lat0, lon0);
        PositionBearingDistanceMercator_Plugin(crossLat, crossLon, course - 90.0,
                                                line.offsetNM, lat1, lon1);
        return true;
    }

    // Extend each end so the drawn parallel line is m_lineLengthFactor times
    // the reference leg's length, centered on the leg.
    double extra = legDist * (m_lineLengthFactor - 1.0) / 2.0;

    double extLat0, extLon0, extLat1, extLon1;
    PositionBearingDistanceMercator_Plugin(refLat0, refLon0, course + 180.0,
                                            extra, &extLat0, &extLon0);
    PositionBearingDistanceMercator_Plugin(refLat1, refLon1, course, extra,
                                            &extLat1, &extLon1);

    // Starboard (right of travel wp0->wp1) is course + 90 in compass terms.
    double perpBrg = course + (line.starboardSide ? 90.0 : -90.0);
    PositionBearingDistanceMercator_Plugin(extLat0, extLon0, perpBrg,
                                            line.offsetNM, lat0, lon0);
    PositionBearingDistanceMercator_Plugin(extLat1, extLon1, perpBrg,
                                            line.offsetNM, lat1, lon1);
    return true;
}

bool indexed_parallel_lines_pi::BuildPreviewLine(IndexedLine *preview)
{
    *preview = m_pending;
    preview->isPerpendicular = m_pendingIsPerpendicular;

    double lat0, lon0, lat1, lon1, course, legDist;
    if (!GetLiveLeg(m_pending.routeGUID, m_pending.wp0GUID, m_pending.wp1GUID,
                     &lat0, &lon0, &lat1, &lon1, &course, &legDist, NULL))
        return false;

    if (m_pendingIsPerpendicular) {
        double crossTrackNM, alongTrackNM;
        TrackOffsets(lat0, lon0, lat1, lon1, m_cursor_lat, m_cursor_lon,
                     &crossTrackNM, &alongTrackNM);
        preview->alongTrackNM = alongTrackNM;
        preview->offsetNM = fabs(crossTrackNM);
    } else {
        double signedDistNM;
        CrossTrackDistance(lat0, lon0, lat1, lon1, m_cursor_lat, m_cursor_lon,
                            &signedDistNM);
        preview->offsetNM = fabs(signedDistNM);
        preview->starboardSide = (signedDistNM > 0.0);
    }

    preview->name = _("Preview");
    return true;
}

static double PointToSegmentPixelDistance(const wxPoint &p, const wxPoint &a,
                                           const wxPoint &b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double lenSq = dx * dx + dy * dy;
    double t = 0.0;
    if (lenSq > 0.0) {
        t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }
    double projX = a.x + t * dx, projY = a.y + t * dy;
    double ddx = p.x - projX, ddy = p.y - projY;
    return sqrt(ddx * ddx + ddy * ddy);
}

void indexed_parallel_lines_pi::BuildHoverInfoLines(const IndexedLine &line,
                                                      PlugIn_Route *route,
                                                      wxArrayString *outLines)
{
    outLines->Clear();
    outLines->Add(line.name);
    outLines->Add(wxString::Format(_("Route: %s"),
                                    route ? route->m_NameString.c_str()
                                          : _("route not found").c_str()));

    double lat0, lon0, lat1, lon1, course, legDist;
    bool reversedInRoute = false;
    bool legFound = GetLiveLegFromRoute(route, line.wp0GUID, line.wp1GUID, &lat0,
                                         &lon0, &lat1, &lon1, &course, &legDist,
                                         &reversedInRoute);
    if (legFound) {
        double displayCourse = reversedInRoute ? fmod(course + 180.0, 360.0) : course;
        outLines->Add(wxString::Format(_T("%s: %03.0f°"), _("Course").c_str(), displayCourse));
    }

    outLines->Add(wxString::Format(_("Offset: %.2f nm"), line.offsetNM));

    if (line.isPerpendicular) {
        wxString posLabel = _("Perpendicular");
        if (legFound) {
            bool ahead = line.alongTrackNM > legDist;
            bool astern = line.alongTrackNM < 0.0;
            if (reversedInRoute) std::swap(ahead, astern);
            if (ahead) posLabel = _("Ahead");
            else if (astern) posLabel = _("Astern");
        }
        outLines->Add(posLabel);
    } else {
        bool displaySide = reversedInRoute ? !line.starboardSide : line.starboardSide;
        outLines->Add(displaySide ? _("Starboard") : _("Port"));
    }
}

void indexed_parallel_lines_pi::DrawHoverBoxGL(const wxArrayString &infoLines,
                                                 const wxPoint &origin)
{
    wxMemoryDC measureDC;
    int lineHeight = measureDC.GetCharHeight() + 2;
    int boxWidth = 0;
    for (size_t li = 0; li < infoLines.GetCount(); li++) {
        wxCoord w, h;
        measureDC.GetTextExtent(infoLines[li], &w, &h);
        if (w > boxWidth) boxWidth = w;
    }
    boxWidth += 12;
    int boxHeight = (int)infoLines.GetCount() * lineHeight + 8;
    if (boxWidth <= 0 || boxHeight <= 0) return;

    // OpenGL has no built-in text drawing, so the box (background + text) is
    // rendered into an offscreen bitmap via wxDC, then blitted as pixels -
    // this keeps font handling identical to the non-GL overlay's hover box.
    wxBitmap boxBmp(boxWidth, boxHeight, 24);
    {
        wxMemoryDC dc(boxBmp);
        dc.SetBackground(wxBrush(wxColour(255, 255, 225)));
        dc.Clear();
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(0, 0, boxWidth, boxHeight);
        dc.SetTextForeground(*wxBLACK);
        for (size_t li = 0; li < infoLines.GetCount(); li++)
            dc.DrawText(infoLines[li], 6, 4 + (int)li * lineHeight);
        dc.SelectObject(wxNullBitmap);
    }

    wxImage img = boxBmp.ConvertToImage();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // A negative y zoom flips the image vertically as it's drawn, since
    // wxImage stores rows top-first while glDrawPixels otherwise raster-scans
    // upward from the raster position.
    glPixelZoom(1.0f, -1.0f);
    glRasterPos2i(origin.x, origin.y);
    glDrawPixels(boxWidth, boxHeight, GL_RGB, GL_UNSIGNED_BYTE, img.GetData());
    glPixelZoom(1.0f, 1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

bool indexed_parallel_lines_pi::IsLineVisible(const IndexedLine &line)
{
    if (!line.visible) return false;

    std::unique_ptr<PlugIn_Route_Ex> route = GetRouteEx_Plugin(line.routeGUID);
    return route && route->m_isVisible;
}

bool indexed_parallel_lines_pi::FindLineNear(double lat, double lon,
                                              size_t *outIndex)
{
    if (!m_haveViewport) return false;

    wxPoint clickPx;
    GetCanvasPixLL(&m_lastViewport, &clickPx, lat, lon);

    bool found = false;
    double bestDist = m_hitToleranceLevel;

    for (size_t i = 0; i < m_indexLines.size(); i++) {
        if (!IsLineVisible(m_indexLines[i])) continue;

        double lat0, lon0, lat1, lon1;
        if (!GetOffsetEndpoints(m_indexLines[i], &lat0, &lon0, &lat1, &lon1))
            continue;

        wxPoint p0, p1;
        GetCanvasPixLL(&m_lastViewport, &p0, lat0, lon0);
        GetCanvasPixLL(&m_lastViewport, &p1, lat1, lon1);

        double dist = PointToSegmentPixelDistance(clickPx, p0, p1);
        if (dist <= bestDist) {
            bestDist = dist;
            *outIndex = i;
            found = true;
        }
    }

    return found;
}

bool indexed_parallel_lines_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    m_lastViewport = *vp;
    m_haveViewport = true;

    for (size_t i = 0; i < m_indexLines.size(); i++) {
        const IndexedLine &line = m_indexLines[i];
        if (!IsLineVisible(line)) continue;

        double lat0, lon0, lat1, lon1;
        if (!GetOffsetEndpoints(line, &lat0, &lon0, &lat1, &lon1)) continue;

        bool selected = ((int)i == m_selectedLineIndex);
        const wxColour &drawColor = selected ? m_selectedLineColor : m_lineColor;
        dc.SetPen(wxPen(drawColor, selected ? 3 : 2));
        dc.SetTextForeground(drawColor);

        wxPoint p0, p1;
        GetCanvasPixLL(vp, &p0, lat0, lon0);
        GetCanvasPixLL(vp, &p1, lat1, lon1);

        dc.DrawLine(p0, p1);
        wxString label = wxString::Format(_T("%s (%.2f nm)"), line.name.c_str(),
                                           line.offsetNM);
        dc.DrawText(label, (p0.x + p1.x) / 2, (p0.y + p1.y) / 2);
    }

    if (m_pickState == PICK_OFFSET) {
        IndexedLine preview;
        double lat0, lon0, lat1, lon1;
        if (BuildPreviewLine(&preview) &&
            GetOffsetEndpoints(preview, &lat0, &lon0, &lat1, &lon1)) {
            wxPoint p0, p1;
            GetCanvasPixLL(vp, &p0, lat0, lon0);
            GetCanvasPixLL(vp, &p1, lat1, lon1);

            dc.SetPen(wxPen(*wxBLUE, 2, wxPENSTYLE_SHORT_DASH));
            dc.DrawLine(p0, p1);
        }
    }

    if (m_pickState == PICK_NONE && m_hoveredLineIndex >= 0 &&
        m_hoveredLineIndex < (int)m_indexLines.size()) {
        const IndexedLine &line = m_indexLines[m_hoveredLineIndex];
        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(line.routeGUID);
        wxArrayString infoLines;
        BuildHoverInfoLines(line, route.get(), &infoLines);

        int lineHeight = dc.GetCharHeight() + 2;
        int boxWidth = 0;
        for (size_t li = 0; li < infoLines.GetCount(); li++) {
            wxCoord w, h;
            dc.GetTextExtent(infoLines[li], &w, &h);
            if (w > boxWidth) boxWidth = w;
        }
        boxWidth += 12;
        int boxHeight = (int)infoLines.GetCount() * lineHeight + 8;
        wxPoint boxOrigin(m_hoverScreenPos.x + 16, m_hoverScreenPos.y + 16);

        dc.SetPen(wxPen(*wxBLACK, 1));
        dc.SetBrush(wxBrush(wxColour(255, 255, 225)));
        dc.DrawRectangle(boxOrigin.x, boxOrigin.y, boxWidth, boxHeight);

        dc.SetTextForeground(*wxBLACK);
        for (size_t li = 0; li < infoLines.GetCount(); li++)
            dc.DrawText(infoLines[li], boxOrigin.x + 6,
                        boxOrigin.y + 4 + (int)li * lineHeight);
    }

    return true;
}

bool indexed_parallel_lines_pi::RenderGLOverlay(wxGLContext *pcontext,
                                                 PlugIn_ViewPort *vp)
{
    m_lastViewport = *vp;
    m_haveViewport = true;

    for (size_t i = 0; i < m_indexLines.size(); i++) {
        const IndexedLine &line = m_indexLines[i];
        if (!IsLineVisible(line)) continue;

        double lat0, lon0, lat1, lon1;
        if (!GetOffsetEndpoints(line, &lat0, &lon0, &lat1, &lon1)) continue;

        bool selected = ((int)i == m_selectedLineIndex);
        const wxColour &drawColor = selected ? m_selectedLineColor : m_lineColor;
        glColor3ub(drawColor.Red(), drawColor.Green(), drawColor.Blue());
        glLineWidth(selected ? 3.0 : 2.0);

        wxPoint p0, p1;
        GetCanvasPixLL(vp, &p0, lat0, lon0);
        GetCanvasPixLL(vp, &p1, lat1, lon1);

        glBegin(GL_LINES);
        glVertex2i(p0.x, p0.y);
        glVertex2i(p1.x, p1.y);
        glEnd();
    }

    if (m_pickState == PICK_OFFSET) {
        IndexedLine preview;
        double lat0, lon0, lat1, lon1;
        if (BuildPreviewLine(&preview) &&
            GetOffsetEndpoints(preview, &lat0, &lon0, &lat1, &lon1)) {
            wxPoint p0, p1;
            GetCanvasPixLL(vp, &p0, lat0, lon0);
            GetCanvasPixLL(vp, &p1, lat1, lon1);

            glColor3ub(0, 0, 255);
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x00FF);
            glBegin(GL_LINES);
            glVertex2i(p0.x, p0.y);
            glVertex2i(p1.x, p1.y);
            glEnd();
            glDisable(GL_LINE_STIPPLE);
        }
    }

    if (m_pickState == PICK_NONE && m_hoveredLineIndex >= 0 &&
        m_hoveredLineIndex < (int)m_indexLines.size()) {
        const IndexedLine &line = m_indexLines[m_hoveredLineIndex];
        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(line.routeGUID);
        wxArrayString infoLines;
        BuildHoverInfoLines(line, route.get(), &infoLines);

        wxPoint boxOrigin(m_hoverScreenPos.x + 16, m_hoverScreenPos.y + 16);
        DrawHoverBoxGL(infoLines, boxOrigin);
    }

    return true;
}

void indexed_parallel_lines_pi::OnDialogClose(wxCloseEvent &event)
{
    if (m_pListRefreshTimer) {
        m_pListRefreshTimer->Stop();
        m_pListRefreshTimer = NULL;  // owned by m_pDialog, destroyed with it
    }
    m_pDialog->Destroy();
    m_pDialog = NULL;
    m_pListCtrl = NULL;
    m_pRouteFilterChoice = NULL;
    m_pPickStatusLabel = NULL;
    m_pickState = PICK_NONE;
    SetToolbarItemState(m_indexed_parallel_lines_button_id, false);
}

void indexed_parallel_lines_pi::LoadIndexedLines(void)
{
    wxFileConfig *pConf = (wxFileConfig *)GetOCPNConfigObject();
    if (!pConf) return;

    pConf->SetPath(CONFIG_GROUP);
    long count = 0;
    pConf->Read(_T("Count"), &count, 0L);

    m_indexLines.clear();
    for (long i = 0; i < count; i++) {
        wxString group = wxString::Format(_T("Line%ld"), i);
        pConf->SetPath(CONFIG_GROUP + _T("/") + group);

        IndexedLine line;
        pConf->Read(_T("Name"), &line.name, wxEmptyString);
        pConf->Read(_T("RouteGUID"), &line.routeGUID, wxEmptyString);
        pConf->Read(_T("Wp0GUID"), &line.wp0GUID, wxEmptyString);
        pConf->Read(_T("Wp1GUID"), &line.wp1GUID, wxEmptyString);
        pConf->Read(_T("OffsetNM"), &line.offsetNM, 0.0);
        bool starboard = true;
        pConf->Read(_T("Starboard"), &starboard, true);
        line.starboardSide = starboard;

        bool visible = true;
        pConf->Read(_T("Visible"), &visible, true);
        line.visible = visible;

        bool isPerpendicular = false;
        pConf->Read(_T("IsPerpendicular"), &isPerpendicular, false);
        line.isPerpendicular = isPerpendicular;
        pConf->Read(_T("AlongTrackNM"), &line.alongTrackNM, 0.0);

        m_indexLines.push_back(line);
        pConf->SetPath(CONFIG_GROUP);
    }
}

void indexed_parallel_lines_pi::SaveIndexedLines(void)
{
    wxFileConfig *pConf = (wxFileConfig *)GetOCPNConfigObject();
    if (!pConf) return;

    pConf->DeleteGroup(CONFIG_GROUP);
    pConf->SetPath(CONFIG_GROUP);
    pConf->Write(_T("Count"), (long)m_indexLines.size());

    for (size_t i = 0; i < m_indexLines.size(); i++) {
        const IndexedLine &line = m_indexLines[i];
        wxString group = wxString::Format(_T("Line%zu"), i);
        pConf->SetPath(CONFIG_GROUP + _T("/") + group);

        pConf->Write(_T("Name"), line.name);
        pConf->Write(_T("RouteGUID"), line.routeGUID);
        pConf->Write(_T("Wp0GUID"), line.wp0GUID);
        pConf->Write(_T("Wp1GUID"), line.wp1GUID);
        pConf->Write(_T("OffsetNM"), line.offsetNM);
        pConf->Write(_T("Starboard"), line.starboardSide);
        pConf->Write(_T("Visible"), line.visible);
        pConf->Write(_T("IsPerpendicular"), line.isPerpendicular);
        pConf->Write(_T("AlongTrackNM"), line.alongTrackNM);

        pConf->SetPath(CONFIG_GROUP);
    }
    pConf->Flush();
}

void indexed_parallel_lines_pi::LoadSettings(void)
{
    wxFileConfig *pConf = (wxFileConfig *)GetOCPNConfigObject();
    if (!pConf) return;

    pConf->SetPath(CONFIG_GROUP);
    pConf->Read(_T("LineLengthFactor"), &m_lineLengthFactor, DEFAULT_LINE_LENGTH_FACTOR);
    pConf->Read(_T("HitToleranceLevel"), &m_hitToleranceLevel, DEFAULT_HIT_TOLERANCE_PX);

    wxString lineColorStr = m_lineColor.GetAsString(wxC2S_HTML_SYNTAX);
    pConf->Read(_T("LineColor"), &lineColorStr, lineColorStr);
    m_lineColor.Set(lineColorStr);

    wxString selColorStr = m_selectedLineColor.GetAsString(wxC2S_HTML_SYNTAX);
    pConf->Read(_T("SelectedLineColor"), &selColorStr, selColorStr);
    m_selectedLineColor.Set(selColorStr);
}

void indexed_parallel_lines_pi::SaveSettings(void)
{
    wxFileConfig *pConf = (wxFileConfig *)GetOCPNConfigObject();
    if (!pConf) return;

    pConf->SetPath(CONFIG_GROUP);
    pConf->Write(_T("LineLengthFactor"), m_lineLengthFactor);
    pConf->Write(_T("HitToleranceLevel"), m_hitToleranceLevel);
    pConf->Write(_T("LineColor"), m_lineColor.GetAsString(wxC2S_HTML_SYNTAX));
    pConf->Write(_T("SelectedLineColor"), m_selectedLineColor.GetAsString(wxC2S_HTML_SYNTAX));
    pConf->Flush();
}

void indexed_parallel_lines_pi::ExportToGpx(const wxString &path,
                                             const wxArrayString &routeGUIDs)
{
    TiXmlDocument doc;
    doc.LinkEndChild(new TiXmlDeclaration("1.0", "UTF-8", ""));

    TiXmlElement *gpx = new TiXmlElement("gpx");
    gpx->SetAttribute("version", "1.1");
    gpx->SetAttribute("creator", "indexed_parallel_lines_pi");
    gpx->SetAttribute("xmlns", "http://www.topografix.com/GPX/1/1");
    gpx->SetAttribute("xmlns:ipl", GPX_EXTENSION_NS);
    doc.LinkEndChild(gpx);

    int routesWritten = 0;
    for (size_t r = 0; r < routeGUIDs.GetCount(); r++) {
        std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(routeGUIDs[r]);
        if (!route || !route->pWaypointList || route->pWaypointList->GetCount() < 2)
            continue;

        std::vector<PlugIn_Waypoint *> waypoints;
        wxPlugin_WaypointListNode *node = route->pWaypointList->GetFirst();
        while (node) {
            waypoints.push_back(node->GetData());
            node = node->GetNext();
        }

        TiXmlElement *rte = new TiXmlElement("rte");
        TiXmlElement *nameEl = new TiXmlElement("name");
        nameEl->LinkEndChild(new TiXmlText((const char *)route->m_NameString.utf8_str()));
        rte->LinkEndChild(nameEl);

        std::vector<const IndexedLine *> linesForRoute;
        for (size_t i = 0; i < m_indexLines.size(); i++)
            if (m_indexLines[i].routeGUID == routeGUIDs[r])
                linesForRoute.push_back(&m_indexLines[i]);

        if (!linesForRoute.empty()) {
            TiXmlElement *linesEl = new TiXmlElement("ipl:indexedLines");
            for (size_t li = 0; li < linesForRoute.size(); li++) {
                const IndexedLine &line = *linesForRoute[li];

                int startIdx = -1, endIdx = -1;
                for (size_t w = 0; w < waypoints.size(); w++) {
                    if (waypoints[w]->m_GUID == line.wp0GUID) startIdx = (int)w;
                    if (waypoints[w]->m_GUID == line.wp1GUID) endIdx = (int)w;
                }
                if (startIdx < 0 || endIdx < 0) continue;

                TiXmlElement *lineEl = new TiXmlElement("ipl:line");
                lineEl->SetAttribute("name", (const char *)line.name.utf8_str());
                lineEl->SetAttribute("legStartIndex", startIdx);
                lineEl->SetAttribute("legEndIndex", endIdx);
                lineEl->SetAttribute("type", line.isPerpendicular ? "perpendicular" : "parallel");
                lineEl->SetDoubleAttribute("offsetNM", line.offsetNM);
                if (line.isPerpendicular)
                    lineEl->SetDoubleAttribute("alongTrackNM", line.alongTrackNM);
                else
                    lineEl->SetAttribute("side", line.starboardSide ? "starboard" : "port");
                lineEl->SetAttribute("visible", line.visible ? 1 : 0);
                linesEl->LinkEndChild(lineEl);
            }

            if (linesEl->FirstChildElement()) {
                TiXmlElement *ext = new TiXmlElement("extensions");
                ext->LinkEndChild(linesEl);
                rte->LinkEndChild(ext);
            } else {
                delete linesEl;
            }
        }

        for (size_t w = 0; w < waypoints.size(); w++) {
            TiXmlElement *pt = new TiXmlElement("rtept");
            pt->SetDoubleAttribute("lat", waypoints[w]->m_lat);
            pt->SetDoubleAttribute("lon", waypoints[w]->m_lon);
            TiXmlElement *ptName = new TiXmlElement("name");
            ptName->LinkEndChild(new TiXmlText((const char *)waypoints[w]->m_MarkName.utf8_str()));
            pt->LinkEndChild(ptName);
            rte->LinkEndChild(pt);
        }

        gpx->LinkEndChild(rte);
        routesWritten++;
    }

    if (routesWritten == 0) {
        wxMessageBox(_("Nothing to export - the selected route(s) could not be resolved."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_WARNING, m_pDialog);
        return;
    }

    if (!doc.SaveFile(std::string(path.mb_str()))) {
        wxMessageBox(_("Failed to write the export file."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_ERROR, m_pDialog);
        return;
    }

    wxMessageBox(wxString::Format(_("Exported %d route(s) to:\n%s"), routesWritten, path.c_str()),
                 _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION, m_pDialog);
}

void indexed_parallel_lines_pi::ImportFromGpx(const wxString &path)
{
    TiXmlDocument doc;
    if (!doc.LoadFile(std::string(path.mb_str()))) {
        wxMessageBox(_("Failed to read or parse the selected file."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_ERROR, m_pDialog);
        return;
    }

    TiXmlElement *gpx = doc.FirstChildElement("gpx");
    if (!gpx) {
        wxMessageBox(_("This does not look like a valid GPX file."),
                     _("Indexed Parallel Navigation"), wxOK | wxICON_ERROR, m_pDialog);
        return;
    }

    struct ParsedPt {
        double lat, lon;
        wxString name;
    };

    int importedLines = 0, skippedLines = 0, newRoutes = 0, routesSeen = 0;

    for (TiXmlElement *rte = gpx->FirstChildElement("rte"); rte;
         rte = rte->NextSiblingElement("rte")) {
        wxString routeName;
        TiXmlElement *rteNameEl = rte->FirstChildElement("name");
        if (rteNameEl && rteNameEl->GetText())
            routeName = wxString::FromUTF8(rteNameEl->GetText());

        std::vector<ParsedPt> pts;
        for (TiXmlElement *pt = rte->FirstChildElement("rtept"); pt;
             pt = pt->NextSiblingElement("rtept")) {
            ParsedPt p;
            p.lat = 0.0;
            p.lon = 0.0;
            pt->QueryDoubleAttribute("lat", &p.lat);
            pt->QueryDoubleAttribute("lon", &p.lon);
            TiXmlElement *ptNameEl = pt->FirstChildElement("name");
            if (ptNameEl && ptNameEl->GetText())
                p.name = wxString::FromUTF8(ptNameEl->GetText());
            pts.push_back(p);
        }
        if (pts.size() < 2) continue;
        routesSeen++;

        // Find an existing route matching by name and waypoint positions, so
        // re-importing a plan someone already has doesn't create a duplicate
        // copy of the route itself.
        wxString matchedRouteGUID;
        wxArrayString existingGUIDs = GetRouteGUIDArray();
        for (size_t i = 0; i < existingGUIDs.GetCount(); i++) {
            std::unique_ptr<PlugIn_Route> candidate = GetRoute_Plugin(existingGUIDs[i]);
            if (!candidate || !candidate->pWaypointList) continue;
            if (candidate->m_NameString != routeName) continue;
            if ((size_t)candidate->pWaypointList->GetCount() != pts.size()) continue;

            bool allMatch = true;
            wxPlugin_WaypointListNode *node = candidate->pWaypointList->GetFirst();
            for (size_t i2 = 0; i2 < pts.size() && node; i2++, node = node->GetNext()) {
                if (fabs(node->GetData()->m_lat - pts[i2].lat) > 1e-5 ||
                    fabs(node->GetData()->m_lon - pts[i2].lon) > 1e-5) {
                    allMatch = false;
                    break;
                }
            }
            if (allMatch) {
                matchedRouteGUID = existingGUIDs[i];
                break;
            }
        }

        wxArrayString wpGUIDs;
        wxString routeGUID;
        if (!matchedRouteGUID.IsEmpty()) {
            routeGUID = matchedRouteGUID;
            std::unique_ptr<PlugIn_Route> route = GetRoute_Plugin(routeGUID);
            wxPlugin_WaypointListNode *node = route->pWaypointList->GetFirst();
            while (node) {
                wpGUIDs.Add(node->GetData()->m_GUID);
                node = node->GetNext();
            }
        } else {
            routeGUID = GetNewGUID();
            PlugIn_Route newRoute;  // ctor already allocates pWaypointList
            newRoute.m_NameString = routeName;
            newRoute.m_GUID = routeGUID;

            std::vector<PlugIn_Waypoint *> tempWaypoints;
            for (size_t i2 = 0; i2 < pts.size(); i2++) {
                wxString wpGUID = GetNewGUID();
                wxString wpName = pts[i2].name.IsEmpty()
                                       ? wxString::Format(_("WP%d"), (int)i2 + 1)
                                       : pts[i2].name;
                PlugIn_Waypoint *wp = new PlugIn_Waypoint(
                    pts[i2].lat, pts[i2].lon, _T("triangle"), wpName, wpGUID);
                newRoute.pWaypointList->Append(wp);
                tempWaypoints.push_back(wp);
                wpGUIDs.Add(wpGUID);
            }

            AddPlugInRoute(&newRoute, true);

            // AddPlugInRoute deep-copies waypoint data rather than taking
            // ownership, and PlugIn_Route's destructor does not delete its
            // waypoints (DeleteContents(false)) - the caller must free them.
            for (size_t k = 0; k < tempWaypoints.size(); k++) delete tempWaypoints[k];
            newRoutes++;
        }

        TiXmlElement *ext = rte->FirstChildElement("extensions");
        TiXmlElement *linesEl = ext ? ext->FirstChildElement("ipl:indexedLines") : NULL;
        if (!linesEl) continue;

        for (TiXmlElement *lineEl = linesEl->FirstChildElement("ipl:line"); lineEl;
             lineEl = lineEl->NextSiblingElement("ipl:line")) {
            int startIdx = -1, endIdx = -1;
            lineEl->QueryIntAttribute("legStartIndex", &startIdx);
            lineEl->QueryIntAttribute("legEndIndex", &endIdx);
            if (startIdx < 0 || endIdx < 0 || (size_t)startIdx >= wpGUIDs.GetCount() ||
                (size_t)endIdx >= wpGUIDs.GetCount())
                continue;

            IndexedLine line;
            const char *nameAttr = lineEl->Attribute("name");
            line.name = nameAttr ? wxString::FromUTF8(nameAttr) : _("Imported Line");
            line.routeGUID = routeGUID;
            line.wp0GUID = wpGUIDs[startIdx];
            line.wp1GUID = wpGUIDs[endIdx];

            const char *typeAttr = lineEl->Attribute("type");
            line.isPerpendicular = typeAttr && wxString(typeAttr) == _T("perpendicular");

            double offsetNM = 0.0;
            lineEl->QueryDoubleAttribute("offsetNM", &offsetNM);
            line.offsetNM = offsetNM;

            if (line.isPerpendicular) {
                double alongTrackNM = 0.0;
                lineEl->QueryDoubleAttribute("alongTrackNM", &alongTrackNM);
                line.alongTrackNM = alongTrackNM;
                line.starboardSide = true;
            } else {
                const char *sideAttr = lineEl->Attribute("side");
                line.starboardSide = !sideAttr || wxString(sideAttr) == _T("starboard");
            }

            int visibleAttr = 1;
            lineEl->QueryIntAttribute("visible", &visibleAttr);
            line.visible = (visibleAttr != 0);

            // Skip a line that's an exact match of one already present on
            // this route, so re-importing the same file doesn't pile up
            // duplicates; only genuinely new lines get added.
            bool duplicate = false;
            for (size_t i2 = 0; i2 < m_indexLines.size(); i2++) {
                const IndexedLine &existing = m_indexLines[i2];
                if (existing.routeGUID != line.routeGUID) continue;
                bool sameLeg =
                    (existing.wp0GUID == line.wp0GUID && existing.wp1GUID == line.wp1GUID) ||
                    (existing.wp0GUID == line.wp1GUID && existing.wp1GUID == line.wp0GUID);
                if (!sameLeg) continue;
                if (existing.isPerpendicular != line.isPerpendicular) continue;
                if (fabs(existing.offsetNM - line.offsetNM) > 1e-6) continue;
                if (line.isPerpendicular) {
                    if (fabs(existing.alongTrackNM - line.alongTrackNM) > 1e-6) continue;
                } else {
                    if (existing.starboardSide != line.starboardSide) continue;
                }
                duplicate = true;
                break;
            }

            if (duplicate) {
                skippedLines++;
                continue;
            }

            m_indexLines.push_back(line);
            importedLines++;
        }
    }

    SaveIndexedLines();
    RefreshList();
    RefreshRouteFilterChoice();
    if (GetOCPNCanvasWindow()) RequestRefresh(GetOCPNCanvasWindow());

    wxString summary = wxString::Format(
        _("Imported %d indexed line(s) on %d route(s), %d new route(s) created."),
        importedLines, routesSeen, newRoutes);
    if (skippedLines > 0)
        summary += wxString::Format(_("\nSkipped %d duplicate line(s) already present."),
                                     skippedLines);
    wxMessageBox(summary, _("Indexed Parallel Navigation"), wxOK | wxICON_INFORMATION, m_pDialog);
}
