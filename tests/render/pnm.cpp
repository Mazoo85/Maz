// tests/render/pnm.cpp — verifies the Netpbm (PNM) codec against HAND-AUTHORED byte streams (independent
// of the implementation) for all six variants P1..P6, plus an encode->decode round-trip.
#include "maz/render/ImageCodecPnm.hpp"
#include <cstdio>
#include <string>
#include <vector>
using namespace maz::render;

static int g_fail = 0;
#define CHECK(c,m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool px(const Image& im, int x, int y, int r, int g, int b) {
    const auto& d = im.data();
    const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(im.width()) +
                           static_cast<std::size_t>(x)) * 4;
    return d[i]==r && d[i+1]==g && d[i+2]==b && d[i+3]==255;
}
static std::vector<std::uint8_t> bytes(const std::string& s){ return {s.begin(), s.end()}; }

int main() {
    // --- P3 ASCII pixmap: 2x2 red/green/blue/yellow ---
    {
        auto b = bytes("P3\n# a comment\n2 2\n255\n 255 0 0  0 255 0  0 0 255  255 255 0\n");
        Image im = decodePnm(b);
        CHECK(im.width()==2 && im.height()==2, "P3 dims");
        CHECK(px(im,0,0,255,0,0) && px(im,1,0,0,255,0) && px(im,0,1,0,0,255) && px(im,1,1,255,255,0), "P3 pixels");
    }
    // --- P6 binary pixmap: 2x2 same colors ---
    {
        std::vector<std::uint8_t> b = bytes("P6\n2 2\n255\n");
        const std::uint8_t body[] = {255,0,0, 0,255,0, 0,0,255, 255,255,0};
        b.insert(b.end(), body, body+12);
        Image im = decodePnm(b);
        CHECK(px(im,0,0,255,0,0) && px(im,1,1,255,255,0), "P6 pixels");
    }
    // --- P2 ASCII graymap: 2x1 black/white ---
    {
        Image im = decodePnm(bytes("P2\n2 1\n255\n 0 255\n"));
        CHECK(px(im,0,0,0,0,0) && px(im,1,0,255,255,255), "P2 gray");
    }
    // --- P5 binary graymap: 2x1 ---
    {
        std::vector<std::uint8_t> b = bytes("P5\n2 1\n255\n");
        const std::uint8_t body[] = {0,255};
        b.insert(b.end(), body, body+2);
        Image im = decodePnm(b);
        CHECK(px(im,0,0,0,0,0) && px(im,1,0,255,255,255), "P5 gray");
    }
    // --- P1 ASCII bitmap: 2x2, "0110" (1=black) ---
    {
        Image im = decodePnm(bytes("P1\n2 2\n0110\n"));
        CHECK(px(im,0,0,255,255,255) && px(im,1,0,0,0,0) && px(im,0,1,0,0,0) && px(im,1,1,255,255,255), "P1 bitmap");
    }
    // --- P4 binary bitmap: 8x1, byte 0x80 => only pixel 0 black ---
    {
        std::vector<std::uint8_t> b = bytes("P4\n8 1\n");
        b.push_back(0x80);
        Image im = decodePnm(b);
        CHECK(px(im,0,0,0,0,0) && px(im,1,0,255,255,255) && px(im,7,0,255,255,255), "P4 bitmap");
    }
    // --- round-trip: build an image, encode P6, decode, compare ---
    {
        Image src(3,2);
        src.setPixel(0,0,color8(10,20,30,255)); src.setPixel(1,0,color8(40,50,60,255));
        src.setPixel(2,0,color8(70,80,90,255)); src.setPixel(0,1,color8(100,110,120,255));
        src.setPixel(1,1,color8(130,140,150,255)); src.setPixel(2,1,color8(200,210,220,255));
        Image r6 = decodePnm(encodePnmP6(src));
        Image r3 = decodePnm(encodePnmP3(src));
        bool same6 = r6.width()==3 && r6.height()==2, same3 = r3.width()==3 && r3.height()==2;
        for (int y=0;y<2;++y) for(int x=0;x<3;++x){
            const auto c = src.getPixel(x,y);
            const int rr=static_cast<int>(c.r*255.f+0.5f), gg=static_cast<int>(c.g*255.f+0.5f), bb=static_cast<int>(c.b*255.f+0.5f);
            if(!px(r6,x,y,rr,gg,bb)) same6=false;
            if(!px(r3,x,y,rr,gg,bb)) same3=false;
        }
        CHECK(same6, "P6 round-trip exact");
        CHECK(same3, "P3 round-trip exact");
    }
    // --- malformed rejected ---
    CHECK(decodePnm(bytes("P7\n1 1\n255\n")).empty(), "unknown magic rejected");
    CHECK(decodePnm(bytes("not a pnm")).empty(), "garbage rejected");

    if(g_fail==0){ std::printf("pnm: OK — P1..P6 decode + P6/P3 round-trip verified against hand-authored bytes.\n"); return 0; }
    std::printf("pnm: %d failure(s).\n", g_fail); return 1;
}
