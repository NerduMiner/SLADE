
#include "Main.h"
#include "LibraryPanel.h"
#include "Archive/Archive.h"
#include "General/Database.h"
#include "General/Misc.h"
#include "General/UI.h"
#include "Graphics/Icons.h"
#include "Library/ArchiveFile.h"
#include "Library/Library.h"
#include "MainEditor/MainEditor.h"
#include "UI/Dialogs/RunDialog.h"
#include "UI/SToolBar/SToolBar.h"
#include "UI/State.h"
#include "UI/WxUtils.h"
#include "Utility/DateTime.h"

using namespace slade;
using namespace ui;
using namespace library;


namespace
{
icons::IconCache icon_cache;
}


namespace
{
template<typename T> int compare(const T& left, const T& right)
{
	if (left == right)
		return 0;

	return left < right ? -1 : 1;
}
} // namespace

LibraryViewModel::LibraryViewModel()
{
	loadRows();
	Cleared();

	// Connect to library signals ----------------------------------------------
	auto& signals = library::signals();

	// Archive updated
	signal_connections_ += signals.archive_file_updated.connect(
		[this](int64_t id)
		{
			for (auto& row : rows_)
				if (row.id == id)
				{
					row = ArchiveFileRow{ db::global(), id };
					ItemChanged(wxDataViewItem{ &row });
				}
		});

	// Archive added
	signal_connections_ += signals.archive_file_inserted.connect(
		[this](int64_t id)
		{
			rows_.emplace_back(database::global(), id);
			ItemAdded({}, wxDataViewItem{ &rows_.back() });
		});

	// Archive deleted
	signal_connections_ += signals.archive_file_deleted.connect(
		[this](int64_t id)
		{
			for (auto i = 0; i < rows_.size(); ++i)
				if (rows_[i].id == id)
				{
					ItemDeleted({}, wxDataViewItem{ &rows_[i] });
					rows_.erase(rows_.begin() + i);
					break;
				}
		});
}

wxDataViewItem LibraryViewModel::itemForArchiveId(int64_t id) const
{
	for (auto& row : rows_)
		if (row.id == id)
			return wxDataViewItem{ &row };

	return {};
}

wxString LibraryViewModel::GetColumnType(unsigned col) const
{
	switch (static_cast<Column>(col))
	{
	case Column::Name: return "wxDataViewIconText";
	default: return "string";
	}
}

void LibraryViewModel::GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned col) const
{
	auto* row = static_cast<ArchiveFileRow*>(item.GetID());
	if (!row)
		return;

	switch (static_cast<Column>(col))
	{
	case Column::Name:
	{
		// Determine icon
		string icon = "archive";
		if (row->format_id == "wad")
			icon = "wad";
		else if (row->format_id == "zip")
			icon = "zip";
		else if (row->format_id == "folder")
			icon = "folder";

		// Find icon in cache
		if (!icon_cache.isCached(icon))
		{
			// Not found, add to cache
			const auto pad = Point2i{ 1, 1 };
			icon_cache.cacheIcon(icons::Type::Entry, icon, 16, pad);
		}

		auto fn = wxutil::strFromView(strutil::Path::fileNameOf(row->path));

		variant << wxDataViewIconText(fn, icon_cache.icons[icon]);

		break;
	}
	case Column::Path: variant = wxutil::strFromView(strutil::Path::pathOf(row->path, false)); break;
	case Column::Size: variant = misc::sizeAsString(row->size); break;
	case Column::Type:
	{
		auto fn_ext = strutil::Path::extensionOf(row->path);
		auto desc   = archive::formatDesc(row->format_id);

		variant = archive::formatDesc(row->format_id).name;

		for (const auto& ext : desc.extensions)
			if (strutil::equalCI(fn_ext, ext.first))
				variant = ext.second;

		break;
	}
	case Column::LastOpened:
		if (row->last_opened == 0)
			variant = "Never";
		else
			variant = datetime::toString(row->last_opened, datetime::Format::Local);
		break;
	case Column::FileModified:
		if (row->last_modified == 0)
			variant = "Unknown";
		else
			variant = datetime::toString(row->last_modified, datetime::Format::Local);
		break;
	case Column::_Count: break;
	default: break;
	}
}

bool LibraryViewModel::GetAttr(const wxDataViewItem& item, unsigned col, wxDataViewItemAttr& attr) const
{
	return wxDataViewModel::GetAttr(item, col, attr);
}

bool LibraryViewModel::SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned col)
{
	return false;
}

wxDataViewItem LibraryViewModel::GetParent(const wxDataViewItem& item) const
{
	return {};
}

bool LibraryViewModel::IsContainer(const wxDataViewItem& item) const
{
	return !item.IsOk();
}

unsigned LibraryViewModel::GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const
{
	if (!item.IsOk())
	{
		// Root item
		for (auto& row : rows_)
			children.Add(wxDataViewItem{ &row });

		return rows_.size();
	}

	return 0;
}

int LibraryViewModel::Compare(const wxDataViewItem& item1, const wxDataViewItem& item2, unsigned column, bool ascending)
	const
{
	auto row1 = static_cast<ArchiveFileRow*>(item1.GetID());
	auto row2 = static_cast<ArchiveFileRow*>(item2.GetID());
	if (!row1 || !row2)
		return 0;

	auto col = static_cast<Column>(column);

	// Size column
	if (col == Column::Size)
		return ascending ? compare(row1->size, row2->size) : compare(row2->size, row1->size);

	// Last Opened column
	if (col == Column::LastOpened)
		return ascending ? compare(row1->last_opened, row2->last_opened) :
                           compare(row2->last_opened, row1->last_opened);

	// File Modified column
	if (col == Column::FileModified)
		return ascending ? compare(row1->last_modified, row2->last_modified) :
                           compare(row2->last_modified, row1->last_modified);

	// Default compare
	return wxDataViewModel::Compare(item1, item2, column, ascending);
}

void LibraryViewModel::loadRows() const
{
	rows_ = library::allArchiveFileRows();
}

LibraryPanel::LibraryPanel(wxWindow* parent) : wxPanel{ parent }
{
	setup();
}

void LibraryPanel::setup()
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	// Toolbar
	toolbar_ = new SToolBar(this);
	sizer->Add(toolbar_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad());
	sizer->AddSpacer(px(Size::PadMinimum));
	toolbar_->addActionGroup("_Library", { "alib_open", "alib_run", "alib_remove" });

	// Archive list
	list_archives_ = new SDataViewCtrl{ this, wxDV_MULTIPLE };
	sizer->Add(list_archives_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad());

	// Init archive list
	model_library_ = new LibraryViewModel();
	list_archives_->AssociateModel(model_library_);
	model_library_->DecRef();
	setupListColumns();

	bindEvents();
}

bool LibraryPanel::handleAction(string_view id)
{
	// Open
	if (id == "alib_open")
	{
		wxDataViewItemArray selection;
		list_archives_->GetSelections(selection);

		for (const auto& item : selection)
			if (auto row = static_cast<ArchiveFileRow*>(item.GetID()))
				maineditor::openArchiveFile(row->path);

		return true;
	}

	// Remove
	if (id == "alib_remove")
	{
		wxDataViewItemArray selection;
		list_archives_->GetSelections(selection);

		vector<int64_t> to_remove;
		for (const auto& item : selection)
			if (auto row = static_cast<ArchiveFileRow*>(item.GetID()))
				to_remove.push_back(row->id);

		for (auto archive_id : to_remove)
			library::removeArchiveFile(archive_id);

		return true;
	}

	// Run
	if (id == "alib_run")
	{
		wxDataViewItemArray selection;
		list_archives_->GetSelections(selection);

		string path;
		for (const auto& item : selection)
			if (auto row = static_cast<ArchiveFileRow*>(item.GetID()))
			{
				path = row->path;
				break;
			}

		RunDialog dlg{ this, nullptr };
		if (dlg.ShowModal() == wxID_OK) // TODO: IWAD probably shouldn't be just the current base resource
			dlg.run(RunDialog::Config{ path });

		return true;
	}

	return false;
}

void LibraryPanel::bindEvents()
{
	// Open archive if activated
	list_archives_->Bind(
		wxEVT_DATAVIEW_ITEM_ACTIVATED,
		[this](wxDataViewEvent& e)
		{
			if (auto row = static_cast<ArchiveFileRow*>(e.GetItem().GetID()))
				maineditor::openArchiveFile(row->path);
		});

	// Context menu
	list_archives_->Bind(
		wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
		[this](wxDataViewEvent& e)
		{
			wxMenu context;
			SAction::fromId("alib_open")->addToMenu(&context);
			SAction::fromId("alib_run")->addToMenu(&context);
			SAction::fromId("alib_remove")->addToMenu(&context);
			list_archives_->PopupMenu(&context);
		});

	// Header right click
	list_archives_->Bind(
		wxEVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK,
		[this](wxDataViewEvent& e)
		{
			using Column = LibraryViewModel::Column;

			// Popup context menu
			wxMenu context;
			context.Append(static_cast<int>(Column::_Count), "Reset Sorting");
			context.AppendSeparator();
			list_archives_->appendColumnToggleItem(context, static_cast<int>(Column::Path));
			list_archives_->appendColumnToggleItem(context, static_cast<int>(Column::Size));
			list_archives_->appendColumnToggleItem(context, static_cast<int>(Column::Type));
			list_archives_->appendColumnToggleItem(context, static_cast<int>(Column::LastOpened));
			list_archives_->appendColumnToggleItem(context, static_cast<int>(Column::FileModified));
			list_archives_->PopupMenu(&context);
			e.Skip();
		});

	// Header context menu
	list_archives_->Bind(
		wxEVT_MENU,
		[this](wxCommandEvent& e)
		{
			using Column = LibraryViewModel::Column;

			if (e.GetId() == static_cast<int>(Column::_Count))
				list_archives_->resetSorting();
			else if (e.GetId() == static_cast<int>(Column::Path))
			{
				list_archives_->toggleColumnVisibility(e.GetId(), "LibraryPanelPathVisible");
				updateColumnWidths();
			}
			else if (e.GetId() == static_cast<int>(Column::Size))
			{
				list_archives_->toggleColumnVisibility(e.GetId(), "LibraryPanelSizeVisible");
				updateColumnWidths();
			}
			else if (e.GetId() == static_cast<int>(Column::Type))
			{
				list_archives_->toggleColumnVisibility(e.GetId(), "LibraryPanelTypeVisible");
				updateColumnWidths();
			}
			else if (e.GetId() == static_cast<int>(Column::LastOpened))
			{
				list_archives_->toggleColumnVisibility(e.GetId(), "LibraryPanelLastOpenedVisible");
				updateColumnWidths();
			}
			else if (e.GetId() == static_cast<int>(Column::FileModified))
			{
				list_archives_->toggleColumnVisibility(e.GetId(), "LibraryPanelFileModifiedVisible");
				updateColumnWidths();
			}
			else
				e.Skip();
		});

	// List column resized
	list_archives_->Bind(
		EVT_SDVC_COLUMN_RESIZED,
		[this](wxDataViewEvent& e)
		{
			using Column = LibraryViewModel::Column;
			auto col     = static_cast<Column>(e.GetColumn());
			auto width   = e.GetDataViewColumn()->GetWidth();

			switch (col)
			{
			case Column::Name: saveStateInt("LibraryPanelFilenameWidth", width); break;
			case Column::Path: saveStateInt("LibraryPanelPathWidth", width); break;
			case Column::Size: saveStateInt("LibraryPanelSizeWidth", width); break;
			case Column::Type: saveStateInt("LibraryPanelTypeWidth", width); break;
			case Column::LastOpened: saveStateInt("LibraryPanelLastOpenedWidth", width); break;
			case Column::FileModified: saveStateInt("LibraryPanelFileModifiedWidth", width); break;
			default: break;
			}
		});
}

void LibraryPanel::setupListColumns() const
{
	using Column = LibraryViewModel::Column;

	// Search by filename column
	list_archives_->setSearchColumn(static_cast<int>(Column::Name));

	auto colstyle_visible = wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE;
	auto colstyle_hidden  = colstyle_visible | wxDATAVIEW_COL_HIDDEN;

	list_archives_->AppendIconTextColumn(
		"Filename",
		static_cast<int>(Column::Name),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelFilenameWidth"),
		wxALIGN_NOT,
		colstyle_visible);
	list_archives_->AppendTextColumn(
		"Path",
		static_cast<int>(Column::Path),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelPathWidth"),
		wxALIGN_NOT,
		getStateBool("LibraryPanelPathVisible") ? colstyle_visible : colstyle_hidden);
	list_archives_->AppendTextColumn(
		"Size",
		static_cast<int>(Column::Size),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelSizeWidth"),
		wxALIGN_NOT,
		getStateBool("LibraryPanelSizeVisible") ? colstyle_visible : colstyle_hidden);
	list_archives_->AppendTextColumn(
		"Type",
		static_cast<int>(Column::Type),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelTypeWidth"),
		wxALIGN_NOT,
		getStateBool("LibraryPanelTypeVisible") ? colstyle_visible : colstyle_hidden);
	list_archives_->AppendTextColumn(
		"Last Opened",
		static_cast<int>(Column::LastOpened),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelLastOpenedWidth"),
		wxALIGN_NOT,
		getStateBool("LibraryPanelLastOpenedVisible") ? colstyle_visible : colstyle_hidden);
	list_archives_->AppendTextColumn(
		"File Modified",
		static_cast<int>(Column::FileModified),
		wxDATAVIEW_CELL_INERT,
		getStateInt("LibraryPanelFileModifiedWidth"),
		wxALIGN_NOT,
		getStateBool("LibraryPanelFileModifiedVisible") ? colstyle_visible : colstyle_hidden);
}

void LibraryPanel::updateColumnWidths()
{
	using Column = LibraryViewModel::Column;

	Freeze();
	list_archives_->setColumnWidth(modelColumn(Column::Name), getStateInt("LibraryPanelFilenameWidth"));
	list_archives_->setColumnWidth(modelColumn(Column::Path), getStateInt("LibraryPanelPathWidth"));
	list_archives_->setColumnWidth(modelColumn(Column::Size), getStateInt("LibraryPanelSizeWidth"));
	list_archives_->setColumnWidth(modelColumn(Column::Type), getStateInt("LibraryPanelTypeWidth"));
	list_archives_->setColumnWidth(modelColumn(Column::LastOpened), getStateInt("LibraryPanelLastOpenedWidth"));
	list_archives_->setColumnWidth(modelColumn(Column::FileModified), getStateInt("LibraryPanelFileModifiedWidth"));
	Thaw();
}
