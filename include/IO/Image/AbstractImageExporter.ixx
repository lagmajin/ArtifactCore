module;


export module IO.Image.Abstract;


export namespace ArtifactCore {

 enum class ImageExporterBackend {
  AutoDetect, // 拡張子などから自動判別
  OIIO,
  Qt,
  Stb
 };

 class AbstractImageExporter {

 };






};