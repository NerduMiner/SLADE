#pragma once

#include "PrefsPanelBase.h"
#include "UI/Controls/STabCtrl.h"

namespace slade
{
class STabCtrl;
namespace ui
{
	struct LayoutHelper;
}

class MapDisplayPrefsPanel : public PrefsPanelBase
{
public:
	MapDisplayPrefsPanel(wxWindow* parent);
	~MapDisplayPrefsPanel() override = default;

	void setupGeneralTab(const ui::LayoutHelper& lh);
	void setupVerticesTab(const ui::LayoutHelper& lh);
	void setupLinesTab(const ui::LayoutHelper& lh);
	void setupThingsTab(const ui::LayoutHelper& lh);
	void setupFlatsTab(const ui::LayoutHelper& lh);
	void init() override;
	void applyPreferences() override;

	wxString pageTitle() override { return "Map Editor Display Settings"; }

private:
	TabControl* stc_pages_ = nullptr;

	wxCheckBox* cb_grid_dashed_       = nullptr;
	wxChoice*   choice_grid_64_       = nullptr;
	wxCheckBox* cb_grid_show_origin_  = nullptr;
	wxCheckBox* cb_animate_hilight_   = nullptr;
	wxCheckBox* cb_animate_selection_ = nullptr;
	wxCheckBox* cb_animate_tagged_    = nullptr;
	wxChoice*   choice_crosshair_     = nullptr;
	wxCheckBox* cb_action_lines_      = nullptr;
	wxCheckBox* cb_show_help_         = nullptr;
	wxChoice*   choice_tex_filter_    = nullptr;

	wxSlider*   slider_vertex_size_     = nullptr;
	wxCheckBox* cb_vertex_round_        = nullptr;
	wxChoice*   choice_vertices_always_ = nullptr;

	wxSlider*   slider_line_width_   = nullptr;
	wxCheckBox* cb_line_smooth_      = nullptr;
	wxCheckBox* cb_line_tabs_always_ = nullptr;
	wxCheckBox* cb_line_fade_        = nullptr;

	wxChoice*   choice_thing_shape_      = nullptr;
	wxChoice*   choice_things_always_    = nullptr;
	wxCheckBox* cb_thing_sprites_        = nullptr;
	wxCheckBox* cb_thing_force_dir_      = nullptr;
	wxCheckBox* cb_thing_overlay_square_ = nullptr;
	wxSlider*   slider_thing_shadow_     = nullptr;
	wxCheckBox* cb_use_zeth_icons_       = nullptr;
	wxSlider*   slider_halo_width_       = nullptr;
	wxSlider*   slider_light_intensity_  = nullptr;

	wxSlider*   slider_flat_brightness_  = nullptr;
	wxCheckBox* cb_flat_ignore_light_    = nullptr;
	wxCheckBox* cb_sector_hilight_fill_  = nullptr;
	wxCheckBox* cb_flat_fade_            = nullptr;
	wxCheckBox* cb_sector_selected_fill_ = nullptr;
};
} // namespace slade
