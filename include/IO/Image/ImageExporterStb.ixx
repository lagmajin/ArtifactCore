module;
#include <stb_image.h>
#include <stb_image_write.h>

#include <QVector>
export module IO.Image.Stb;

import std;

export namespace ArtifactCore {

 struct WriteContext {
  QVector<unsigned char> buffer; // std::vector から QVector に変更
 };

 // この関数は stb_image_write から呼び出され、エンコードされたデータを受け取る
 void write_to_memory_callback(void* context, void* data, int size) {
  WriteContext* ctx = static_cast<WriteContext*>(context);
  const unsigned char* bytes = static_cast<const unsigned char*>(data); // const を追加


 }
 




};