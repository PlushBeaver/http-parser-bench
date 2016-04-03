#include <chrono>
#include <iostream>

#include <gsl.h>

#include <http_parser.h>

#include <pion/http/parser.hpp>
#include <pion/http/request.hpp>

using StringView = gsl::span<const char>;

// Разобрать нужно в такую структуру; если нужны преобразования, это недостаток.
class HTTPRequest {
public:
    const StringView& get_uri() const {
        return _uri;
    }

    const StringView& get_host() const {
        return _host;
    }

    const StringView& get_user_agent() const {
        return _user_agent;
    }

    const StringView& get_accept() const {
        return _accept;
    }

protected:
    StringView _uri;
    StringView _host;
    StringView _user_agent;
    StringView _accept;
};

namespace http_parser_bench {

class HTTPRequest : public ::HTTPRequest {
public:
    static HTTPRequest parse(const StringView& text);

private:
    enum {
        CONTINUE = 0,
        STOP = 1
    };

    enum Item {
        ITEM_NOTHING = -1,
        ITEM_URI = 0,
        ITEM_HOST,
        ITEM_USER_AGENT,
        ITEM_ACCEPT,
        ITEM_COUNT
    };

    static int on_url(http_parser* parser, const char *at, size_t length);
    static int on_header_field(http_parser* parser, const char *at, size_t length);
    static int on_header_value(http_parser* parser, const char *at, size_t length);
    static int on_headers_complete(http_parser* parser);

    Item _current_item = ITEM_NOTHING;
};

}  // namespace http_parser_bench

namespace pion_bench {

class HTTPRequest : public ::HTTPRequest {
public:
    static HTTPRequest parse(const StringView& text);

private:
    inline StringView save(const std::string& new_data);

    std::array<char, 1500> _data;
    size_t _data_used = 0;
};

}  // namespace pion_bench

template<class Request> void benchmark(const std::string& caption);

int
main() {
    benchmark<http_parser_bench::HTTPRequest>("http_parser");
    benchmark<pion_bench::HTTPRequest>("pion");
}

static const char REQUEST[] =
        "GET /w/cpp/chrono/duration HTTP/1.1\r\n"
        "Host: en.cppreference.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Accept-Language: en,ru-RU;q=0.8,ru;q=0.5,en-US;q=0.3\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Referer: http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=duration\r\n"
        "Cookie: __utma=165123437.262467405.1456259269.1457796545.1459620610.13; __utmz=165123437.1456259269.1.1.utmcsr=(direct)|utmccn=(direct)|utmcmd=(none); __utmb=165123437.2.10.1459620610; __utmc=165123437; __utmt=1\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: max-age=0\r\n"
        "\r\n";

template<size_t N>
static inline bool
equals(const StringView& view, const char (&string)[N]) {
    const size_t size = N - 1;
    return (view.size() == size) && (std::strncmp(view.data(), string, size) == 0);
}

template<class Code>
std::chrono::nanoseconds
benchmark(Code&& code) {
    using Clock = std::chrono::high_resolution_clock;

    constexpr size_t RUNS = 100;
    constexpr size_t COUNT = 1000;

    std::chrono::nanoseconds total{0};
    for (size_t i = 0; i < RUNS; ++i)
    {
        auto start = Clock::now();
        for (size_t j = 0; j < COUNT; ++j) {
            code();
        }
        auto finish = Clock::now();
        total += (finish - start) / COUNT;
    }
    return total / RUNS;
}

template<class Request>
void
benchmark(const std::string& caption) {
    auto request = Request::parse(REQUEST);
    assert(equals(request.get_uri(), "/w/cpp/chrono/duration"));
    assert(equals(request.get_host(), "en.cppreference.com"));
    assert(equals(request.get_user_agent(), "Mozilla/5.0 (X11; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0"));
    assert(equals(request.get_accept(), "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"));

    auto result = benchmark([]{
        Request::parse(REQUEST);
    });
    std::cout << caption << ": " << result.count() << "ns" << std::endl;
}

namespace http_parser_bench {

HTTPRequest
HTTPRequest::parse(const StringView& text) {
    HTTPRequest request;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = &request;

    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_url = HTTPRequest::on_url;
    settings.on_header_field = HTTPRequest::on_header_field;
    settings.on_header_value = HTTPRequest::on_header_value;
    settings.on_headers_complete = HTTPRequest::on_headers_complete;

    http_parser_execute(&parser, &settings, text.data(), text.size());

    return request;
}

int
HTTPRequest::on_url(http_parser* parser, const char *at, size_t length) {
    HTTPRequest& request = *reinterpret_cast<HTTPRequest*>(parser->data);
    request._uri = gsl::as_span(at, length);
    return CONTINUE;
}

int
HTTPRequest::on_header_field(http_parser* parser, const char *at, size_t length) {
    HTTPRequest& request = *reinterpret_cast<HTTPRequest*>(parser->data);
    const StringView header = gsl::as_span(at, length);
    if (equals(header, "Host")) {
        request._current_item = ITEM_HOST;
    } else if (equals(header, "User-Agent")) {
        request._current_item = ITEM_USER_AGENT;
    } else if (equals(header, "Accept")) {
        request._current_item = ITEM_ACCEPT;
    } else {
        request._current_item = ITEM_NOTHING;
    }
    return CONTINUE;
}

int
HTTPRequest::on_header_value(http_parser* parser, const char *at, size_t length) {
    HTTPRequest& request = *reinterpret_cast<HTTPRequest*>(parser->data);
    if (request._current_item == ITEM_NOTHING) {
        return CONTINUE;
    }

    const StringView value = gsl::as_span(at, length);
    switch (request._current_item) {
    case ITEM_HOST:
        request._host = value;
        break;
    case ITEM_USER_AGENT:
        request._user_agent = value;
        break;
    case ITEM_ACCEPT:
        request._accept = value;
        break;
    default:
        return CONTINUE;
    }

    request._current_item = ITEM_NOTHING;
    return CONTINUE;
}

int
HTTPRequest::on_headers_complete(http_parser* parser) {
    return STOP;
}

}  // namespace http_parser_bench

namespace pion_bench {

// NOTE: есть в реализации gsl::span<> у Microsoft, нет в более старых.
// static inline StringView
// as_span(const std::string& string) {
//     return gsl::as_span(string.data(), string.size());
// }

inline StringView
HTTPRequest::save(const std::string& new_data) {
    const auto data = &_data[_data_used];
    const auto size = new_data.size();
    std::memcpy(data, new_data.data(), size);
    _data_used += size;
    // TODO: автоматическое преобразование gsl::span<T> в gsl::span<const T>.
    return gsl::as_span(data, size);
}

HTTPRequest
HTTPRequest::parse(const StringView& text) {
    HTTPRequest result;

    pion::http::parser parser{true};
    pion::http::request request;
    boost::system::error_code error_code;
    parser.set_read_buffer(text.data(), text.size());
    parser.parse_headers_only(true);
    parser.parse(request, error_code);

    result._uri = result.save(request.get_resource());
    if (request.has_header("Host")) {
        result._host = result.save(request.get_header("Host"));
    }
    if (request.has_header("User-Agent")) {
        result._user_agent = result.save(request.get_header("User-Agent"));
    }
    if (request.has_header("Accept")) {
        result._accept = result.save(request.get_header("Accept"));
    }

    return result;
}

}  // namespace pion_bench
