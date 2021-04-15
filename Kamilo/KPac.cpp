﻿#include "KPac.h"
//
#include <mutex>
#include "KInternal.h"
#include "KZlib.h"

namespace Kamilo {


const int PAC_COMPRESS_LEVEL = 1;
const int PAC_MAX_LABEL_LEN  = 128; // 128以外にすると昔の pac データが読めなくなるので注意


#pragma region KPacFileWriter
class CPacWriterImpl {
	KOutputStream output_;
public:
	CPacWriterImpl() {
	}
	bool open(KOutputStream &output) {
		output_ = output;
		return false;
	}
	virtual bool addEntryFromFileName(const KPath &entry_name, const KPath &filename) {
		KReader *file = KReader::createFromFileName(filename.u8());
		if (file == nullptr) {
			K__Error(u8"E_PAC_WRITE: ファイル '%s' をロードできないため pac ファイルに追加しませんでした", filename.u8());
			return false;
		}
		std::string bin = file->read_bin();
		file->drop();
		return addEntryFromMemory(entry_name, bin.data(), bin.size());
	}
	bool addEntryFromMemory(const KPath &entry_name, const void *data, size_t size) {
		
		// エントリー名を書き込む。固定長で、XORスクランブルをかけておく
		{
			const char *u8 = entry_name.u8();
			if (strlen(u8) >= PAC_MAX_LABEL_LEN) {
				K__Error(u8"ラベル名 '%s' が長すぎます", u8);
				return false;
			}
			char label[PAC_MAX_LABEL_LEN];
			memset(label, 0, PAC_MAX_LABEL_LEN);
			strcpy_s(label, sizeof(label), entry_name.u8());
			// '\0' 以降の文字は完全に無視される部分なのでダミーの乱数を入れておく
			for (int i=strlen(u8)+1; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = rand() & 0xFF;
			}
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				label[i] = label[i] ^ i;
			}
			output_.write(label, PAC_MAX_LABEL_LEN);
		}
		if (data == nullptr || size <= 0) {
			// nullptrデータ
			// Data size in file
			output_.writeUint32(0); // Hash
			output_.writeUint32(0); // 元データサイズ
			output_.writeUint32(0); // pacファイル内でのデータサイズ
			output_.writeUint32(0); // Flags

		} else {
			// 圧縮データ
			std::string zbuf = KZlib::compress_zlib(data, size, PAC_COMPRESS_LEVEL);
			output_.writeUint32(0); // Hash
			output_.writeUint32(size); // 元データサイズ
			output_.writeUint32(zbuf.size()); // pacファイル内でのデータサイズ
			output_.writeUint32(0); // Flags
			output_.write(zbuf.data(), zbuf.size());
		}
		return true;
	}
};

KPacFileWriter::KPacFileWriter() {
	m_Impl = nullptr;
}
KPacFileWriter KPacFileWriter::fromFileName(const char *filename) {
	KOutputStream output = KOutputStream::fromFileName(filename);
	return fromStream(output);
}
KPacFileWriter KPacFileWriter::fromStream(KOutputStream &output) {
	KPacFileWriter ret;
	if (output.isOpen()) {
		CPacWriterImpl *pac = new CPacWriterImpl();
		if (pac->open(output)) {
			ret.m_Impl = std::shared_ptr<CPacWriterImpl>(pac);
		} else {
			delete pac;
		}
	}
	return ret;
}
bool KPacFileWriter::isOpen() {
	return m_Impl != nullptr;
}
bool KPacFileWriter::addEntryFromFileName(const KPath &entry_name, const KPath &filename) {
	if (m_Impl) {
		return m_Impl->addEntryFromFileName(entry_name, filename);
	}
	return false;
}
bool KPacFileWriter::addEntryFromMemory(const KPath &entry_name, const void *data, size_t size) {
	if (m_Impl) {
		return m_Impl->addEntryFromMemory(entry_name, data, size);
	}
	return false;
}
#pragma endregion//  KPacFileWriter


#pragma region KPacFileReader
class CPacReaderImpl {
	std::mutex mutex_;
	KInputStream file_;
public:
	CPacReaderImpl() {
	}
	~CPacReaderImpl() {
		mutex_.lock();
		file_ = KInputStream(); // スレッドセーフでデストラクタが実行されるように、あえて空オブジェクトを代入しておく
		mutex_.unlock();
	}
	int getCount() {
		int ret = 0;
		mutex_.lock();
		{
			ret = getCount_unsafe();
		}
		mutex_.unlock();
		return ret;
	}
	int getIndexByName(const KPath &entry_name, bool ignore_case, bool ignore_path) {
		int ret = 0;
		mutex_.lock();
		{
			ret = getIndexByName_unsafe(entry_name, ignore_case, ignore_path);
		}
		mutex_.unlock();
		return ret;
	}
	KPath getName(int index) {
		KPath ret;
		mutex_.lock();
		{
			ret = getName_unsafe(index);
		}
		mutex_.unlock();
		return ret;
	}
	std::string getData(int index) {
		std::string ret;
		mutex_.lock();
		{
			ret = getData_unsafe(index);
		}
		mutex_.unlock();
		return ret;
	}
	bool open(KInputStream &input) {
		mutex_.lock();
		file_ = input;
		mutex_.unlock();
		return file_.isOpen();
	}
	void seekFirst_unsafe() {
		file_.seek(0);
	}
	bool seekNext_unsafe(KPath *name) {
		if (file_.tell() >= file_.size()) {
			return false;
		}
		uint32_t len = 0;
		file_.read(nullptr, PAC_MAX_LABEL_LEN); // Name
		file_.read(nullptr, 4); // Hash
		file_.read(nullptr, 4); // Data size
		file_.read(&len, 4); // Data size in pac file
		file_.read(nullptr, 4); // Flags
		file_.read(nullptr, len); // Data
		return true;
	}
	bool readFile_unsafe(KPath *name, std::string *data) {
		if (file_.tell() >= file_.size()) {
			return false;
		}
		if (name) {
			char s[PAC_MAX_LABEL_LEN];
			file_.read(s, PAC_MAX_LABEL_LEN);
			for (uint8_t i=0; i<PAC_MAX_LABEL_LEN; i++) {
				s[i] = s[i] ^ i;
			}
			*name = KPath::fromUtf8(s);
		} else {
			file_.read(nullptr, PAC_MAX_LABEL_LEN);
		}

		// Hash (NOT USE)
		uint32_t hash = file_.readUint32();

		// Data size (NOT USE)
		uint32_t datasize_orig = file_.readUint32();
		if (datasize_orig >= 1024 * 1024 * 100) { // 100MBはこえないだろう
			K__Error("too big datasize_orig size");
			return false;
		}

		// Data size in pac file
		uint32_t datasize_inpac = file_.readUint32();
		if (datasize_inpac >= 1024 * 1024 * 100) { // 100MBはこえないだろう
			K__Error("too big datasize_inpac size");
			return false;
		}

		// Flags (NOT USE)
		uint32_t flags = file_.readUint32();

		// Read data
		if (data) {
			if (datasize_orig > 0) {
				std::string zdata = file_.readBin(datasize_inpac);
				*data = KZlib::uncompress_zlib(zdata, datasize_orig);
				if (data->size() != datasize_orig) {
					K__Error("E_PAC_DATA_SIZE_NOT_MATCHED");
					data->clear();
					return false;
				}
			} else {
				*data = std::string();
			}
		} else {
			file_.read(nullptr, datasize_inpac);
		}
		return true;
	}
	int getCount_unsafe() {
		int num = 0;
		seekFirst_unsafe();
		while (seekNext_unsafe(nullptr)) {
			num++;
		}
		return num;
	}
	int getIndexByName_unsafe(const KPath &entry_name, bool ignore_case, bool ignore_path) {
		seekFirst_unsafe();
		int idx = 0;
		KPath name;
		while (readFile_unsafe(&name, nullptr)) {
			if (name.compare(entry_name, ignore_case, ignore_path) == 0) {
				return idx;
			}
#ifdef _DEBUG
			if (name.compare(entry_name, true, false) == 0) { // ignore case で一致した
				if (name.compare(entry_name, false, false) != 0) { // case sensitive で不一致になった
					K__Print(
						u8"W_PAC_CASE_NAME: PACファイル内をファイル名 '%s' で検索中に、"
						u8"大小文字だけが異なるファイル '%s' を発見しました。"
						u8"これは意図した動作ですか？予期せぬ不具合の原因になるため、ファイル名の変更を強く推奨します",
						entry_name.u8(), name.u8()
					);
				}
			}
#endif
			idx++;
		}
		return -1;
	}
	KPath getName_unsafe(int index) {
		seekFirst_unsafe();
		for (int i=1; i<index; i++) {
			if (!seekNext_unsafe(nullptr)) {
				return KPath::Empty;
			}
		}
		KPath name;
		if (readFile_unsafe(&name, nullptr)) {
			return name;
		}
		return KPath::Empty;
	}
	std::string getData_unsafe(int index) {
		seekFirst_unsafe();
		for (int i=0; i<index; i++) {
			if (!seekNext_unsafe(nullptr)) {
				return false;
			}
		}
		std::string data;
		if (readFile_unsafe(nullptr, &data)) {
			return data;
		}
		return std::string();
	}
};

KPacFileReader::KPacFileReader() {
	m_Impl = nullptr;
}
KPacFileReader KPacFileReader::fromFileName(const char *filename) {
	KInputStream input = KInputStream::fromFileName(filename);
	return fromStream(input);
}
KPacFileReader KPacFileReader::fromStream(KInputStream &input) {
	KPacFileReader ret;
	if (input.isOpen()) {
		CPacReaderImpl *pac = new CPacReaderImpl();
		if (pac->open(input)) {
			ret.m_Impl = std::shared_ptr<CPacReaderImpl>(pac);
		} else {
			delete pac;
		}
	}
	return ret;
}
bool KPacFileReader::isOpen() {
	return m_Impl != nullptr;
}
int KPacFileReader::getCount() {
	if (m_Impl) {
		return m_Impl->getCount();
	}
	return 0;
}
int KPacFileReader::getIndexByName(const KPath &entry_name, bool ignore_case, bool ignore_path) {
	if (m_Impl) {
		return m_Impl->getIndexByName(entry_name, ignore_case, ignore_path);
	}
	return 0;
}
KPath KPacFileReader::getName(int index) {
	if (m_Impl) {
		return m_Impl->getName(index);
	}
	return "";
}
std::string KPacFileReader::getData(int index) {
	if (m_Impl) {
		return m_Impl->getData(index);
	}
	return "";
}
#pragma endregion // KPacFileReader

} // namespace
