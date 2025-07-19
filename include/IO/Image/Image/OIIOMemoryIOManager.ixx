module;
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <vector>
#include <memory>
#include <iostream>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp> 

#include <array>

//namespace OIIO {};

export module Image.Exporter.Memory.OIIO;

export namespace ArtifactCore {

 using namespace OIIO;

 using IOProxy = OIIO::Filesystem::IOProxy;
 
 using Mode = OIIO::Filesystem::IOProxy::Mode;


 class MemoryProxy : public OIIO::Filesystem::IOProxy {
 private:
  std::vector<char> m_buffer;
  bool opend_ = false;
  int64_t m_pos=0;
 public:
  MemoryProxy() :IOProxy()
  {

  }
  MemoryProxy(const std::string &file,IOProxy::Mode mode) :IOProxy()
  {
   


  }


  virtual ~MemoryProxy()=default;






  const char* proxytype() const override
  {
   return "mem_io_proxy";
  }


  void close() override
  {
   //std::cout << "MyMemoryIOProxy::close() called for: " << m_name << std::endl;
  }


  bool opened() const override
  {
   return opend_;
  }

 

  void flush() override
  {
  
  }


  size_t write(const void* buf, size_t size) override
  {
   // `pwrite` と同様に、`m_buffer.resize()` による競合を避けるためのミューテックスが必要
	// ただし、通常の write() と pwrite() が同時に呼ばれる可能性をどう扱うかによる。
	// OIIOのコメント「pread/pwriteは互いにスレッドセーフだが、IOProxyの他の関数とはそうではない」
	// を考慮すると、`m_pwrite_mutex` とは別の `m_write_mutex` を使うか、
	// あるいは両方を保護する単一のより大きなロックが必要になるかもしれない。
	//
	// 最もシンプルなのは、全ての書き込み操作（write, pwrite）を単一のミューテックスで保護すること。
	// ここでは、pwrite() で追加した m_pwrite_mutex を流用して m_buffer へのアクセスを保護します。
	// (名前は m_buffer_access_mutex など、より汎用的な方が良いかもしれません)
   //std::lock_guard<std::mutex> lock(m_pwrite_mutex); // m_bufferへのアクセスを保護

   // バッファのサイズが足りなければ拡張
   // 現在の書き込み位置 (m_pos) から `size` バイト書き込むために、バッファを拡張する
   if (m_pos + size > m_buffer.size()) {
	m_buffer.resize(static_cast<size_t>(m_pos + size));
   }

   // データをコピー
   // m_pos からデータを書き込む
   std::memcpy(m_buffer.data() + m_pos, buf, size);

   // 書き込み後に現在のファイル位置 (m_pos) を進める
   m_pos += size;

   // std::cout << "MyMemoryIOProxy::write: Wrote " << size << " bytes for '" << m_name << "'. New pos: " << m_pos << std::endl;
   return size;
  }


  int64_t tell() const override
  {
   return m_pos;
  }


  bool seek(int64_t offset) override
  {
   if (offset < 0) {
	std::cerr << "MyMemoryIOProxy::seek: Negative offset provided (" << offset << ") for '" << "" << "'" << std::endl;
	return false; // 無効なオフセットなので失敗
   }

   // 2. 現在のファイル位置 (m_pos) を直接更新
   // このメソッドは「ストリームの先頭からの絶対オフセット」として解釈される
   m_pos = offset;

   // 3. 成功を返す
   // OIIOの他のAPIが、このシーク結果に対して、読み書き可能な範囲を超えたシークを行った場合に
   // どのように振る舞うかを期待するかによって、追加のチェックが必要になる場合がある。
   // 例えば、m_pos が m_buffer.size() を超える場合は false を返す、など。
   // しかし、一般的に書き込み可能なストリームでは、末尾を超えたシークは許容され、
   // その後の書き込みによってバッファが拡張されることを期待する。
   // 読み込み専用のストリームであれば、m_buffer.size() を超える場合は false を返すべき。
   // 今回は「書き込み専用」IOProxyなので、m_buffer.size() を超えるシークは許容される。

   // std::cout << "MyMemoryIOProxy::seek: Seeked to " << m_pos << " for '" << m_name << "'" << std::endl;
   return true; // 正常にシークが完了
  }


  size_t read(void* buf, size_t size) override
  {
   return 0;
  }


  size_t pwrite(const void* buf, size_t size, int64_t offset) override
  {
   if (offset < 0) {
	//std::cerr << "MyMemoryIOProxy::pwrite: Negative offset provided for '" << m_name << "'" << std::endl;
	return 0;
   }

   // offset + size が現在のバッファサイズを超える場合
   if (offset + size > m_buffer.size()) {
	m_buffer.resize(static_cast<size_t>(offset + size));
   }
   std::memcpy(m_buffer.data() + offset, buf, size);
  }


  size_t size() const override
  {
   throw std::logic_error("The method or operation is not implemented.");
  }


  size_t pread(void* buf, size_t size, int64_t offset) override
  {
   return 0;
  }

 };




};