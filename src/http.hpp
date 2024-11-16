#pragma once
#include <HTTPClient.h>
#include <cstdint>

struct ScopedHTTP
{
    HTTPClient client;

    ~ScopedHTTP() { client.end(); }
};

bool GETStream(const char* uri, std::function<bool(HTTPClient&, const char*)> onStream)
{
    ScopedHTTP http;
    if (http.client.begin(uri) <= 0)
    {
        onStream(http.client, "Unable to connect");
        return false;
    }

    int resCode = http.client.GET();
    if (resCode == HTTP_CODE_OK)
    {
        return onStream(http.client, nullptr);
    }
    else
    {
        onStream(http.client, http.client.errorToString(resCode).c_str());
        return false;
    }
}

bool GET(const char* uri, std::function<void(char*, size_t)> onData,
    std::function<void(const char*)> onComplete)
{
    return GETStream(uri, [&onData, &onComplete](HTTPClient& client, const char* err)
        {
            if (err) {
                onComplete(err);
                return false;
            }
            else {
                char buffer[512];
                int size = client.getSize();
                Stream* stream = client.getStreamPtr();

                do {
                    size_t count = stream->readBytes(buffer, sizeof(buffer));
                    onData(buffer, count);
                } while (stream->available() > 0);
                onComplete(nullptr);
                return true;
            }
        });
}
