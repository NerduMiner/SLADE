#pragma once

#include "Archive/Archive.h"
#include "General/SAction.h"
#include "MapEditor.h"

namespace slade
{
class MCOverlay;
class ItemSelection;
class InfoOverlay3D;
class ThingInfoOverlay;
class SectorInfoOverlay;
class LineInfoOverlay;
class VertexInfoOverlay;
class Edit3D;
class UndoManager;
class UndoStep;
class ObjectEdit;
class MoveObjects;
class LineDraw;
class Edit2D;
class Camera;
class MapCanvas;
namespace gl::draw2d
{
	struct Context;
}
namespace mapeditor
{
	class Renderer;
	class Input;
} // namespace mapeditor
namespace ui
{
	enum class MouseCursor;
}

class MapEditContext : public SActionHandler
{
public:
	struct EditorMessage
	{
		string message;
		long   act_time;

		EditorMessage(string_view message, long act_time) : message{ message }, act_time{ act_time } {}
	};

	MapEditContext();
	~MapEditContext() override;

	SLADEMap&             map() const { return *map_; }
	mapeditor::Mode       editMode() const { return edit_mode_; }
	mapeditor::SectorMode sectorEditMode() const { return sector_mode_; }
	double                gridSize() const;
	ItemSelection&        selection() const { return *selection_; }
	vector<MapSector*>&   taggedSectors() { return tagged_sectors_; }
	vector<MapLine*>&     taggedLines() { return tagged_lines_; }
	vector<MapThing*>&    taggedThings() { return tagged_things_; }
	vector<MapLine*>&     taggingLines() { return tagging_lines_; }
	vector<MapThing*>&    taggingThings() { return tagging_things_; }
	vector<MapThing*>&    pathedThings() { return pathed_things_; }
	bool                  gridSnap() const { return grid_snap_; }
	UndoManager*          undoManager() const { return undo_manager_.get(); }
	Archive::MapDesc&     mapDesc() { return map_desc_; }
	MapCanvas*            canvas() const { return canvas_; }
	mapeditor::Renderer&  renderer() const { return *renderer_; }
	mapeditor::Input&     input() const { return *input_; }
	bool                  mouseLocked() const { return mouse_locked_; }

	void setEditMode(mapeditor::Mode mode);
	void setPrevEditMode() { setEditMode(edit_mode_prev_); }
	void setSectorEditMode(mapeditor::SectorMode mode);
	void cycleSectorEditMode();
	void setCanvas(MapCanvas* canvas) { canvas_ = canvas; }
	void lockMouse(bool lock);

	// General
	bool update(long frametime);

	// Map loading
	bool openMap(const Archive::MapDesc& map);
	void clearMap();

	// Selection/hilight
	mapeditor::Item hilightItem() const;
	void            showItem(int index) const;
	void            updateTagged();
	void            selectionUpdated();
	void            clearSelection() const;

	// Grid
	void   incrementGrid();
	void   decrementGrid();
	double snapToGrid(double position, bool force = true) const;
	Vec2d  relativeSnapToGrid(const Vec2d& origin, const Vec2d& mouse_pos) const;

	// Tag edit
	int  beginTagEdit();
	void tagSectorAt(const Vec2d& pos);
	void endTagEdit(bool accept = true);

	// Editing handlers
	MoveObjects& moveObjects() const { return *move_objects_; }
	LineDraw&    lineDraw() const { return *line_draw_; }
	ObjectEdit&  objectEdit() const { return *object_edit_; }
	Edit3D&      edit3D() const { return *edit_3d_; }
	Edit2D&      edit2D() const { return *edit_2d_; }

	// Editor messages
	unsigned      numEditorMessages() const { return editor_messages_.size(); }
	const string& editorMessage(int index);
	long          editorMessageTime(int index) const;
	void          addEditorMessage(string_view message);

	// Feature help text
	const vector<string>& featureHelpLines() const { return feature_help_lines_; }
	void                  setFeatureHelp(const vector<string>& lines);

	// Undo/Redo
	void beginUndoRecord(string_view name, bool mod = true, bool create = true, bool del = true);
	void beginUndoRecordLocked(string_view name, bool mod = true, bool create = true, bool del = true);
	void endUndoRecord(bool success = true);
	void recordPropertyChangeUndoStep(MapObject* object) const;
	void doUndo();
	void doRedo();
	void resetLastUndoLevel() { last_undo_level_ = ""; }

	// Overlays
	MCOverlay* currentOverlay() const { return overlay_current_.get(); }
	bool       overlayActive() const;
	void       closeCurrentOverlay(bool cancel = false) const;
	void       openSectorTextureOverlay(const vector<MapSector*>& sectors);
	void       openQuickTextureOverlay();
	void       openLineTextureOverlay();
	bool       infoOverlayActive() const { return info_showing_; }
	void       updateInfoOverlay() const;
	void       drawInfoOverlay(gl::draw2d::Context& dc, float alpha) const;

	// Player start swapping
	void swapPlayerStart3d();
	void swapPlayerStart2d(const Vec2d& pos);
	void resetPlayerStart() const;

	// Renderer
	Camera& camera3d() const;

	// Misc
	string modeString(bool plural = true) const;
	bool   handleKeyBind(string_view key, Vec2d position);
	void   updateDisplay() const;
	void   updateStatusText() const;
	void   updateThingLists();
	void   setCursor(ui::MouseCursor cursor) const;
	void   forceRefreshRenderer();


	// SAction handler
	bool handleAction(string_view id) override;

private:
	unique_ptr<SLADEMap> map_;
	MapCanvas*           canvas_ = nullptr;
	Archive::MapDesc     map_desc_;
	long                 next_frame_length_ = 0;

	// Undo/Redo stuff
	unique_ptr<UndoManager> undo_manager_     = nullptr;
	unique_ptr<UndoStep>    us_create_delete_ = nullptr;

	// Editor state
	mapeditor::Mode           edit_mode_      = mapeditor::Mode::Lines;
	mapeditor::Mode           edit_mode_prev_ = mapeditor::Mode::Lines;
	unique_ptr<ItemSelection> selection_;
	int                       grid_size_    = 9;
	mapeditor::SectorMode     sector_mode_  = mapeditor::SectorMode::Both;
	bool                      grid_snap_    = true;
	int                       current_tag_  = 0;
	bool                      mouse_locked_ = false;

	// Undo/Redo
	bool   undo_modified_ = false;
	bool   undo_created_  = false;
	bool   undo_deleted_  = false;
	string last_undo_level_;

	// Tagged items
	vector<MapSector*> tagged_sectors_;
	vector<MapLine*>   tagged_lines_;
	vector<MapThing*>  tagged_things_;

	// Tagging items
	vector<MapLine*>  tagging_lines_;
	vector<MapThing*> tagging_things_;

	// Pathed things
	vector<MapThing*> pathed_things_;

	// Editing
	unique_ptr<MoveObjects> move_objects_;
	unique_ptr<LineDraw>    line_draw_;
	unique_ptr<Edit2D>      edit_2d_;
	unique_ptr<Edit3D>      edit_3d_;
	unique_ptr<ObjectEdit>  object_edit_;

	// Object properties and copy/paste
	unique_ptr<MapThing>  copy_thing_      = nullptr;
	unique_ptr<MapSector> copy_sector_     = nullptr;
	unique_ptr<MapSide>   copy_side_front_ = nullptr;
	unique_ptr<MapSide>   copy_side_back_  = nullptr;
	unique_ptr<MapLine>   copy_line_       = nullptr;

	// Editor messages
	vector<EditorMessage> editor_messages_;

	// Feature help text
	vector<string> feature_help_lines_;

	// Player start swap
	Vec2d player_start_pos_;
	int   player_start_dir_ = 0;

	// Renderer
	unique_ptr<mapeditor::Renderer> renderer_;

	// Input
	unique_ptr<mapeditor::Input> input_;

	// Full-Screen Overlay
	unique_ptr<MCOverlay> overlay_current_ = nullptr;

	// Info overlays
	bool                          info_showing_ = false;
	unique_ptr<VertexInfoOverlay> info_vertex_;
	unique_ptr<LineInfoOverlay>   info_line_;
	unique_ptr<SectorInfoOverlay> info_sector_;
	unique_ptr<ThingInfoOverlay>  info_thing_;
	unique_ptr<InfoOverlay3D>     info_3d_;
};
} // namespace slade
