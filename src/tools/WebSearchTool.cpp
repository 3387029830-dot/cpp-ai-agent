#include "tools/WebSearchTool.h"

#include <algorithm>
#include <cpr/cpr.h>
#include <cpr/ssl_options.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cpp_ai_agent::tools {

namespace {

nlohmann::json stringProperty(const std::string& description) {
    return {
        {"type", "string"},
        {"description", description},
    };
}

void collectRelatedTopics(const nlohmann::json& topics, std::vector<nlohmann::json>& output) {
    for (const auto& item : topics) {
        if (item.contains("Topics")) {
            collectRelatedTopics(item.at("Topics"), output);
            continue;
        }
        if (item.contains("Text") || item.contains("FirstURL")) {
            output.push_back(item);
        }
    }
}

std::string cleanText(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    return value;
}

bool isUrlUnreserved(unsigned char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
           ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

std::string urlEncode(const std::string& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char ch : value) {
        if (isUrlUnreserved(ch)) {
            encoded.push_back(static_cast<char>(ch));
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hex[ch >> 4]);
        encoded.push_back(hex[ch & 0x0F]);
    }
    return encoded;
}

std::string explainSearchError(const std::string& message, const std::string& proxyUrl) {
    std::ostringstream output;
    output << message;
    if (!proxyUrl.empty()) {
        output << "\nHint: Web search is using proxy " << proxyUrl
               << ". If that local proxy is not running, clear web_search.proxy_url in config/settings.json "
               << "or set WEB_SEARCH_PROXY_URL to a reachable proxy.";
    } else {
        output << "\nHint: Check network access to https://api.duckduckgo.com/. "
               << "If your network requires a proxy, set WEB_SEARCH_PROXY_URL.";
    }
    return output.str();
}

std::string formatSearchFallback(
    const std::string& query,
    const std::string& reason,
    const std::string& proxyUrl
) {
    std::ostringstream output;
    output << "Web search unavailable\n";
    output << "Query: " << query << "\n";
    output << "Provider: DuckDuckGo Instant Answer\n";
    output << "Reason: " << reason << "\n\n";
    output << "Fallback links\n";
    output << "1. DuckDuckGo: https://duckduckgo.com/?q=" << urlEncode(query) << "\n";
    output << "2. Bing: https://www.bing.com/search?q=" << urlEncode(query) << "\n\n";
    if (!proxyUrl.empty()) {
        output << "Hint: Web search is using proxy " << proxyUrl
               << ". If that local proxy is not running, clear web_search.proxy_url in config/settings.json "
               << "or set WEB_SEARCH_PROXY_URL to a reachable proxy.\n";
    } else {
        output << "Hint: Check network access to https://api.duckduckgo.com/. "
               << "If your network requires a proxy, set WEB_SEARCH_PROXY_URL.\n";
    }
    return output.str();
}

}  // namespace

WebSearchTool::WebSearchTool(std::string proxyUrl)
    : proxyUrl_(std::move(proxyUrl)) {}

std::string WebSearchTool::name() const {
    return "web_search";
}

std::string WebSearchTool::description() const {
    return "Search the web and return organized results with titles, snippets, and URLs.";
}

nlohmann::json WebSearchTool::parametersSchema() const {
    return {
        {"type", "object"},
        {"properties",
         {
             {"query", stringProperty("Search query.")},
             {"max_results",
              {
                  {"type", "integer"},
                  {"description", "Maximum number of organized results to return."},
                  {"minimum", 1},
                  {"maximum", 10},
              }},
         }},
        {"required", nlohmann::json::array({"query"})},
        {"additionalProperties", false},
    };
}

RiskLevel WebSearchTool::risk() const {
    return RiskLevel::Safe;
}

ToolResult WebSearchTool::execute(const nlohmann::json& args) const {
    try {
        const auto query = args.at("query").get<std::string>();
        if (query.empty()) {
            throw std::invalid_argument("query must not be empty.");
        }

        const auto maxResults = (std::max)(1, (std::min)(10, args.value("max_results", 5)));

        cpr::Session session;
        session.SetUrl(cpr::Url{"https://api.duckduckgo.com/"});
        session.SetParameters(cpr::Parameters{
            {"q", query},
            {"format", "json"},
            {"no_html", "1"},
            {"skip_disambig", "1"},
        });
        session.SetHeader(cpr::Header{{"Accept", "application/json"}});
        session.SetTimeout(cpr::Timeout{15000});
#ifdef _WIN32
        // Schannel can fail when Windows cannot reach certificate revocation servers.
        session.SetSslOptions(cpr::Ssl(cpr::ssl::NoRevoke{true}));
#endif
        if (!proxyUrl_.empty()) {
            session.SetProxies(cpr::Proxies{
                {"http", proxyUrl_},
                {"https", proxyUrl_},
            });
        }

        const auto response = session.Get();
        if (response.error) {
            const auto reason = "web search request failed: " + response.error.message;
            return {
                true,
                formatSearchFallback(query, reason, proxyUrl_),
                "Web search fallback: " + query,
            };
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            const auto reason = "web search returned HTTP " + std::to_string(response.status_code);
            return {
                true,
                formatSearchFallback(query, reason, proxyUrl_),
                "Web search fallback: " + query,
            };
        }

        const auto body = nlohmann::json::parse(response.text);
        const auto formatted = formatDuckDuckGoResults(query, body, maxResults);
        return {true, formatted, "Web search: " + query};
    } catch (const std::exception& ex) {
        return {false, ex.what(), std::string("Tool failed: ") + ex.what()};
    }
}

std::string formatDuckDuckGoResults(
    const std::string& query,
    const nlohmann::json& body,
    int maxResults
) {
    std::ostringstream output;
    output << "Web search results\n";
    output << "Query: " << query << "\n";
    output << "Provider: DuckDuckGo Instant Answer\n\n";

    const auto heading = body.value("Heading", "");
    const auto abstractText = body.value("AbstractText", "");
    const auto abstractUrl = body.value("AbstractURL", "");
    if (!heading.empty() || !abstractText.empty()) {
        output << "Top answer\n";
        if (!heading.empty()) {
            output << "Title: " << heading << "\n";
        }
        if (!abstractText.empty()) {
            output << "Summary: " << cleanText(abstractText) << "\n";
        }
        if (!abstractUrl.empty()) {
            output << "URL: " << abstractUrl << "\n";
        }
        output << "\n";
    }

    std::vector<nlohmann::json> results;
    for (const auto& item : body.value("Results", nlohmann::json::array())) {
        results.push_back(item);
    }
    collectRelatedTopics(body.value("RelatedTopics", nlohmann::json::array()), results);

    output << "Results\n";
    int count = 0;
    for (const auto& result : results) {
        if (count >= maxResults) {
            break;
        }
        const auto text = result.value("Text", "");
        const auto url = result.value("FirstURL", "");
        if (text.empty() && url.empty()) {
            continue;
        }

        ++count;
        output << count << ". " << cleanText(text.empty() ? "(no snippet)" : text) << "\n";
        if (!url.empty()) {
            output << "   URL: " << url << "\n";
        }
    }

    if (count == 0) {
        output << "(no structured results returned)\n";
        output << "Search URL: https://duckduckgo.com/?q=" << urlEncode(query) << "\n";
    }

    output << "\n整理建议: 使用编号结果作为引用来源，优先概括共同结论，再列出差异和不确定性。\n";
    return output.str();
}

}  // namespace cpp_ai_agent::tools
