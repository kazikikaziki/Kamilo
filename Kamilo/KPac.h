#pragma once
#include <memory>
#include "KString.h"

namespace Kamilo {

class KInputStream;
class KOutputStream;
class CPacWriterImpl; // internal
class CPacReaderImpl; // internal

/// ゲーム用のアーカイブファイル
class KPacFileWriter {
public:
	static KPacFileWriter fromFileName(const char *filename);
	static KPacFileWriter fromStream(KOutputStream &output);

	KPacFileWriter();
	bool isOpen();
	bool addEntryFromFileName(const KPath &entry_name, const KPath &filename);
	bool addEntryFromMemory(const KPath &entry_name, const void *data, size_t size);
private:
	std::shared_ptr<CPacWriterImpl> m_Impl;
};

/// ゲーム用アーカイブファイル
class KPacFileReader {
public:
	static KPacFileReader fromFileName(const char *filename);
	static KPacFileReader fromStream(KInputStream &input);

	KPacFileReader();
	bool isOpen();
	int getCount();
	int getIndexByName(const KPath &entry_name, bool ignore_case, bool ignore_path);
	KPath getName(int index);
	std::string getData(int index);
private:
	std::shared_ptr<CPacReaderImpl> m_Impl;
};

} // namespace
