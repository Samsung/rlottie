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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "lottieloader.h"
#include "lottieparser.h"

#include <fstream>
#include <unordered_map>
#include <cstring>
using namespace std;

class LottieFileCache {
public:
    static LottieFileCache &get()
    {
        static LottieFileCache CACHE;

        return CACHE;
    }
    std::shared_ptr<LOTModel> find(const std::string &key);
    void add(const std::string &key, std::shared_ptr<LOTModel> value);

private:
    LottieFileCache() = default;

    std::unordered_map<std::string, std::shared_ptr<LOTModel>> mHash;
};

std::shared_ptr<LOTModel> LottieFileCache::find(const std::string &key)
{
    auto search = mHash.find(key);
    if (search != mHash.end()) {
        return search->second;
    } else {
        return nullptr;
    }
}

void LottieFileCache::add(const std::string &key, std::shared_ptr<LOTModel> value)
{
    mHash[key] = std::move(value);
}

static std::string dirname(const std::string &path)
{
    const char *ptr = strrchr(path.c_str(), '/');
    int len = int(ptr + 1 - path.c_str()); // +1 to include '/'
    return std::string(path, 0, len);
}

bool LottieLoader::load(const std::string &path)
{
    LottieFileCache &fileCache = LottieFileCache::get();

    mModel = fileCache.find(path);
    if (mModel) return true;

    std::ifstream f;
    f.open(path);

    if (!f.is_open()) {
        vCritical << "failed to open file = " << path.c_str();
        return false;
    } else {
        std::stringstream buf;
        buf << f.rdbuf();

        LottieParser parser(const_cast<char *>(buf.str().data()), dirname(path).c_str());
        mModel = parser.model();
        fileCache.add(path, mModel);

        f.close();
    }

    return true;
}

bool LottieLoader::loadFromData(std::string &&jsonData, const std::string &key, const std::string &resourcePath)
{
    LottieFileCache &fileCache = LottieFileCache::get();

    mModel = fileCache.find(key);
    if (mModel) return true;

    LottieParser parser(const_cast<char *>(jsonData.c_str()), resourcePath.c_str());
    mModel = parser.model();
    fileCache.add(key, mModel);

    return true;
}

std::shared_ptr<LOTModel> LottieLoader::model()
{
    return mModel;
}
