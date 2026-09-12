// tests/docs/site.cpp — verifies the static docs-site generator (maz::docs). Pure in-memory string work, so
// every claim is checked headlessly: the Markdown subset renders to the right HTML, literal <>& are escaped
// (no markup injection), intra-doc .md links are rewritten to .html, and buildSite emits an index plus one
// page per doc with a shared nav linking them all.
#include "maz/docs/SiteGen.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::docs;

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

int main() {
    // --- 1. Markdown subset renders to the expected HTML. ---
    {
        const std::string md =
            "# Title\n"
            "\n"
            "Some text with **bold** and `code` and a [guide](GUIDE.md).\n"
            "\n"
            "- first\n"
            "- second\n"
            "\n"
            "```\nint x = a < b;\n```\n";
        const std::string html = renderMarkdown(md);
        CHECK(contains(html, "<h1>Title</h1>"), "H1 heading rendered");
        CHECK(contains(html, "<strong>bold</strong>"), "bold rendered");
        CHECK(contains(html, "<code>code</code>"), "inline code rendered");
        CHECK(contains(html, "<li>first</li>") && contains(html, "<li>second</li>"), "list items rendered");
        CHECK(contains(html, "<pre><code>"), "fenced code block rendered");
        CHECK(contains(html, "int x = a &lt; b;"), "code block content is HTML-escaped");
        CHECK(contains(html, "href=\"GUIDE.html\""), "intra-doc .md link rewritten to .html");
    }

    // --- 2. Literal HTML in prose is escaped, not injected. ---
    {
        const std::string html = renderMarkdown("Use <script> & <b> carefully.\n");
        CHECK(contains(html, "&lt;script&gt;") && contains(html, "&amp;"), "prose HTML escaped");
        CHECK(!contains(html, "<script>"), "no raw script tag leaks through");
    }

    // --- 3. External links and anchors handled correctly. ---
    {
        const std::string html = renderMarkdown("[site](https://example.com/x.md) and [a](API.md#foo)\n");
        CHECK(contains(html, "href=\"https://example.com/x.md\""), "external .md link left untouched");
        CHECK(contains(html, "href=\"API.html#foo\""), ".md#anchor rewritten to .html#anchor");
    }

    // --- 4. buildSite emits an index + one page per doc, with a shared nav linking them. ---
    {
        std::vector<Page> pages = {
            {"intro", "Introduction", "# Introduction\n\nWelcome.\n"},
            {"api", "API Reference", "# API\n\nDetails.\n"},
        };
        SiteOptions opt;
        opt.siteTitle = "Maz Docs";
        const std::map<std::string, std::string> site = buildSite(pages, opt);

        CHECK(site.count("index.html") == 1, "index.html generated");
        CHECK(site.count("intro.html") == 1, "intro.html generated");
        CHECK(site.count("api.html") == 1, "api.html generated");

        const std::string& index = site.at("index.html");
        CHECK(contains(index, "<title>Home — Maz Docs</title>"), "index title set");
        CHECK(contains(index, "href=\"intro.html\"") && contains(index, "href=\"api.html\""),
              "index links every page");

        const std::string& intro = site.at("intro.html");
        CHECK(contains(intro, "<h1>Introduction</h1>"), "page renders its content");
        CHECK(contains(intro, "class=\"sidebar\""), "page has the nav sidebar");
        CHECK(contains(intro, "href=\"api.html\""), "nav cross-links to sibling pages");
        CHECK(contains(intro, "<!doctype html>"), "page is a full HTML document");
    }

    if (g_fail == 0) {
        std::printf("docs_site: OK — markdown subset, escaping, link rewrite, index+nav site build.\n");
        return 0;
    }
    std::printf("docs_site: %d failure(s).\n", g_fail);
    return 1;
}
