

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>

#include <vlc/vlc.h>

#include "../../include/Video/LibVLCDecoder.hpp"


namespace ArtifactCore {

 static void* lock(void* opaque, void** planes) {

 }

 static void unlock(void* opaque, void* picture, void* const* planes) {

 }

 static void display(void* opaque, void* picture) {

 }
 class LibVLCDecoderPrivate {
 private: 
	   struct InstanceDeleter {
		void operator()(libvlc_instance_t* ptr) const {
		 libvlc_release(ptr);
		 }
	    };

		struct MediaPlayerDeleter {
		 void operator()(libvlc_media_player_t* ptr) const {
		  libvlc_media_player_release(ptr);
		 }
		};

		struct MediaDeleter {
		 void operator()(libvlc_media_t* ptr) const {
		  libvlc_media_release(ptr);
		 }
		};
		using LibVlcInstancePtr = std::unique_ptr<libvlc_instance_t, InstanceDeleter>;
		using MediaPtr = std::unique_ptr<libvlc_media_t, MediaDeleter>;
		using MediaPlayerPtr = std::unique_ptr<libvlc_media_player_t, MediaPlayerDeleter>;

		LibVlcInstancePtr instance_=nullptr;
		MediaPtr  media_ = nullptr;
		MediaPlayerPtr player_ = nullptr;
 public:
  bool initialize();
  void loadFromFile(const QFile& file);
  void seekiAtTime();
 };

 bool LibVLCDecoderPrivate::initialize()
 {
  auto instance= libvlc_new(0, nullptr);
  
  if (instance)
  {
   instance_ = LibVlcInstancePtr(instance);
  
   return true;
  }
  

  return false;
 }

 void LibVLCDecoderPrivate::loadFromFile(const QFile& file)
 {
  if (file.exists())
  {
   auto filename = file.fileName().toUtf8();

   auto media = libvlc_media_new_path(instance_.get(), filename);
  
   if (media)
   {
	media_ = MediaPtr(media);
  }

  }
  

 }

 void LibVLCDecoderPrivate::seekiAtTime()
 {



 }

 LibVLCDecoder::LibVLCDecoder()
 {

 }

 LibVLCDecoder::~LibVLCDecoder()
 {

 }

 void LibVLCDecoder::loadFromFile(const QFile& file)
 {
  throw std::logic_error("The method or operation is not implemented.");
 }

 void LibVLCDecoder::seekiAtTime()
 {

 }

};

