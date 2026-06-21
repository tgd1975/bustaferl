#include "HttpsNet.h"

#include <curl/curl.h>

namespace bustaferl::native_runtime {

namespace {

constexpr long TIMEOUT_MS = 8000; // mirrors Esp32Network
constexpr long CONNECT_TIMEOUT_MS = 5000;

size_t writeCb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *out = static_cast<std::string *>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

} // namespace

HttpsNet::HttpsNet() { curl_global_init(CURL_GLOBAL_DEFAULT); }
HttpsNet::~HttpsNet() { curl_global_cleanup(); }

bool HttpsNet::httpGet(const std::string &url, std::string &out) {
  out.clear();
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
    return false;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TIMEOUT_MS);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, CONNECT_TIMEOUT_MS);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "bustaferl-native-runtime/1.0");
  if (insecure_) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  const CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  if (rc == CURLE_OK)
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);

  return rc == CURLE_OK && status >= 200 && status < 300;
}

bool HttpsNet::httpPost(const std::string &url, const std::string &body,
                        const std::string &content_type, std::string &out) {
  out.clear();
  CURL *curl = curl_easy_init();
  if (curl == nullptr)
    return false;

  struct curl_slist *headers = nullptr;
  headers =
      curl_slist_append(headers, ("Content-Type: " + content_type).c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TIMEOUT_MS);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, CONNECT_TIMEOUT_MS);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "bustaferl-native-runtime/1.0");
  if (insecure_) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  const CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  if (rc == CURLE_OK)
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return rc == CURLE_OK && status >= 200 && status < 300;
}

} // namespace bustaferl::native_runtime
