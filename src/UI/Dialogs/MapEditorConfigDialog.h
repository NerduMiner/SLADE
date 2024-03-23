#pragma once

// #include "Archive/Archive.h"
// #include "UI/Lists/ListView.h"
#include "UI/SDialog.h"

class wxImageList;

namespace slade
{
class ListView;
struct MapDesc;
class Archive;
class MapPreviewCanvas;
class BaseResourceChooser;
class ResourceArchiveChooser;

class MapEditorConfigDialog : public SDialog
{
public:
	MapEditorConfigDialog(
		wxWindow* parent,
		Archive*  archive      = nullptr,
		bool      show_maplist = true,
		bool      creating     = false);
	~MapEditorConfigDialog() override;

	void     populateGameList();
	void     populatePortList();
	void     populateMapList();
	MapDesc  selectedMap();
	bool     configMatchesMap(const MapDesc& map) const;
	wxString selectedGame();
	wxString selectedPort();

private:
	wxChoice*               choice_game_config_   = nullptr;
	wxChoice*               choice_port_config_   = nullptr;
	BaseResourceChooser*    choice_base_resource_ = nullptr;
	ListView*               list_maps_            = nullptr;
	ResourceArchiveChooser* rac_resources_        = nullptr;
	wxButton*               btn_new_map_          = nullptr;
	MapPreviewCanvas*       canvas_preview_       = nullptr;
	wxImageList*            img_list_             = nullptr;
	wxButton*               btn_ok_               = nullptr;
	wxButton*               btn_cancel_           = nullptr;
	wxString                game_current_;
	wxString                port_current_;
	bool                    creating_ = false;

	Archive*         archive_ = nullptr;
	vector<MapDesc>  maps_;
	vector<wxString> games_list_;
	vector<wxString> ports_list_;

	// Events
	void onChoiceGameConfigChanged(wxCommandEvent& e);
	void onChoicePortConfigChanged(wxCommandEvent& e);
	void onMapActivated(wxListEvent& e);
	void onBtnNewMap(wxCommandEvent& e);
	void onMapSelected(wxListEvent& e);
};
} // namespace slade
