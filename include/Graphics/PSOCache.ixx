module;

export module Graphics.PSO.Cache;

import Utils.String.UniString;

export namespace ArtifactCore
{
 UniString getPSOCacheDirectory(const UniString& vendor,
                               const UniString& deviceName,
                               const UniString& cacheFolder = UniString("PSO"),
                               bool createIfMissing = true);
}
