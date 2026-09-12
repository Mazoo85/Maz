// tools/gen_docs.cpp — CLI for the maz::docs static site generator. Reads a directory of Markdown files and
// writes a linked static HTML site (index + one page per doc + shared nav) that can be opened in any browser
// or served as GitHub Pages. Usage:
//   gen_docs <input-dir> <output-dir>
// Each "*.md" in <input-dir> becomes "<slug>.html"; the page title is the file's first "# " heading (or the
// filename). Pure std::filesystem I/O around the header-only renderer.
#include "maz/docs/SiteGen.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Title = first "# " heading in the markdown, else the stem.
static std::string deriveTitle(const std::string& md, const std::string& fallback) {
    std::istringstream in(md);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("# ", 0) == 0) return line.substr(2);
    }
    return fallback;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <input-dir> <output-dir>\n", argv[0]);
        return 2;
    }
    const fs::path inDir = argv[1];
    const fs::path outDir = argv[2];
    if (!fs::is_directory(inDir)) {
        std::fprintf(stderr, "gen_docs: input directory not found: %s\n", inDir.string().c_str());
        return 1;
    }

    std::vector<maz::docs::Page> pages;
    for (const fs::directory_entry& e : fs::directory_iterator(inDir)) {
        if (!e.is_regular_file() || e.path().extension() != ".md") continue;
        const std::string slug = e.path().stem().string();
        const std::string md = readFile(e.path());
        pages.push_back({slug, deriveTitle(md, slug), md});
    }
    std::sort(pages.begin(), pages.end(),
              [](const maz::docs::Page& a, const maz::docs::Page& b) { return a.title < b.title; });

    if (pages.empty()) {
        std::fprintf(stderr, "gen_docs: no .md files in %s\n", inDir.string().c_str());
        return 1;
    }

    maz::docs::SiteOptions opt;
    opt.siteTitle = "Maz Engine Documentation";
    const std::map<std::string, std::string> site = maz::docs::buildSite(pages, opt);

    std::error_code ec;
    fs::create_directories(outDir, ec);
    for (const auto& [name, html] : site) {
        std::ofstream out(outDir / name, std::ios::binary);
        out << html;
    }
    std::printf("gen_docs: wrote %zu pages to %s\n", site.size(), outDir.string().c_str());
    return 0;
}
