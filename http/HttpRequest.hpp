#pragma once

#include <unordered_map>
#include <unordered_set>

#include "../base/Buffer.hpp"

using std::string;
using std::unordered_map;
using std::unordered_set;

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HTTP_CODE httpCode;

    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer &buff);

    std::string path() const;
    std::string &path();
    std::string method() const;
    std::string version() const;

    bool IsKeepAlive() const;

private:
    bool ParseRequestLine_(const std::string &line);
    void ParseHeader_(const std::string &line);
    void ParseBody_(const std::string &line);

    void ParsePath_();

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};