#include "KStream.h"
#include "KInternal.h"
#include "KFile.h"
namespace Kamilo {


class CReaderStrm: public KInputStream::Impl {
public:
	KReader *m_Reader;
	CReaderStrm(KReader *r) {
		m_Reader = r;
		K_Grab(m_Reader);
	}
	virtual ~CReaderStrm() {
		K_Drop(m_Reader);
	}
	virtual int read(void *buf, int size) {
		return m_Reader->read(buf, size);
	}
	virtual int tell() {
		return m_Reader->tell();
	}
	virtual int size() {
		return m_Reader->size();
	}
	virtual void seek(int pos) {
		return m_Reader->seek(pos);
	}
	virtual bool eof() {
		return tell() >= size();
	}
	virtual void close() {
		K_Drop(m_Reader);
	}
	virtual bool isOpen() {
		return m_Reader != nullptr;
	}
};

class CWriterStrm: public KOutputStream::Impl {
public:
	KWriter *m_Writer;
	CWriterStrm(KWriter *w) {
		m_Writer = w;
		K_Grab(m_Writer);
	}
	virtual ~CWriterStrm() {
		K_Drop(m_Writer);
	}
	virtual int tell() {
		return m_Writer->tell();
	}
	virtual int write(const void *data, int size) {
		return m_Writer->write(data, size);
	}
	virtual void seek(int pos) {
		assert(0);
	}
	virtual void close() {
		K_Drop(m_Writer);
	}
	virtual bool isOpen() {
		return m_Writer != nullptr;
	}
};







#pragma region File Impl
class CFileReadImpl: public KInputStream::Impl {
public:
	FILE *m_file;
	std::string m_name;

	CFileReadImpl(FILE *fp, const std::string &name) {
		m_file = fp;
		m_name = name;
	}
	virtual ~CFileReadImpl() {
		if (m_file) {
			fclose(m_file);
		}
	}
	virtual int tell() override {
		return ftell(m_file);
	}
	virtual int read(void *data, int size) override {
		if (data) {
			return fread(data, 1, size, m_file);
		} else {
			return fseek(m_file, size, SEEK_CUR);
		}
	}
	virtual void seek(int pos) override {
		fseek(m_file, pos, SEEK_SET);
	}
	virtual int size() {
		int i=ftell(m_file);
		fseek(m_file, 0, SEEK_END);
		int n=ftell(m_file);
		fseek(m_file, i, SEEK_SET);
		return n; 
	}
	virtual bool eof() override {
		return feof(m_file) != 0;
	}
	virtual void close() override {
		if (m_file) {
			fclose(m_file);
			m_file = nullptr;
		}
	}
	virtual bool isOpen() override {
		return m_file != nullptr;
	}
};
class CFileWriteImpl: public KOutputStream::Impl {
public:
	FILE *m_file;
	std::string m_name;

	CFileWriteImpl(FILE *fp, const std::string &name) {
		m_file = fp;
		m_name = name;
	}
	virtual ~CFileWriteImpl() {
		if (m_file) {
			fclose(m_file);
		}
	}
	virtual int tell() override {
		return ftell(m_file);
	}
	virtual int write(const void *data, int size) override {
		return fwrite(data, 1, size, m_file);
	}
	virtual void seek(int pos) override {
		fseek(m_file, pos, SEEK_SET);
	}
	virtual void close() override {
		if (m_file) {
			fclose(m_file);
			m_file = nullptr;
		}
	}
	virtual bool isOpen() override {
		return m_file != nullptr;
	}
};
#pragma endregion // File Impl


#pragma region Memory Impl
class CMemoryReadImpl: public KInputStream::Impl {
	void *m_ptr;
	int m_size;
	int m_pos;
	bool m_copy;
public:
	CMemoryReadImpl(const void *p, int size, bool copy) {
		if (copy) {
			m_ptr = malloc(size);
			memcpy(m_ptr, p, size);
			m_copy = true;
		} else {
			m_ptr = const_cast<void*>(p);
			m_copy = false;
		}
		m_size = size;
		m_pos = 0;
	}
	virtual ~CMemoryReadImpl() {
		if (m_copy && m_ptr) {
			free(m_ptr);
		}
	}
	virtual int tell() override {
		return m_pos;
	}
	virtual int read(void *data, int size) override {
		if (m_ptr == nullptr) return 0;
		int n = 0;
		if (m_pos + size <= m_size) {
			n = size;
		} else if (m_pos < m_size) {
			n = m_size - m_pos;
		}
		if (n > 0) {
			if (data) memcpy(data, (uint8_t*)m_ptr + m_pos, n); // data=nullptr だと単なるシークになる
			m_pos += n;
			return n;
		}
		return 0;
	}
	virtual void seek(int pos) override {
		if (pos < 0) {
			m_pos = 0;
		} else if (pos < m_size) {
			m_pos = pos;
		} else {
			m_pos = m_size;
		}
	}
	virtual int size() override {
		return m_size;
	}
	virtual bool eof() override {
		return m_pos >= m_size;
	}
	virtual void close() override {
		m_ptr = nullptr;
		m_size = 0;
		m_pos = 0;
		m_copy = false;
	}
	virtual bool isOpen() override {
		return m_ptr != nullptr;
	}
};
class CMemoryWriteImpl: public KOutputStream::Impl {
	std::string *m_buf;
	int m_pos;
public:
	explicit CMemoryWriteImpl(std::string *dest) {
		m_buf = dest;
		m_pos = 0;
	}
	virtual int tell() override {
		return m_pos;
	}
	virtual int write(const void *data, int size) {
		if (m_buf == nullptr) return 0;
		m_buf->resize(m_pos + size);
		memcpy((char*)m_buf->data() + m_pos, data, size);
		m_pos += size;
		return size;
	}
	virtual void seek(int pos) override {
		if (m_buf == nullptr) return;
		if (pos < 0) {
			m_pos = 0;
		} else if (pos < (int)m_buf->size()) {
			m_pos = pos;
		} else {
			m_pos = m_buf->size();
		}
	}
	virtual void close() override {
		m_buf = nullptr;
		m_pos = 0;
	}
	virtual bool isOpen() override {
		return m_buf != nullptr;
	}
};
#pragma endregion // Memory Impl


#pragma region KInputStream
KInputStream KInputStream::fromFileName(const std::string &filename) {
	Impl *impl = nullptr;
	FILE *fp = K__fopen_u8(filename.c_str(), "rb");
	if (fp) {
		impl = new CFileReadImpl(fp, filename);
	}
	return KInputStream(impl);
}
KInputStream KInputStream::fromMemory(const void *data, int size) {
	Impl *impl = nullptr;
	if (data && size > 0) {
		impl = new CMemoryReadImpl(data, size, false); // No copy
	}
	return KInputStream(impl);
}
KInputStream KInputStream::fromMemoryCopy(const void *data, int size) {
	Impl *impl = nullptr;
	if (data && size > 0) {
		impl = new CMemoryReadImpl(data, size, true); // Copy
	}
	return KInputStream(impl);
}
KInputStream KInputStream::fromReader(KReader *r) {
	Impl *impl = nullptr;
	if (r) {
		impl = new CReaderStrm(r);
	}
	return KInputStream(impl);
}

KInputStream::KInputStream() {
	m_Impl = nullptr;
}
KInputStream::KInputStream(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
KInputStream::~KInputStream() {
	// ここでクローズしてはいけない. 
	// std::shared_ptr によって参照が自動カウントされているので
	// ここで参照がゼロになるとは限らない
	// close();
}
int KInputStream::tell() {
	if (m_Impl) {
		return m_Impl->tell();
	}
	return 0;
}
int KInputStream::read(void *data, int size) {
	if (m_Impl) {
		return m_Impl->read(data, size);
	}
	return 0;
}
void KInputStream::seek(int pos) {
	if (m_Impl) {
		m_Impl->seek(pos);
	}
}
int KInputStream::size() {
	if (m_Impl) {
		return m_Impl->size();
	}
	return 0;
}
bool KInputStream::eof() {
	return m_Impl && m_Impl->eof();
}
void KInputStream::close() {
	if (m_Impl) {
		m_Impl->close();
	}
}
bool KInputStream::isOpen() {
	return m_Impl && m_Impl->isOpen();
}
uint16_t KInputStream::readUint16() {
	const int size = 2;
	uint16_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
uint32_t KInputStream::readUint32() {
	const int size = 4;
	uint32_t val = 0;
	if (read(&val, size) == size) {
		return val;
	}
	return 0;
}
std::string KInputStream::readBin(int readsize) {
	if (readsize < 0) {
		readsize = size(); // 現在位置から終端までのサイズ
	}
	if (readsize > 0) {
		std::string bin(readsize, '\0');
		int sz = read((void*)bin.data(), bin.size());
		bin.resize(sz);// bin.size() が実際のデータ長さを示すように
		return bin;
	}
	return std::string(); // 読み取りサイズゼロ
}
#pragma endregion // KInputStream


#pragma region KOutputStream
KOutputStream KOutputStream::fromFileName(const std::string &filename) {
	Impl *impl = nullptr;
	FILE *fp = K__fopen_u8(filename.c_str(), "wb");
	if (fp) {
		impl = new CFileWriteImpl(fp, filename);
	}
	return KOutputStream(impl);
}
KOutputStream KOutputStream::fromMemory(std::string *dest) {
	Impl *impl = new CMemoryWriteImpl(dest); // No copy
	return KOutputStream(impl);
}
KOutputStream KOutputStream::fromWriter(KWriter *w) {
	Impl *impl = new CWriterStrm(w);
	return KOutputStream(impl);
}

KOutputStream::KOutputStream() {
	m_Impl = nullptr;
}
KOutputStream::KOutputStream(Impl *impl) {
	if (impl) {
		m_Impl = std::shared_ptr<Impl>(impl);
	} else {
		m_Impl = nullptr;
	}
}
KOutputStream::~KOutputStream() {
	// ここでクローズしてはいけない. 
	// std::shared_ptr によって参照が自動カウントされているので
	// ここで参照がゼロになるとは限らない
	// close();
}
void KOutputStream::close() {
	if (m_Impl) {
		m_Impl->close();
	}
}
bool KOutputStream::isOpen() {
	return m_Impl && m_Impl->isOpen();
}
int KOutputStream::tell() {
	if (m_Impl) {
		return m_Impl->tell();
	}
	return 0;
}
void KOutputStream::seek(int pos) {
	if (m_Impl) {
		m_Impl->seek(pos);
	}
}
int KOutputStream::write(const void *data, int size) {
	if (m_Impl) {
		return m_Impl->write(data, size);
	}
	return 0;
}
int KOutputStream::writeUint16(uint16_t value) {
	return write(&value, sizeof(value));
}
int KOutputStream::writeUint32(uint32_t value) {
	return write(&value, sizeof(value));
}
int KOutputStream::writeString(const std::string &s) {
	if (s.empty()) {
		return 0;
	} else {
		return write(s.data(), s.size());
	}
}
#pragma endregion // KOutputStream


} // namespace
