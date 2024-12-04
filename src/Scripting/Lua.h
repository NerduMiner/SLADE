#pragma once

struct lua_State;
class wxWindow;

namespace slade
{
namespace lua
{
	bool init();
	void close();

	struct Error
	{
		string type;
		string message;
		int    line_no;
	};
	Error& error();
	void   showErrorDialog(
		  wxWindow*   parent  = nullptr,
		  string_view title   = "Script Error",
		  string_view message = "An error occurred running the script, see details below");

	bool run(const string& program);
	bool runFile(const string& filename);
	bool runArchiveScript(const string& script, Archive* archive);
	bool runEntryScript(const string& script, vector<ArchiveEntry*>& entries);
	bool runMapScript(const string& script, SLADEMap* map);

	lua_State* state();

	wxWindow* currentWindow();
	void      setCurrentWindow(wxWindow* window);
} // namespace lua
} // namespace slade
