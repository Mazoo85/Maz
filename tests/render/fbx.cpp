// tests/render/fbx.cpp — verifies the ASCII FBX geometry importer (render::parseFbxAscii) reads a real
// FBX mesh: locates the Vertices + PolygonVertexIndex arrays, decodes FBX's ~i polygon terminator, and
// fan-triangulates. Uses a hand-authored ASCII FBX unit cube (8 verts, 6 quad faces).
#include "maz/render/FbxLoader.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c,m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // A minimal but syntactically real ASCII FBX: a unit cube spanning [-1,1]^3.
    // Vertices: 8 xyz triples (*24). PolygonVertexIndex: 6 quads (*24), each quad's last index encoded
    // as ~i (negative) to mark the polygon end.
    const std::string fbx = R"FBX(; FBX 7.4.0 project file
Objects:  {
    Geometry: 140, "Geometry::Cube", "Mesh" {
        Vertices: *24 {
            a: -1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1, -1,-1,1, 1,-1,1, 1,1,1, -1,1,1
        }
        PolygonVertexIndex: *24 {
            a: 0,1,2,-4, 4,5,6,-8, 0,1,5,-5, 3,2,6,-8, 0,3,7,-5, 1,2,6,-6
        }
    }
}
)FBX";

    maz::render::shapes::MeshData mesh;
    const bool ok = maz::render::parseFbxAscii(fbx, mesh);
    CHECK(ok, "parseFbxAscii returned true");

    // 6 quads -> 12 triangles -> 36 expanded vertices (3 per tri), 36 sequential indices.
    CHECK(mesh.vertices.size() == 36, "12 triangles => 36 vertices");
    CHECK(mesh.indices.size() == 36, "36 indices");

    // Bounding box must be exactly the unit cube.
    float mnx=1e9f,mny=1e9f,mnz=1e9f,mxx=-1e9f,mxy=-1e9f,mxz=-1e9f;
    for (const auto& v : mesh.vertices) {
        mnx=std::min(mnx,v.px); mny=std::min(mny,v.py); mnz=std::min(mnz,v.pz);
        mxx=std::max(mxx,v.px); mxy=std::max(mxy,v.py); mxz=std::max(mxz,v.pz);
    }
    CHECK(mnx==-1.f&&mny==-1.f&&mnz==-1.f, "min corner (-1,-1,-1)");
    CHECK(mxx== 1.f&&mxy== 1.f&&mxz== 1.f, "max corner (1,1,1)");

    // Every triangle should carry a unit-length normal.
    bool normalsUnit = true;
    for (std::size_t i=0;i<mesh.vertices.size();i+=3){
        const auto& v=mesh.vertices[i];
        const float l=std::sqrt(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz);
        if (std::fabs(l-1.f)>1e-3f){ normalsUnit=false; break; }
    }
    CHECK(normalsUnit, "per-face normals are unit length");

    // A non-mesh buffer must be rejected.
    maz::render::shapes::MeshData none;
    CHECK(!maz::render::parseFbxAscii("Objects: { Model: 1, \"x\", \"Null\" {} }", none), "non-mesh rejected");

    if (g_fail==0){ std::printf("fbx: OK — ASCII FBX cube imported (36 verts, unit bbox, unit normals).\n"); return 0; }
    std::printf("fbx: %d failure(s).\n", g_fail); return 1;
}
