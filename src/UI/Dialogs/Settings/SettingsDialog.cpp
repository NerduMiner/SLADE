
#include "Main.h"
#include "SettingsDialog.h"
#include "AdvancedSettingsPanel.h"
#include "App.h"
#include "AudioSettingsPanel.h"
#include "EditingSettingsPanel.h"
#include "GeneralSettingsPanel.h"
#include "GraphicsSettingsPanel.h"
#include "InputSettingsPanel.h"
#include "InterfaceSettingsPanel.h"
#include "ScriptSettingsPanel.h"
#include "TextEditorSettingsPanel.h"
#include "UI/Layout.h"
#include "UI/SToolBar/SToolBarButton.h"
#include "UI/WxUtils.h"

using namespace slade;
using namespace ui;


namespace
{
wxColour sidePanelColour()
{
	auto bgcol = wxutil::systemPanelBGColour();
	return app::isDarkTheme() ? bgcol.ChangeLightness(105) : bgcol.ChangeLightness(95);
}

void updateMinSize(const wxWindow* window, int& min_width, int& min_height)
{
	auto size  = window->GetBestSize();
	min_width  = std::max(min_width, size.GetWidth());
	min_height = std::max(min_height, size.GetHeight());
}
} // namespace




SettingsDialog::SettingsDialog(wxWindow* parent) : SDialog(parent, "SLADE Settings", "settings")
{
	auto lh = LayoutHelper(this);

	// Set icon
	wxutil::setWindowIcon(this, "settings");

	// Setup main sizer
	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(sizer);

	// Sections
	auto sections_panel = createSectionsPanel();
	sizer->Add(sections_panel, wxSizerFlags(0).Expand());

	content_sizer_ = new wxBoxSizer(wxVERTICAL);
	sizer->Add(content_sizer_, wxSizerFlags(1).Expand());

	// Title
	auto title_panel = new wxPanel(this);
	auto title_sizer = new wxBoxSizer(wxHORIZONTAL);
	title_panel->SetSizer(title_sizer);
	title_text_ = new wxStaticText(title_panel, -1, "General Settings");
	title_text_->SetFont(GetFont().MakeLarger().MakeLarger().Bold());
	title_sizer->Add(title_text_, lh.sfWithLargeBorder(1, wxLEFT | wxTOP).Expand());
	content_sizer_->Add(title_panel, wxSizerFlags().Expand());

	// Settings pages
	settings_pages_[static_cast<size_t>(SettingsPage::General)]   = new GeneralSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Interface)] = new InterfaceSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Keybinds)]  = new InputSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Editing)]   = new EditingSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Text)]      = new TextEditorSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Graphics)]  = new GraphicsSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Audio)]     = new AudioSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Scripting)] = new ScriptSettingsPanel(this);
	settings_pages_[static_cast<size_t>(SettingsPage::Advanced)]  = new AdvancedSettingsPanel(this);
	auto general_panel                                            = settings_pages_[0];
	section_buttons_[0]->setChecked(true);
	content_sizer_->Add(general_panel, lh.sfWithLargeBorder(1).Expand());
	general_panel->Show();
	current_page_ = general_panel;

	// Buttons
	auto button_sizer = new wxBoxSizer(wxHORIZONTAL);
	button_sizer->Add(new wxButton(this, -1, "Apply"), wxSizerFlags(0).Expand());
	button_sizer->AddStretchSpacer();
	button_sizer->Add(new wxButton(this, -1, "OK"), lh.sfWithBorder(0, wxRIGHT).Expand());
	button_sizer->Add(new wxButton(this, -1, "Cancel"), wxSizerFlags(0).Expand());
	content_sizer_->Add(button_sizer, lh.sfWithLargeBorder(0, wxLEFT | wxRIGHT | wxBOTTOM).Expand());

	Bind(wxEVT_STOOLBAR_BUTTON_CLICKED, &SettingsDialog::onSectionButtonClicked, this);

	// Determine best minimum size based on larger pages
	int min_width  = 0;
	int min_height = 0;
	for (const auto* page : settings_pages_)
		updateMinSize(page, min_width, min_height);
	SetMinSize({ sections_panel->GetBestSize().x + min_width + FromDIP(100),
				 min_height + button_sizer->CalcMin().y + title_panel->GetBestSize().y + FromDIP(100) });
}

void SettingsDialog::createSectionButton(
	wxWindow*     parent,
	SettingsPage  page,
	const string& action,
	const string& text,
	const string& icon)
{
	auto btn = new SToolBarButton(parent, action, text, icon, text, true, 24);
	btn->setPadding(8, 0);
	btn->setExactFit(false);
	btn->setFontSize(1.1f);
	btn->SetBackgroundColour(sidePanelColour());
	btn->setFillChecked(true);

	section_buttons_[static_cast<size_t>(page)] = btn;
}

wxPanel* SettingsDialog::createSectionsPanel()
{
	auto panel = new wxPanel(this);
	auto lh    = LayoutHelper(panel);
	auto sizer = new wxBoxSizer(wxVERTICAL);
	panel->SetSizer(sizer);

	auto vbox = new wxBoxSizer(wxVERTICAL);
	sizer->Add(vbox, lh.sfWithLargeBorder(1).Expand());

	// Create section buttons
	createSectionButton(panel, SettingsPage::General, "general", "General", "logo");
	createSectionButton(panel, SettingsPage::Interface, "interface", "Interface", "settings");
	createSectionButton(panel, SettingsPage::Keybinds, "keybinds", "Keyboard Shortcuts", "settings");
	createSectionButton(panel, SettingsPage::Editing, "editing", "Editing", "wrench");
	createSectionButton(panel, SettingsPage::Text, "text", "Text Editor", "text");
	createSectionButton(panel, SettingsPage::Graphics, "gfx", "Graphics", "gfx");
	createSectionButton(panel, SettingsPage::Audio, "audio", "Audio", "sound");
	createSectionButton(panel, SettingsPage::Scripting, "scripts", "ACS Scripts", "script");
	createSectionButton(panel, SettingsPage::Advanced, "advanced", "Advanced", "settings");

	// Set all to width of 'Keyboard Shortcuts' button since it's the widest
	auto keybinds_button = sectionButton(SettingsPage::Keybinds);
	keybinds_button->setExactFit(true);
	auto width = keybinds_button->GetMinSize().GetWidth();
	for (auto* btn : section_buttons_)
		if (btn != keybinds_button)
			btn->SetSize(wxSize(width, -1));

	// Layout buttons
	auto advanced_button = sectionButton(SettingsPage::Advanced);
	for (auto* btn : section_buttons_)
		if (btn != advanced_button)
			vbox->Add(btn, lh.sfWithSmallBorder(0, wxBOTTOM).Expand());
	vbox->AddStretchSpacer();
	vbox->Add(advanced_button, wxSizerFlags().Expand());

	panel->SetBackgroundColour(sidePanelColour());

	return panel;
}

// ReSharper disable CppParameterMayBeConstPtrOrRef
void SettingsDialog::onSectionButtonClicked(wxCommandEvent& e)
{
	auto btn = dynamic_cast<SToolBarButton*>(e.GetEventObject());
	if (!btn)
		return;

	// Uncheck all buttons
	for (auto* b : section_buttons_)
		b->setChecked(false);

	// Check the clicked button
	btn->setChecked(true);

	// Show the appropriate panel
	SettingsPanel* new_page;
	auto           index = std::distance(
        section_buttons_.begin(), std::find(section_buttons_.begin(), section_buttons_.end(), btn));
	new_page = settings_pages_[index];
	title_text_->SetLabel(new_page->title());

	new_page->Hide();
	content_sizer_->Replace(current_page_, new_page);
	current_page_->Hide();
	current_page_ = new_page;
	current_page_->Show();

	Layout();
	Refresh();
}
