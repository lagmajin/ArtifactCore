module;

#include <string>

export module Graphics.PSO.Cache;

import Utils.String.UniString;

export namespace ArtifactCore
{
// Avoid default argument with temporary. Use overload or static const.
UniString getPSOCacheDirectory(const UniString& vendor,
                               const UniString& deviceName,
                               const UniString& cacheFolder,
                               bool createIfMissing = true);

inline UniString getPSOCacheDirectory(const UniString& vendor,
                                      const UniString& deviceName,
                                      bool createIfMissing = true) {
    return getPSOCacheDirectory(vendor, deviceName, UniString(std::string("PSO")), createIfMissing);
}
}
