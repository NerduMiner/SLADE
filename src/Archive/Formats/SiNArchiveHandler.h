#pragma once

#include "Archive/ArchiveFormatHandler.h"

namespace slade
{
class SiNArchiveHandler : public ArchiveFormatHandler
{
public:
	SiNArchiveHandler() : ArchiveFormatHandler(ArchiveFormat::SiN) {}
	~SiNArchiveHandler() override = default;

	// Opening/writing
	bool open(Archive& archive, const MemChunk& mc, bool detect_types) override; // Open from MemChunk
	bool write(Archive& archive, MemChunk& mc) override;                         // Write to MemChunk

	// Format detection
	bool isThisFormat(const MemChunk& mc) override;
	bool isThisFormat(const string& filename) override;
};
} // namespace slade
