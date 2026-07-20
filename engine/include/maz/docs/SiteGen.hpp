#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// maz::docs static documentation-site generator — turns the engine's Markdown docs into a linked, browsable
// static HTML site (index + one page per doc + a shared nav sidebar), the "read the docs" surface every
// mature engine ships. It renders a practical Markdown subset — ATX headings (#..######), paragraphs,
// unordered lists, fenced ``` code blocks, and inline **bold** / `code` / [links](url) — HTML-escaping all
// text so a doc that contains `<`, `>` or `&` renders literally rather than injecting markup, and rewriting
// intra-doc `.md` links to `.html` so navigation works in the generated site. Pure std-only string work: no
// GPU, no I/O in the core (the CLI wrapper does the file reads/writes), so it unit-tests headlessly from
// in-memory strings. `buildSite` returns a filename -> full-HTML-document map ready to write to disk.
//
// Scope note (honest): a focused Markdown subset (the constructs the repo's docs actually use), not a full
// CommonMark implementation — nested lists, tables, blockquotes, and images are documented follow-ups.
namespace maz::docs {

// One source document: a URL-safe slug (becomes "<slug>.html"), a human title (nav + <title>), and its
// Markdown body.
struct Page {
    std::string slug;
    std::string title;
    std::string markdown;
};

struct SiteOptions {
    std::string siteTitle = "Maz Documentation";
};

namespace detail {

inline std::string escapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

// Rewrite an intra-doc link so the generated site is navigable: "GUIDE.md" -> "GUIDE.html",
// "GUIDE.md#anchor" -> "GUIDE.html#anchor". External links (containing "://") are left untouched.
inline std::string rewriteLink(const std::string& url) {
    if (url.find("://") != std::string::npos) return url;
    const std::size_t md = url.find(".md");
    if (md == std::string::npos) return url;
    const bool atEndOrAnchor = (md + 3 == url.size()) || (url[md + 3] == '#');
    if (!atEndOrAnchor) return url;
    return url.substr(0, md) + ".html" + url.substr(md + 3);
}

// Render inline Markdown (code spans, bold, links) to HTML, escaping all literal text.
inline std::string renderInline(const std::string& raw) {
    std::string out;
    const std::size_t n = raw.size();
    std::size_t i = 0;
    while (i < n) {
        const char ch = raw[i];
        if (ch == '`') {
            const std::size_t end = raw.find('`', i + 1);
            if (end != std::string::npos) {
                out += "<code>";
                out += escapeHtml(raw.substr(i + 1, end - i - 1));
                out += "</code>";
                i = end + 1;
                continue;
            }
        }
        if (ch == '*' && i + 1 < n && raw[i + 1] == '*') {
            const std::size_t end = raw.find("**", i + 2);
            if (end != std::string::npos) {
                out += "<strong>";
                out += renderInline(raw.substr(i + 2, end - i - 2));
                out += "</strong>";
                i = end + 2;
                continue;
            }
        }
        if (ch == '[') {
            const std::size_t rb = raw.find(']', i + 1);
            if (rb != std::string::npos && rb + 1 < n && raw[rb + 1] == '(') {
                const std::size_t rp = raw.find(')', rb + 2);
                if (rp != std::string::npos) {
                    const std::string text = raw.substr(i + 1, rb - i - 1);
                    const std::string url = rewriteLink(raw.substr(rb + 2, rp - rb - 2));
                    out += "<a href=\"" + escapeHtml(url) + "\">" + renderInline(text) + "</a>";
                    i = rp + 1;
                    continue;
                }
            }
        }
        // Literal character (escaped).
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch; break;
        }
        ++i;
    }
    return out;
}

inline std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::string cur;
    for (const char c : s) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    lines.push_back(cur);
    return lines;
}

inline bool startsWith(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}

} // namespace detail

// Render a Markdown document to an HTML fragment (no <html>/<head>/<body> wrapper).
inline std::string renderMarkdown(const std::string& md) {
    const std::vector<std::string> lines = detail::splitLines(md);
    std::string out;
    std::vector<std::string> para;

    auto flushPara = [&]() {
        if (para.empty()) return;
        out += "<p>";
        for (std::size_t k = 0; k < para.size(); ++k) {
            if (k) out += " ";
            out += detail::renderInline(para[k]);
        }
        out += "</p>\n";
        para.clear();
    };

    std::size_t i = 0;
    while (i < lines.size()) {
        const std::string& line = lines[i];

        // Fenced code block.
        if (detail::startsWith(line, "```")) {
            flushPara();
            ++i;
            std::string code;
            while (i < lines.size() && !detail::startsWith(lines[i], "```")) {
                code += lines[i];
                code += "\n";
                ++i;
            }
            if (i < lines.size()) ++i; // consume closing fence
            out += "<pre><code>" + detail::escapeHtml(code) + "</code></pre>\n";
            continue;
        }

        // ATX heading.
        std::size_t h = 0;
        while (h < line.size() && line[h] == '#') ++h;
        if (h >= 1 && h <= 6 && h < line.size() && line[h] == ' ') {
            flushPara();
            const std::string txt = line.substr(h + 1);
            const std::string tag = std::to_string(h);
            out += "<h" + tag + ">" + detail::renderInline(txt) + "</h" + tag + ">\n";
            ++i;
            continue;
        }

        // Unordered list.
        if (detail::startsWith(line, "- ") || detail::startsWith(line, "* ")) {
            flushPara();
            out += "<ul>\n";
            while (i < lines.size() &&
                   (detail::startsWith(lines[i], "- ") || detail::startsWith(lines[i], "* "))) {
                out += "<li>" + detail::renderInline(lines[i].substr(2)) + "</li>\n";
                ++i;
            }
            out += "</ul>\n";
            continue;
        }

        // Blank line ends a paragraph.
        if (line.empty()) {
            flushPara();
            ++i;
            continue;
        }

        para.push_back(line);
        ++i;
    }
    flushPara();
    return out;
}

namespace detail {

inline std::string navHtml(const std::vector<Page>& pages) {
    std::string nav = "<nav class=\"sidebar\">\n<a class=\"home\" href=\"index.html\">Home</a>\n<ul>\n";
    for (const Page& p : pages) {
        nav += "<li><a href=\"" + escapeHtml(p.slug) + ".html\">" + escapeHtml(p.title) + "</a></li>\n";
    }
    nav += "</ul>\n</nav>\n";
    return nav;
}

inline const char* siteCss() {
    return "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;margin:0;color:#1a1a1a;"
           "background:#fff;display:flex}"
           ".sidebar{width:240px;min-height:100vh;background:#0f1420;color:#c8d0e0;padding:24px 18px;"
           "box-sizing:border-box}"
           ".sidebar a{color:#c8d0e0;text-decoration:none}.sidebar a:hover{color:#7fb0ff}"
           ".sidebar .home{display:block;font-weight:700;margin-bottom:12px;color:#fff}"
           ".sidebar ul{list-style:none;padding:0;margin:0}.sidebar li{margin:6px 0}"
           "main{padding:32px 48px;max-width:820px;line-height:1.6}"
           "pre{background:#0f1420;color:#e6edf3;padding:14px;border-radius:8px;overflow-x:auto}"
           "code{font-family:ui-monospace,SFMono-Regular,Menlo,monospace}"
           "main>code,p code,li code{background:#eef1f6;padding:1px 5px;border-radius:4px}"
           "h1,h2,h3{line-height:1.25}a{color:#2456c8}"
           "@media(prefers-color-scheme:dark){body{background:#12161c;color:#e6edf3}"
           "main>code,p code,li code{background:#232a36}a{color:#7fb0ff}}";
}

inline std::string wrapDocument(const std::string& pageTitle, const std::string& siteTitle,
                                const std::string& nav, const std::string& body) {
    return "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
           "<title>" + escapeHtml(pageTitle) + " — " + escapeHtml(siteTitle) + "</title>\n"
           "<style>" + std::string(siteCss()) + "</style>\n</head>\n<body>\n" +
           nav + "<main>\n" + body + "</main>\n</body>\n</html>\n";
}

} // namespace detail

// Build the whole static site. Returns filename -> complete HTML document, always including "index.html"
// (a landing page linking every doc) plus one "<slug>.html" per page, each with the shared nav sidebar.
inline std::map<std::string, std::string> buildSite(const std::vector<Page>& pages,
                                                    const SiteOptions& opt = {}) {
    std::map<std::string, std::string> site;
    const std::string nav = detail::navHtml(pages);

    // Index / landing page.
    std::string indexBody = "<h1>" + detail::escapeHtml(opt.siteTitle) + "</h1>\n<ul>\n";
    for (const Page& p : pages) {
        indexBody += "<li><a href=\"" + detail::escapeHtml(p.slug) + ".html\">" +
                     detail::escapeHtml(p.title) + "</a></li>\n";
    }
    indexBody += "</ul>\n";
    site["index.html"] = detail::wrapDocument("Home", opt.siteTitle, nav, indexBody);

    // One page per doc.
    for (const Page& p : pages) {
        const std::string body = renderMarkdown(p.markdown);
        site[p.slug + ".html"] = detail::wrapDocument(p.title, opt.siteTitle, nav, body);
    }
    return site;
}

} // namespace maz::docs
