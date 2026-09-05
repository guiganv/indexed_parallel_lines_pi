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
#ifndef _INDEXED_PARALLEL_LINESPI_H_
#define _INDEXED_PARALLEL_LINESPI_H_

#include "wxWTranslateCatalog.h"
#include "ocpn_plugin.h"

#include <wx/string.h>
#include <wx/colour.h>

#include <vector>

#include "globals.h"

class tpicons;
class wxDialog;
class wxCloseEvent;
class wxListCtrl;
class wxListEvent;
class wxTimerEvent;
class wxChoice;
class wxStaticText;
class wxKeyEvent;

// An indexed line is defined relative to a specific leg (pair of consecutive
// waypoints) of a specific route, identified by GUID. Its geometry is always
// resolved live from the route's current waypoint positions, so it tracks
// the route if the user drags/edits it.
struct IndexedLine {
    wxString name;
    wxString routeGUID;
    wxString wp0GUID;    // reference leg start waypoint
    wxString wp1GUID;    // reference leg end waypoint
    // For a parallel line: perpendicular offset distance from the leg, nm.
    // For a perpendicular line: half-length of the drawn line, nm (it
    // extends this far to each side of the leg).
    double offsetNM;
    bool starboardSide;   // parallel line only: true = offset to starboard
    bool visible = true;  // user-controlled show/hide, independent of the
                           // route's own visibility
    bool isPerpendicular = false;  // false = parallel to the leg (default)
    double alongTrackNM = 0.0;     // perpendicular line only: distance from
                                    // wp0, along the leg, where it crosses
};

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------
class indexed_parallel_lines_pi : public opencpn_plugin_118
{
public:
    indexed_parallel_lines_pi(void *ppimgr);
    ~indexed_parallel_lines_pi();

    //    The required PlugIn Methods
    int Init(void);
    bool DeInit(void);

    int GetAPIVersionMajor();
    int GetAPIVersionMinor();
    int GetPlugInVersionMajor();
    int GetPlugInVersionMinor();
    int GetPlugInVersionPatch();
    int GetPlugInVersionPost();
    wxBitmap *GetPlugInBitmap();
    wxString GetCommonName();
    wxString GetShortDescription();
    wxString GetLongDescription();

    int GetToolbarToolCount(void);
    void OnToolbarToolCallback(int id);
    void SetCursorLatLon(double lat, double lon);
    bool MouseEventHook(wxMouseEvent &event);
    bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
    bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);

private:
    enum PickState { PICK_NONE, PICK_LEG, PICK_OFFSET, PICK_REPICK_TARGET };

    void OnDialogClose(wxCloseEvent &event);
    void OnNewIndexedLineButton(wxCommandEvent &event);
    void OnNewPerpendicularLineButton(wxCommandEvent &event);
    void OnDeleteIndexedLineButton(wxCommandEvent &event);
    void OnChangeDistanceButton(wxCommandEvent &event);
    void OnRepickDistanceButton(wxCommandEvent &event);
    void OnRenameButton(wxCommandEvent &event);
    void OnSettingsButton(wxCommandEvent &event);
    void OnExportButton(wxCommandEvent &event);
    void OnExportAllButton(wxCommandEvent &event);
    void OnImportButton(wxCommandEvent &event);
    void OnCharHook(wxKeyEvent &event);
    void OnListItemRightClick(wxListEvent &event);
    void OnListRefreshTimer(wxTimerEvent &event);
    void OnListItemSelected(wxListEvent &event);
    void OnListItemDeselected(wxListEvent &event);
    void OnListItemChecked(wxListEvent &event);
    void OnListItemUnchecked(wxListEvent &event);
    void OnRouteFilterChoice(wxCommandEvent &event);
    void PromptChangeDistance(void);
    void PromptRename(void);
    void BeginRepick(size_t lineIndex);
    void CancelPick(void);
    void UpdatePickStatusLabel(void);
    void RefreshList(void);
    void SetItemIfChanged(long row, int col, const wxString &text);
    void RefreshRouteFilterChoice(void);
    void SortIndexLines(void);
    bool IsLineVisible(const IndexedLine &line);
    void LoadIndexedLines(void);
    void SaveIndexedLines(void);
    void LoadSettings(void);
    void SaveSettings(void);
    wxArrayString GetRoutesWithLines(void);
    void ExportToGpx(const wxString &path, const wxArrayString &routeGUIDs);
    void ImportFromGpx(const wxString &path);
    bool FindNearestLeg(double lat, double lon, wxString *outRouteGUID,
                         wxString *outWp0GUID, wxString *outWp1GUID);
    bool GetLiveLeg(const wxString &routeGUID, const wxString &wp0GUID,
                     const wxString &wp1GUID, double *lat0, double *lon0,
                     double *lat1, double *lon1, double *course,
                     double *legDistNM, bool *reversedInRoute);
    bool GetLiveLegFromRoute(PlugIn_Route *route, const wxString &wp0GUID,
                              const wxString &wp1GUID, double *lat0,
                              double *lon0, double *lat1, double *lon1,
                              double *course, double *legDistNM,
                              bool *reversedInRoute);
    void BuildHoverInfoLines(const IndexedLine &line, PlugIn_Route *route,
                              wxArrayString *outLines);
    void DrawHoverBoxGL(const wxArrayString &infoLines, const wxPoint &origin);
    void CrossTrackDistance(double lat0, double lon0, double lat1,
                             double lon1, double lat, double lon,
                             double *signedDistNM);
    void TrackOffsets(double lat0, double lon0, double lat1, double lon1,
                       double lat, double lon, double *crossTrackNM,
                       double *alongTrackNM);
    bool GetOffsetEndpoints(const IndexedLine &line, double *lat0,
                             double *lon0, double *lat1, double *lon1);
    bool BuildPreviewLine(IndexedLine *preview);
    bool FindLineNear(double lat, double lon, size_t *outIndex);

    tpicons     *m_ptpicons;
    int          m_indexed_parallel_lines_button_id;
    wxDialog    *m_pDialog;
    wxListCtrl  *m_pListCtrl;
    wxChoice    *m_pRouteFilterChoice;
    wxStaticText *m_pPickStatusLabel;
    double       m_lineLengthFactor;
    double       m_hitToleranceLevel;
    wxColour     m_lineColor;
    wxColour     m_selectedLineColor;
    wxArrayString m_filterChoiceGUIDs;  // index-aligned with m_pRouteFilterChoice
    wxString     m_filterRouteGUID;     // empty = show all routes
    class wxTimer *m_pListRefreshTimer;
    double       m_cursor_lat;
    double       m_cursor_lon;
    PickState    m_pickState;
    bool         m_pendingIsPerpendicular;
    IndexedLine  m_pending;
    IndexedLine  m_repickBackup;    // original line, restored if a repick is cancelled
    bool         m_repickInProgress;
    PlugIn_ViewPort m_lastViewport;
    bool         m_haveViewport;
    int          m_selectedLineIndex;  // -1 when no indexed line is selected
    int          m_hoveredLineIndex;   // -1 when the cursor isn't over a line
    wxPoint      m_hoverScreenPos;
    std::vector<size_t> m_visibleRowToLineIndex;  // list row -> m_indexLines index, after route filtering

    std::vector<IndexedLine> m_indexLines;
};

#endif
