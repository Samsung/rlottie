/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "lottieloader.h"
#include "lottieparser.h"

#include <cstring>
#include <fstream>

#ifdef LOTTIE_CACHE_SUPPORT

#include <unordered_map>
#include <mutex>

class LottieModelCache {
public:
    static LottieModelCache &instance()
    {
        static LottieModelCache CACHE;
        return CACHE;
    }
    std::shared_ptr<LOTModel> find(const std::string &key)
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if (!mcacheSize) return nullptr;

        auto search = mHash.find(key);

        return (search != mHash.end()) ? search->second : nullptr;

    }
    void add(const std::string &key, std::shared_ptr<LOTModel> value)
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if (!mcacheSize) return;

        //@TODO just remove the 1st element
        // not the best of LRU logic
        if (mcacheSize == mHash.size()) mHash.erase(mHash.cbegin());

        mHash[key] = std::move(value);
    }

    void configureCacheSize(size_t cacheSize)
    {
        std::lock_guard<std::mutex> guard(mMutex);
        mcacheSize = cacheSize;

        if (!mcacheSize) mHash.clear();
    }

private:
    LottieModelCache() = default;

    std::unordered_map<std::string, std::shared_ptr<LOTModel>>  mHash;
    std::mutex                                                  mMutex;
    size_t                                                      mcacheSize{10};
};

#else

class LottieModelCache {
public:
    static LottieModelCache &instance()
    {
        static LottieModelCache CACHE;
        return CACHE;
    }
    std::shared_ptr<LOTModel> find(const std::string &) { return nullptr; }
    void add(const std::string &, std::shared_ptr<LOTModel>) {}
    void configureCacheSize(size_t) {}
};

#endif

void LottieLoader::configureModelCacheSize(size_t cacheSize)
{
    LottieModelCache::instance().configureCacheSize(cacheSize);
}

static std::string dirname(const std::string &path)
{
    const char *ptr = strrchr(path.c_str(), '/');
#ifdef _WIN32
    if (ptr) ptr = strrchr(ptr + 1, '\\');
#endif
    int         len = int(ptr + 1 - path.c_str());  // +1 to include '/'
    return std::string(path, 0, len);
}

bool LottieLoader::load(const std::string &path, bool cachePolicy)
{
    if (cachePolicy) {
        mModel = LottieModelCache::instance().find(path);
        if (mModel) return true;
    }

    std::ifstream f;
    f.open(path);

    if (!f.is_open()) {
        vCritical << "failed to open file = " << path.c_str();
        return false;
    } else {
        std::string content;

        std::getline(f, content, '\0') ;
        f.close();

        if (content.empty()) return false;

        const char *str = content.c_str();
        LottieParser parser(const_cast<char *>(str),
                            dirname(path).c_str());
        mModel = parser.model();

        if (!mModel) return false;

        if (cachePolicy)
            LottieModelCache::instance().add(path, mModel);
    }

    return true;
}

bool LottieLoader::loadFromData(
    std::string &&jsonData, const std::string &key,
    const std::string &resourcePath, bool cachePolicy,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>
        &colorReplacements)
{
    if (cachePolicy) {
        mModel = LottieModelCache::instance().find(key);
        if (mModel) return true;
    }

    LottieParser parser(const_cast<char *>(jsonData.c_str()),
                        resourcePath.c_str(), colorReplacements);
    mModel = parser.model();

    if (!mModel) return false;

    if (cachePolicy)
        LottieModelCache::instance().add(key, mModel);

    return true;
}

std::shared_ptr<LOTModel> LottieLoader::model()
{
    return mModel;
}
