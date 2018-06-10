#include <cll/configuration.h>

#include <log.h>
#include <cll/configuration_cache.h>
#include <cll/http/http_error.h>
#include <cll/http/http_client.h>
#include "json_utils.h"

using namespace cll;
using namespace cll::http;

template <>
void ConfigurationProperty<int>::set(nlohmann::json const& json, std::string const& name) {
    auto f = json.find(name);
    if (f == json.end())
        reset();
    else
        set(JsonUtils::asInt(*f));
}

void Configuration::applyFromJson(nlohmann::json const& json) {
    maxEventSizeInBytes.set(json, "MAXEVENTSIZEINBYTES");
    maxEventsPerPost.set(json, "MAXEVENTSPERPOST");
    queueDrainInterval.set(json, "QUEUEDRAININTERVAL");
}

bool Configuration::download(HttpClient& client, ConfigurationCache* cache) {
    Log::trace("Configuration", "Downloading configuration from: %s", url.c_str());

    auto requestStart = std::chrono::system_clock::now();

    std::unique_ptr<HttpRequest> request (client.createRequest());
    request->setUrl(url);

    CachedConfiguration cached;
    bool hasCached = false;
    if (cache)
        hasCached = cache->readFromCache(url, cached);
    if (hasCached && std::chrono::system_clock::now() < cached.expires) {
        importCached(cached);
        return true;
    }

    if (hasCached)
        request->addHeader("If-None-Match", "\"" + cached.etag + "\"");

    HttpResponse response;
    try {
        response = request->send();
    } catch (HttpError& error) {
        Log::warn("Configuration", "Failed to download configuration: %s", error.what());
    }

    if (response.status == 200) { // ok
        nlohmann::json val = safeParseJson(response.body);

        auto settings = val["settings"];
        cached.expires = requestStart + std::chrono::minutes(JsonUtils::asInt(val["refreshInterval"]));
        cached.data = settings;
        cached.etag = response.findHeader("ETag");
        applyFromJson(settings);
        expires = cached.expires;
        downloaded = true;
        if (cache)
            cache->writeConfigToCache(url, cached);
        return true;
    } else if (response.status == 304) { // cached
        importCached(cached);
        return true;
    }
    Log::warn("Configuration", "Failed to download configuration: status code %li", response.status);
    return false;
}

void Configuration::importCached(CachedConfiguration const& cached) {
    expires = cached.expires;
    downloaded = true;

    applyFromJson(cached.data);
}

nlohmann::json Configuration::safeParseJson(std::string const& str) {
    try {
        return nlohmann::json::parse(str);
    } catch (nlohmann::json::parse_error& e) {
        Log::error("Configuration", "Received invalid json: %s", e.what());
        return nlohmann::json::object();
    }
}