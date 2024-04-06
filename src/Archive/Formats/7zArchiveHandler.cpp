
#include "Main.h"
#include "7zArchiveHandler.h"
#include "App.h"
#include "Archive/Archive.h"
#include "Archive/ArchiveDir.h"
#include "Archive/ArchiveEntry.h"
#include "General/Misc.h"
#include "General/UI.h"
#include "Utility/FileUtils.h"
#include "Utility/StringUtils.h"
#include <archive.h>
#include <archive_entry.h>

using namespace slade;

namespace
{
bool readToMemChunk(archive* archive, MemChunk& mc)
{
	const void* buffer;
	size_t      buf_size;
	int64_t     offset;

	mc.seekFromStart(0);

	while (true)
	{
		auto r = archive_read_data_block(archive, &buffer, &buf_size, &offset);
		if (r == ARCHIVE_EOF)
			break;

		if (r < ARCHIVE_OK)
		{
			mc.clear();
			log::error(archive_error_string(archive));
			return false;
		}

		mc.write(buffer, buf_size);
	}

	return true;
}
} // namespace

bool Zip7ArchiveHandler::open(Archive& archive, string_view filename)
{
	// Open file with libarchive
	struct archive* archive_7z = archive_read_new();
	archive_read_set_format(archive_7z, ARCHIVE_FORMAT_7ZIP); // Only 7z format
	archive_read_support_compression_all(archive_7z);         // Any compression
	if (archive_read_open_filename(archive_7z, string{ filename }.c_str(), 10240) != ARCHIVE_OK)
	{
		global::error = "Unable to open 7zip file";
		return false;
	}

	// Open from libarchive archive
	return open7z(archive, archive_7z);
}

bool Zip7ArchiveHandler::open(Archive& archive, const MemChunk& mc)
{
	// Open 7z file data with libarchive
	struct archive* archive_7z = archive_read_new();
	archive_read_set_format(archive_7z, ARCHIVE_FORMAT_7ZIP); // Only 7z format
	archive_read_support_compression_all(archive_7z);         // Any compression
	if (archive_read_open_memory(archive_7z, mc.data(), mc.size()) != ARCHIVE_OK)
	{
		global::error = "Unable to open 7zip file";
		return false;
	}

	// Open from libarchive archive
	return open7z(archive, archive_7z);
}

bool Zip7ArchiveHandler::write(Archive& archive, MemChunk& mc)
{
	auto temp_file = app::path("sladetemp.7z", app::Dir::Temp);

	// Write to temp file
	auto success = write(archive, temp_file);

	// Import to MemChunk
	if (success)
		mc.importFile(temp_file);

	// Delete temp file
	fileutil::removeFile(temp_file);

	return success;
}

bool Zip7ArchiveHandler::write(Archive& archive, string_view filename)
{
	// Check for entries with duplicate names (not allowed for zips)
	auto all_dirs = archive.rootDir()->allDirectories();
	all_dirs.insert(all_dirs.begin(), archive.rootDir());
	for (const auto& dir : all_dirs)
	{
		if (auto* dup_entry = dir->findDuplicateEntryName())
		{
			global::error = fmt::format("Multiple entries named {} found in {}", dup_entry->name(), dup_entry->path());
			return false;
		}
	}

	// Open 7z file to write to
	struct archive* archive_7z = archive_write_new();
	archive_write_set_format_7zip(archive_7z);
	if (archive_write_open_filename(archive_7z, string{ filename }.c_str()) != ARCHIVE_OK)
	{
		global::error = archive_error_string(archive_7z);
		return false;
	}

	// Write all entries
	vector<ArchiveEntry*> entries;
	archive.putEntryTreeAsList(entries);
	auto entry_7z  = archive_entry_new();
	auto index     = 0;
	auto n_entries = entries.size();
	ui::setSplashProgressMessage("Writing zip entries");
	ui::setSplashProgress(0.0f);
	ui::updateSplash();
	for (auto entry : entries)
	{
		ui::setSplashProgress(index, n_entries);

		auto path = entry->path().append(misc::lumpNameToFileName(entry->name()));
		strutil::removePrefixIP(path, '/');

		// Setup entry info
		archive_entry_set_pathname_utf8(entry_7z, path.c_str());
		archive_entry_set_size(entry_7z, entry->size());
		if (entry->isFolderType())
			archive_entry_set_filetype(entry_7z, AE_IFDIR);
		else
			archive_entry_set_filetype(entry_7z, AE_IFREG);

		// Write to archive
		archive_write_header(archive_7z, entry_7z);
		archive_write_data(archive_7z, entry->rawData(), entry->size());

		archive_entry_clear(entry_7z);

		// Update entry info
		entry->setState(EntryState::Unmodified);
		entry->exProp("ZipIndex") = index++;
	}

	// Clean up
	archive_write_close(archive_7z);
	archive_write_free(archive_7z);
	archive_entry_free(entry_7z);

	ui::setSplashProgressMessage("");

	return true;
}

bool Zip7ArchiveHandler::loadEntryData(Archive& archive, const ArchiveEntry* entry, MemChunk& out)
{
	// Check that the entry has a zip index
	int zip_index;
	if (entry->exProps().contains("ZipIndex"))
		zip_index = entry->exProps().get<int>("ZipIndex");
	else
	{
		log::error("Zip7ArchiveHandler::loadEntryData: Entry {} has no zip entry index!", entry->name());
		return false;
	}

	// Open file with libarchive
	struct archive* archive_7z = archive_read_new();
	archive_read_set_format(archive_7z, ARCHIVE_FORMAT_7ZIP); // Only 7z format
	archive_read_support_compression_all(archive_7z);         // Any compression
	if (archive_read_open_filename(archive_7z, archive.filename().c_str(), 10240) != ARCHIVE_OK)
	{
		log::error("Zip7ArchiveHandler::loadEntryData: Unable to open 7zip file");
		return false;
	}

	// Skip to entry in 7z
	archive_entry* entry_7z;
	archive_read_next_header(archive_7z, &entry_7z);
	for (int index = 1; index <= zip_index; ++index)
		archive_read_next_header(archive_7z, &entry_7z);

	// Read entry data
	out.reSize(archive_entry_size(entry_7z));
	auto success = readToMemChunk(archive_7z, out);

	// Clean up libarchive stuff
	archive_read_close(archive_7z);
	archive_read_free(archive_7z);

	return success;
}

bool Zip7ArchiveHandler::isThisFormat(const MemChunk& mc)
{
	// Just check the signature for now
	return mc.size() > 6 && mc[0] == '7' && mc[1] == 'z' && mc[2] == 0xBC && mc[3] == 0xAF && mc[4] == 0x27
		   && mc[5] == 0x1C;
}

bool Zip7ArchiveHandler::isThisFormat(const string& filename)
{
	SFile    file(filename);
	MemChunk temp(10);
	file.read(temp, 10);

	return isThisFormat(temp);
}


bool Zip7ArchiveHandler::open7z(Archive& archive, struct archive* archive_7z)
{
	// Stop announcements (don't want to be announcing modification due to entries being added etc)
	const ArchiveModSignalBlocker sig_blocker{ archive };

	// Read entries
	MemChunk       data;
	archive_entry* entry_7z;
	auto           index = -1;
	ui::setSplashProgressMessage("Reading 7z data");
	while (true)
	{
		ui::setSplashProgress(-1.0f);

		auto r = archive_read_next_header(archive_7z, &entry_7z);
		index++;

		// Check if we have read all entries
		if (r == ARCHIVE_EOF)
			break;

		// Check for errors/warnings
		if (r == ARCHIVE_FATAL)
		{
			// Fatal error reading archive, abort
			global::error = archive_error_string(archive_7z);
			return false;
		}
		if (r == ARCHIVE_FAILED)
		{
			// Failed reading archive entry, skip
			log::error(archive_error_string(archive_7z));
			continue;
		}
		if (r == ARCHIVE_WARN)
			log::warning(archive_error_string(archive_7z));

		// Get the entry name as a Path (so we can break it up)
		strutil::Path fn(archive_entry_pathname_utf8(entry_7z));

		// Check entry type
		if (archive_entry_filetype(entry_7z) != AE_IFDIR)
		{
			// Create entry
			auto new_entry = std::make_shared<ArchiveEntry>(
				misc::fileNameToLumpName(fn.fileName()), archive_entry_size(entry_7z));

			// Add entry and directory to directory tree
			auto ndir = createDir(archive, fn.path(true));
			ndir->addEntry(new_entry, true);

			// Read entry data
			if (readToMemChunk(archive_7z, data))
				new_entry->importMemChunk(data, 0, new_entry->size());

			// Set entry info
			new_entry->exProp("ZipIndex") = index;
		}
		else
		{
			// Entry is a directory, add it to the directory tree
			createDir(archive, fn.path(true));
		}
	}
	ui::updateSplash();

	// Clean up libarchive stuff
	archive_read_close(archive_7z);
	archive_read_free(archive_7z);

	// Set all entries/directories to unmodified
	vector<ArchiveEntry*> entry_list;
	archive.putEntryTreeAsList(entry_list);
	for (auto& entry : entry_list)
		entry->setState(EntryState::Unmodified);

	// Enable announcements
	sig_blocker.unblock();

	archive.setModified(false);

	ui::setSplashProgressMessage("");

	return true;
}
