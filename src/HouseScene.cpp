#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include "HouseScene.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

namespace {
using V = HouseScene::Vec;
V sub(V a, V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V cross(V a, V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
V unit(V a) { float l=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); return {a.x/l,a.y/l,a.z/l}; }
constexpr V cream{0.78f,0.73f,0.61f}, trim{0.86f,0.83f,0.73f};
constexpr V timber{0.31f,0.20f,0.12f}, dark{0.12f,0.18f,0.18f};
GLuint shader(GLenum type, const char* source) {
    GLuint id=glCreateShader(type); glShaderSource(id,1,&source,nullptr); glCompileShader(id);
    GLint ok=0; glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(id,sizeof(log),nullptr,log);
        std::cerr << "Scene shader: " << log << '\n'; glDeleteShader(id); return 0; }
    return id;
}
GLuint link(const char* vs, const char* fs) {
    GLuint v=shader(GL_VERTEX_SHADER,vs), f=shader(GL_FRAGMENT_SHADER,fs);
    if (!v || !f) { if(v) glDeleteShader(v); if(f) glDeleteShader(f); return 0; }
    GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if (!ok) { char log[4096]; glGetProgramInfoLog(p,sizeof(log),nullptr,log);
        std::cerr << "Scene program: " << log << '\n'; glDeleteProgram(p); return 0; }
    return p;
}
const char* vertexSource=R"GLSL(
#version 120
uniform mat4 lightMatrix;
varying vec3 world, normal, tint;
varying vec2 uv;
varying vec4 shadowPos;
void main() {
    world=gl_Vertex.xyz; normal=gl_Normal; tint=gl_Color.rgb;
    uv=gl_MultiTexCoord0.xy; shadowPos=lightMatrix*gl_Vertex;
    gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
}
)GLSL";
const char* fragmentSource=R"GLSL(
#version 120
uniform sampler2D shadowMap;
uniform int material;
uniform bool lightsOn, shadows;
uniform vec3 eye;
varying vec3 world, normal, tint;
varying vec2 uv;
varying vec4 shadowPos;
float hash(vec2 p) { return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p) {
    vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float gridLine(float p, float width) {
    float aa=max(fwidth(p),0.001);
    float distance=min(fract(p),1.0-fract(p));
    return 1.0-smoothstep(width-aa,width+aa,distance);
}
float sunVisibility(vec3 n) {
    if (!shadows) return 1.0;
    vec3 p=shadowPos.xyz/shadowPos.w*0.5+0.5;
    if (p.x<0.0 || p.x>1.0 || p.y<0.0 || p.y>1.0 || p.z>1.0) return 1.0;
    float bias=max(0.00018,0.00065*(1.0-dot(n,normalize(vec3(-0.55,1.0,-0.8)))));
    float result=0.0;
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
        float depth=texture2D(shadowMap,p.xy+vec2(float(x),float(y))/4096.0).r;
        result += p.z-bias <= depth ? 1.0 : 0.0;
    }
    return result/9.0;
}
void main() {
    vec3 n=normalize(normal), base=tint;
    float grain=noise(uv*18.0);
    if(material==0) base*=0.94+0.09*grain;
    if(material==1) {
        vec2 p=uv*vec2(1.25,2.5); p.x+=mod(floor(p.y),2.0)*0.5;
        float mortar=max(gridLine(p.x,0.025),gridLine(p.y,0.035));
        base=mix(base*(0.82+0.25*hash(floor(p))),vec3(0.58,0.55,0.47),min(mortar,1.0));
    }
    if(material==2) {
        vec2 p=vec2(uv.x*0.7,uv.y*0.2); p.y+=mod(floor(p.x),3.0)*0.33;
        float seam=max(gridLine(p.x,0.008),gridLine(p.y,0.005));
        base*=0.82+0.25*noise(vec2(uv.x*15.0,uv.y*1.5));
        base*=1.0-min(seam,1.0)*0.35;
    }
    if(material==3) {
        vec2 p=uv*vec2(0.7,0.9); p.x+=mod(floor(p.y),2.0)*0.5;
        base*=0.7+0.3*hash(floor(p));
        base*=1.0-0.25*max(gridLine(p.x,0.012),gridLine(p.y,0.02));
    }
    if(material==4) base*=0.65+0.45*noise(uv*2.0)+0.14*grain;
    if(material==5) {
        vec2 p=uv*0.25; float seam=max(gridLine(p.x,0.006),gridLine(p.y,0.006));
        base*=0.87+0.15*grain; base*=1.0-0.25*min(seam,1.0);
    }
    if(material==7) base*=0.9+0.1*grain;
    vec3 sun=normalize(vec3(-0.55,1.0,-0.8));
    float visibility=sunVisibility(n);
    bool inside=abs(world.x)<30.05 && abs(world.z)<30.05 && world.y<14.05;
    vec3 ambient=inside ? vec3(0.16,0.19,0.23) : vec3(0.34,0.40,0.48);
    vec3 lighting=ambient+vec3(1.0,0.86,0.63)*max(dot(n,sun),0.0)*visibility*1.05;
    if(inside && lightsOn) {
        vec3 delta=vec3(0,11,0)-world;
        lighting+=vec3(0.54,0.43,0.30)*(0.35+0.65*max(dot(n,normalize(delta)),0.0))/(1.0+dot(delta,delta)*0.0015);
    }
    vec3 color=base*lighting;
    if(material==6) {
        vec3 h=normalize(sun+normalize(eye-world));
        color+=vec3(0.3)*pow(max(dot(n,h),0.0),40.0)*visibility;
    }
    if(material==9) color=base*(lightsOn ? 1.4 : 0.25);
    float fog=smoothstep(95.0,210.0,length(eye-world));
    color=pow(max(color,vec3(0.0)),vec3(1.0/2.2));
    color=mix(color,vec3(0.64,0.75,0.81),fog);
    gl_FragColor=vec4(color,1.0);
}
)GLSL";
}

void HouseScene::quad(Vec a, Vec b, Vec c, Vec d, Material m, Vec color) {
    Vec n=unit(cross(sub(b,a),sub(c,a)));
    auto vertex=[&](Vec p) {
        float u,v;
        if(std::abs(n.y)>0.5f) { u=p.x; v=p.z; }
        else if(std::abs(n.x)>0.5f) { u=p.z; v=p.y; }
        else { u=p.x; v=p.y; }
        mesh[m].push_back({p,n,u,v,color.x,color.y,color.z});
    };
    vertex(a); vertex(b); vertex(c); vertex(a); vertex(c); vertex(d);
}

void HouseScene::box(float x,float y,float z,float w,float h,float d,Material m,Vec color,bool solid) {
    float a=x-w/2,b=x+w/2,c=y,e=y+h,f=z-d/2,g=z+d/2;
    if(solid) solids.push_back({{a,c,f},{b,e,g}});
    quad({a,c,g},{b,c,g},{b,e,g},{a,e,g},m,color);
    quad({b,c,f},{a,c,f},{a,e,f},{b,e,f},m,color);
    quad({a,c,f},{a,c,g},{a,e,g},{a,e,f},m,color);
    quad({b,c,g},{b,c,f},{b,e,f},{b,e,g},m,color);
    quad({a,e,g},{b,e,g},{b,e,f},{a,e,f},m,color);
    quad({a,c,f},{b,c,f},{b,c,g},{a,c,g},m,color);
}

// Each wall is made of actual solid segments. No hidden face spans a window.
void HouseScene::wall(bool alongX,float fixed,bool doorway) {
    auto piece=[&](float lo,float hi,float bottom,float top,Material m,Vec color,float thickness=1.2f,float offset=0.0f) {
        float center=(lo+hi)/2;
        if(alongX) box(center,bottom,fixed+offset,hi-lo,top-bottom,thickness,m,color);
        else box(fixed+offset,bottom,center,thickness,top-bottom,hi-lo,m,color);
    };
    // Window openings [-22,-10] and [10,22], sill 3, lintel 10.
    const float segments[][2]={{-30,-22},{-10,10},{22,30}};
    float outside=fixed>0 ? 0.63f : -0.63f;
    for(auto& s:segments) {
        if(doorway && s[0]==-10) {
            piece(-10,-4.5f,0,height,Plaster,cream);
            piece(4.5f,10,0,height,Plaster,cream);
            piece(-4.5f,4.5f,8.5f,height,Plaster,cream);
        } else piece(s[0],s[1],0,height,Plaster,cream);
    }
    for(float center:{-16.0f,16.0f}) {
        piece(center-6,center+6,0,3,Plaster,cream);
        piece(center-6,center+6,10,height,Plaster,cream);
        piece(center-6.3f,center+6.3f,2.7f,3.1f,Stone,trim,2.0f);
        piece(center-6.3f,center+6.3f,9.9f,10.3f,Stone,trim,1.6f);
        for(float u:{center-5.9f,center+5.9f,center})
            piece(u-0.11f,u+0.11f,3.1f,9.9f,Metal,dark,0.28f);
        piece(center-6,center+6,6.4f,6.6f,Metal,dark,0.28f);
        panes.push_back({alongX ? Vec{center,6.5f,fixed} : Vec{fixed,6.5f,center},alongX});
        // Glass blocks movement and bullets, but is omitted from the sun depth pass.
        solids.push_back({alongX ? Vec{center-6,3,fixed-0.04f} : Vec{fixed-0.04f,3,center-6},
                          alongX ? Vec{center+6,10,fixed+0.04f} : Vec{fixed+0.04f,10,center+6}});
    }
    // Brick plinth and white fascia, split at the open entrance.
    if(doorway) {
        piece(-30,-4.5f,0,2.4f,Brick,{0.51f,0.30f,0.19f},0.16f,outside);
        piece(4.5f,30,0,2.4f,Brick,{0.51f,0.30f,0.19f},0.16f,outside);
        for(float u:{-4.65f,4.65f}) piece(u-0.16f,u+0.16f,0,8.7f,Wood,timber,1.6f);
        piece(-4.8f,4.8f,8.5f,8.85f,Wood,timber,1.6f);
    } else piece(-30,30,0,2.4f,Brick,{0.51f,0.30f,0.19f},0.16f,outside);
    piece(-30.7f,30.7f,13.4f,14.2f,Wood,trim,1.8f);
    // Interior skirting follows the doorway.
    float inward=-outside;
    if(doorway) { piece(-29,-4.5f,0.05f,0.42f,Wood,timber,0.15f,inward); piece(4.5f,29,0.05f,0.42f,Wood,timber,0.15f,inward); }
    else piece(-29,29,0.05f,0.42f,Wood,timber,0.15f,inward);
}

void HouseScene::tree(float x,float z,float scale) {
    box(x,0,z,0.9f*scale,9*scale,0.9f*scale,Wood,timber);
    // Layered low-poly crowns keep the geometry deterministic and inexpensive.
    for(int layer=0;layer<3;layer++) {
        float y=(5+layer*2.4f)*scale,r=(4.8f-layer)*scale;
        V top{x,y+5*scale,z};
        for(int i=0;i<10;i++) {
            float a=i*6.2831853f/10,b=(i+1)*6.2831853f/10;
            V p{x+std::cos(a)*r,y,z+std::sin(a)*r},q{x+std::cos(b)*r,y,z+std::sin(b)*r};
            quad(q,p,top,top,Leaf,{0.20f+layer*0.025f,0.33f+layer*0.03f,0.16f});
        }
    }
}

void HouseScene::build() {
    for(auto& m:mesh) m.clear();
    solids.clear();
    panes.clear();
    box(0,-0.65f,0,210,0.5f,210,Grass,{0.31f,0.40f,0.20f},false);
    // Keep the foundation below the floor finish to avoid coplanar flickering.
    box(0,-0.4f,0,62,0.28f,62,Stone,{0.49f,0.48f,0.42f});
    box(0,-0.12f,0,59,0.12f,59,Wood,{0.52f,0.36f,0.21f});
    box(0,14,0,61.4f,0.35f,61.4f,Plaster,{0.78f,0.77f,0.70f});
    wall(true,-30,false); wall(true,30,true); wall(false,-30,false); wall(false,30,false);
    // Gabled roof with proper thickness, ridge, fascia and chimney.
    for(float sign:{-1.0f,1.0f}) {
        Vec a{0,22,-33},b{sign*33,14.6f,-33},c{sign*33,14.6f,33},d{0,22,33};
        if(sign>0) quad(d,c,b,a,Roof,{0.25f,0.30f,0.32f});
        else quad(a,b,c,d,Roof,{0.25f,0.30f,0.32f});
        box(sign*32.6f,14.25f,0,0.7f,0.5f,66.8f,Wood,trim,false);
    }
    quad({-30.6f,14.2f,30.6f},{30.6f,14.2f,30.6f},{0,21.8f,30.6f},{0,21.8f,30.6f},Plaster,cream);
    quad({30.6f,14.2f,-30.6f},{-30.6f,14.2f,-30.6f},{0,21.8f,-30.6f},{0,21.8f,-30.6f},Plaster,cream);
    box(0,21.8f,0,0.5f,0.4f,66.5f,Metal,dark,false);
    box(18,14.3f,-12,3.5f,9,3.5f,Brick,{0.51f,0.30f,0.20f});
    box(18,23.3f,-12,4.2f,0.5f,4.2f,Stone,trim);
    box(18,23.81f,-12,2.5f,0.05f,2.5f,Metal,{0.07f,0.08f,0.08f},false);
    // Porch, open door swung against the interior wall, front path.
    box(0,-0.14f,37,23,0.14f,13,Stone,{0.64f,0.61f,0.53f});
    box(0,-0.13f,65,9,0.13f,44,Stone,{0.59f,0.57f,0.51f},false);
    box(0,9.5f,36,24,0.45f,14,Roof,{0.26f,0.31f,0.32f});
    for(float x:{-10.0f,10.0f}) {
        box(x,0,41,0.65f,9.5f,0.65f,Wood,trim);
        box(x,0,41,1.2f,1.2f,1.2f,Brick,{0.51f,0.30f,0.19f});
    }
    box(-4.35f,0,26,0.3f,8.1f,7.5f,Wood,{0.25f,0.29f,0.24f});
    box(-4.10f,3.8f,23,0.25f,0.18f,0.7f,Metal,{0.65f,0.55f,0.28f},false);
    // Ceiling beams and pendant light fixtures.
    for(float z:{-20.0f,0.0f,20.0f}) {
        box(0,13.3f,z,59,0.7f,0.65f,Wood,timber);
        box(0,10.9f,z,0.09f,2.4f,0.09f,Metal,dark,false);
        box(0,10.6f,z,2.8f,0.4f,1.2f,Metal,dark,false);
        box(0,10.57f,z,2.4f,0.05f,0.9f,Glow,{0.95f,0.78f,0.48f},false);
    }
    // Rear bedroom: two door openings leave an accessible central circulation route.
    box(8,0,8,0.65f,14,8,Plaster,{0.65f,0.70f,0.67f});
    box(8,0,25,0.65f,14,10,Plaster,{0.65f,0.70f,0.67f});
    box(8,8.5f,16,0.65f,5.5f,8,Plaster,{0.65f,0.70f,0.67f});
    box(12,0,4,8,14,0.65f,Plaster,{0.65f,0.70f,0.67f});
    box(27,0,4,6,14,0.65f,Plaster,{0.65f,0.70f,0.67f});
    box(20,8.5f,4,8,5.5f,0.65f,Plaster,{0.65f,0.70f,0.67f});
    // Bed, pillows, bedside cabinet and wardrobe.
    box(20,0,22,9,1.1f,12,Wood,timber);
    box(20,1.1f,22,8.7f,0.9f,11.7f,Fabric,{0.77f,0.75f,0.65f});
    box(20,2,20.7f,8.8f,0.2f,8.5f,Fabric,{0.29f,0.43f,0.47f});
    box(20,0,28,9.3f,4,0.5f,Wood,timber);
    for(float x:{17.8f,22.2f}) box(x,2,26.3f,3.6f,0.6f,2.4f,Fabric,trim);
    box(12,0,26,3,2.8f,3,Wood,{0.43f,0.30f,0.19f});
    box(27.5f,0,9,3.5f,9,6,Wood,{0.43f,0.30f,0.19f});
    // Living room: rug, sectional sofa, coffee table, shelving and books.
    box(-16,0.015f,-13,21,0.04f,20,Fabric,{0.25f,0.34f,0.34f},false);
    box(-24,0,-12,5,1.4f,17,Wood,timber);
    box(-26,1.4f,-12,1.2f,3.2f,17,Fabric,{0.42f,0.49f,0.42f});
    for(float z:{-18.0f,-13.0f,-8.0f}) box(-23.5f,1.4f,z,4.3f,1,4.7f,Fabric,{0.53f,0.59f,0.50f});
    for(float z:{-20.0f,-3.8f}) box(-24,1.4f,z,5,2.2f,0.9f,Fabric,{0.42f,0.49f,0.42f});
    box(-14,2,-12,6,0.35f,8,Wood,{0.43f,0.29f,0.17f});
    for(float x:{-16.4f,-11.6f}) for(float z:{-15.3f,-8.7f}) box(x,0,z,0.25f,2,0.25f,Metal,dark);
    box(-14,2.35f,-11,2,0.2f,2.8f,Fabric,{0.60f,0.26f,0.17f},false);
    box(-5,0,-27,8,7,1.5f,Wood,timber);
    for(int shelf=0;shelf<4;shelf++) {
        box(-5,0.5f+shelf*1.65f,-25.95f,7.5f,0.15f,1,Wood,{0.52f,0.37f,0.21f});
        for(int i=0;i<12;i++) box(-8.3f+i*0.56f,0.65f+shelf*1.65f,-25.8f,0.35f,0.8f+(i%3)*0.22f,0.7f,Fabric,
            {0.24f+(i%3)*0.15f,0.24f+(i%4)*0.09f,0.20f+(i%5)*0.06f},false);
    }
    // Kitchen cabinetry, counter, sink and stove in the rear left corner.
    for(int i=0;i<5;i++) {
        float z=7+i*4.2f;
        box(-27,0,z,4,3.2f,4,Wood,{0.28f,0.38f,0.34f});
        box(-27,3.2f,z,4.4f,0.3f,4.2f,Stone,{0.73f,0.71f,0.64f});
        box(-24.94f,2.5f,z,0.12f,0.13f,1.3f,Metal,trim,false);
    }
    box(-27,3.51f,15.4f,2.8f,0.05f,3,Metal,{0.22f,0.28f,0.28f},false);
    box(-28.5f,3.5f,15.4f,0.15f,1.5f,0.15f,Metal,trim,false);
    box(-28,4.85f,15.4f,1.2f,0.15f,0.15f,Metal,trim,false);
    for(float x:{-28.0f,-26.0f}) for(float z:{6.0f,8.0f}) box(x,3.51f,z,1.3f,0.08f,1.3f,Metal,dark,false);
    box(-20,0,27,5,8,4,Metal,{0.70f,0.73f,0.70f});
    box(-20,5.5f,24.95f,4.8f,0.08f,0.08f,Metal,dark,false);
    // Dining set (central rear area).
    box(-10,3.2f,16,7,0.3f,5,Wood,{0.46f,0.30f,0.16f});
    for(float x:{-12.8f,-7.2f}) for(float z:{14.0f,18.0f}) box(x,0,z,0.3f,3.2f,0.3f,Wood,timber);
    for(float x:{-12.0f,-8.0f}) for(float z:{11.6f,20.4f}) {
        box(x,1.9f,z,2.3f,0.2f,2.3f,Wood,timber);
        box(x,2.1f,z+(z<16?-1:1),2.3f,2.3f,0.18f,Wood,timber);
        for(float dx:{-0.85f,0.85f}) for(float dz:{-0.85f,0.85f}) box(x+dx,0,z+dz,0.18f,1.9f,0.18f,Wood,timber);
    }
    // Fireplace contains the original animated fire, away from circulation.
    box(0,0,-0.8f,5,0.3f,4,Brick,{0.43f,0.30f,0.22f});
    box(0,0, -2.4f,5,4.5f,0.5f,Brick,{0.43f,0.30f,0.22f});
    for(float x:{-2.2f,2.2f}) box(x,0,-0.7f,0.6f,4.5f,3.2f,Brick,{0.43f,0.30f,0.22f});
    box(0,4.5f,-0.8f,5.4f,0.4f,4,Stone,trim);
    box(0,4.9f,-1.8f,2.5f,9.1f,2,Brick,{0.43f,0.30f,0.22f});
    // Garden beds, boundary fence and trees; the gate aligns with the front path.
    for(float x:{-17.0f,17.0f}) {
        box(x,0,47,10,0.6f,5,Stone,{0.49f,0.46f,0.36f});
        box(x,0.6f,47,9.4f,1.3f,4.4f,Leaf,{0.28f,0.40f,0.18f});
        for(int i=0;i<8;i++) box(x-4+i*1.1f,1.9f,47+(i%2),0.35f,0.3f,0.35f,Fabric,{0.76f,0.46f,0.17f},false);
    }
    for(int i=-8;i<=8;i++) {
        float u=i*10.0f;
        for(float side:{-85.0f,85.0f}) {
            box(side,0,u,0.55f,4,0.55f,Wood,trim);
            box(u,0,side,0.55f,4,0.55f,Wood,trim);
            if(i<8) {
                box(side,1.2f,u+5,0.25f,0.3f,10,Wood,trim);
                box(side,2.8f,u+5,0.25f,0.3f,10,Wood,trim);
                if(side<0 || (i!=-1 && i!=0)) {
                    box(u+5,1.2f,side,10,0.3f,0.25f,Wood,trim);
                    box(u+5,2.8f,side,10,0.3f,0.25f,Wood,trim);
                }
            }
        }
    }
    for(int i=0;i<7;i++) { tree(-65,-65+i*20,0.9f+(i%3)*0.2f); tree(65,-66+i*20,1.0f+(i%2)*0.3f); }
    for(int i=0;i<5;i++) tree(-42+i*21,-67,1.2f+(i%2)*0.2f);
    // Outdoor bench and stacked crates add scale and cover.
    box(43,2,42,9,0.4f,3,Wood,timber);
    box(43,2.4f,43.3f,9,2,0.3f,Wood,timber);
    for(float x:{39.5f,46.5f}) box(x,0,42,0.4f,2,2.6f,Metal,dark);
    for(int i=0;i<3;i++) box(40+i*4.3f,0,-42,4,4+(i%2)*2,4,Wood,{0.45f,0.36f,0.22f});
}

bool HouseScene::collides(float x,float y,float z,float r) const {
    if(std::abs(x)+r>worldLimit || std::abs(z)+r>worldLimit || y-r<0.05f || y+r>55) return true;
    for(const auto& b:solids)
        if(x+r>b.min.x && x-r<b.max.x && y+r>b.min.y && y-r<b.max.y && z+r>b.min.z && z-r<b.max.z) return true;
    // The pitched roof is also solid, including its overhang.
    if(std::abs(x)<33+r && std::abs(z)<33+r) {
        float roof=22-std::abs(x)*7.4f/33;
        if(y+r>roof-0.4f && y-r<roof+0.2f) return true;
    }
    // Vertical end faces of the attic, above the rectangular walls.
    if(std::abs(x)<30.6f+r && std::abs(std::abs(z)-30.6f)<r+0.1f &&
       y+r>14.2f && y-r<21.8f-std::abs(x)*7.6f/30.6f) return true;
    return false;
}

float HouseScene::trace(Vec o,Vec d,Vec& normal) const {
    float best=1e9f; normal={0,0,0};
    // Trace triangles as well as panes: this handles slopes and decorative objects
    // exactly as rendered, while movement uses simpler solid bounds.
    for(const auto& batch:mesh) for(size_t i=0;i<batch.size();i+=3) {
        V a=batch[i].p,e1=sub(batch[i+1].p,a),e2=sub(batch[i+2].p,a);
        V p=cross(d,e2),t=sub(o,a); float det=e1.x*p.x+e1.y*p.y+e1.z*p.z;
        if(std::abs(det)<1e-7f) continue;
        float inv=1/det,u=(t.x*p.x+t.y*p.y+t.z*p.z)*inv;
        if(u<0 || u>1) continue;
        V q=cross(t,e1); float v=(d.x*q.x+d.y*q.y+d.z*q.z)*inv;
        if(v<0 || u+v>1) continue;
        float dist=(e2.x*q.x+e2.y*q.y+e2.z*q.z)*inv;
        if(dist>0.001f && dist<best) { best=dist; normal=batch[i].n; if(normal.x*d.x+normal.y*d.y+normal.z*d.z>0) normal={-normal.x,-normal.y,-normal.z}; }
    }
    for(const auto& p:panes) {
        float dir=p.alongX?d.z:d.x; if(std::abs(dir)<1e-7f) continue;
        float t=((p.alongX?p.center.z:p.center.x)-(p.alongX?o.z:o.x))/dir;
        float horizontal=p.alongX?o.x+d.x*t-p.center.x:o.z+d.z*t-p.center.z;
        float y=o.y+d.y*t;
        if(t>0.001f && t<best && std::abs(horizontal)<6 && y>3 && y<10) {
            best=t; float sign=dir>0?-1.0f:1.0f; normal=p.alongX?Vec{0,0,sign}:Vec{sign,0,0};
        }
    }
    return best;
}

bool HouseScene::initializeGL() {
    program=link(vertexSource,fragmentSource);
    shadowProgram=link("#version 120\nvoid main(){gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}",
                       "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}");
    if(!program || !shadowProgram) { shutdownGL(); return false; }
    glGenBuffers(Count,buffers.data());
    for(int i=0;i<Count;i++) {
        glBindBuffer(GL_ARRAY_BUFFER,buffers[i]);
        glBufferData(GL_ARRAY_BUFFER,mesh[i].size()*sizeof(Vertex),mesh[i].data(),GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glGenTextures(1,&shadowTexture); glBindTexture(GL_TEXTURE_2D,shadowTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,4096,4096,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float border[]={1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glGenFramebuffers(1,&shadowFbo); glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTexture,0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0); glBindTexture(GL_TEXTURE_2D,0);
    if(!ok) { std::cerr<<"Cannot create sunlight depth buffer\n"; shutdownGL(); return false; }
    renderShadow();
    return true;
}

void HouseScene::submit(bool depthOnly) {
    glEnableClientState(GL_VERTEX_ARRAY);
    if(!depthOnly) { glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_COLOR_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); }
    for(int i=0;i<Count;i++) {
        glBindBuffer(GL_ARRAY_BUFFER,buffers[i]);
        glVertexPointer(3,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,p)));
        if(!depthOnly) {
            glNormalPointer(GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,n)));
            glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,u)));
            glColorPointer(3,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,r)));
            glUniform1i(glGetUniformLocation(program,"material"),i);
        }
        glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(mesh[i].size()));
    }
    glBindBuffer(GL_ARRAY_BUFFER,0); glDisableClientState(GL_VERTEX_ARRAY);
    if(!depthOnly) { glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
}

void HouseScene::renderShadow() {
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo); glViewport(0,0,4096,4096);
    glClear(GL_DEPTH_BUFFER_BIT); glUseProgram(shadowProgram);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(-125,125,-125,125,1,350);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    gluLookAt(-88,160,-128,0,0,0,0,1,0);
    float projection[16],view[16]; glGetFloatv(GL_PROJECTION_MATRIX,projection); glGetFloatv(GL_MODELVIEW_MATRIX,view);
    for(int c=0;c<4;c++) for(int r=0;r<4;r++) {
        lightMatrix[c*4+r]=0;
        for(int k=0;k<4;k++) lightMatrix[c*4+r]+=projection[k*4+r]*view[c*4+k];
    }
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f,2.0f);
    submit(true);
    glDisable(GL_POLYGON_OFFSET_FILL); glUseProgram(0);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
}

void HouseScene::draw(float x,float y,float z,bool lightsOn,bool shadows) {
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,shadowTexture); glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program,"shadowMap"),1);
    glUniformMatrix4fv(glGetUniformLocation(program,"lightMatrix"),1,GL_FALSE,lightMatrix.data());
    glUniform3f(glGetUniformLocation(program,"eye"),x,y,z);
    glUniform1i(glGetUniformLocation(program,"lightsOn"),lightsOn);
    glUniform1i(glGetUniformLocation(program,"shadows"),shadows);
    submit(false); glUseProgram(0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,0); glActiveTexture(GL_TEXTURE0);
}

void HouseScene::drawGlass(float x,float y,float z) {
    auto sorted=panes;
    auto distance=[&](const Pane& p) { return (p.center.x-x)*(p.center.x-x)+(p.center.y-y)*(p.center.y-y)+(p.center.z-z)*(p.center.z-z); };
    std::sort(sorted.begin(),sorted.end(),[&](const Pane& a,const Pane& b){return distance(a)>distance(b);});
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    for(const auto& p:sorted) {
        float facing=std::abs(p.alongX?z-p.center.z:x-p.center.x)/std::max(std::sqrt(distance(p)),0.001f);
        glColor4f(0.60f,0.79f,0.83f,0.10f+0.22f*std::pow(1-facing,3.0f));
        auto vertex=[&](float u,float v){ if(p.alongX) glVertex3f(p.center.x+u,v,p.center.z); else glVertex3f(p.center.x,v,p.center.z+u); };
        glBegin(GL_QUADS); vertex(-5.8f,3.15f); vertex(5.8f,3.15f); vertex(5.8f,9.85f); vertex(-5.8f,9.85f); glEnd();
        glColor4f(0.85f,0.94f,1.0f,0.10f);
        glBegin(GL_QUADS); vertex(-4.8f,3.3f); vertex(-4.35f,3.3f); vertex(-1.85f,9.7f); vertex(-2.3f,9.7f); glEnd();
    }
    glPopAttrib();
}

unsigned HouseScene::triangleCount() const { unsigned n=0; for(const auto& m:mesh) n+=m.size()/3; return n; }
unsigned HouseScene::drawCalls() const { return Count; }
void HouseScene::shutdownGL() {
    glDeleteBuffers(Count,buffers.data()); buffers.fill(0);
    if(program) glDeleteProgram(program);
    if(shadowProgram) glDeleteProgram(shadowProgram);
    if(shadowFbo) glDeleteFramebuffers(1,&shadowFbo);
    if(shadowTexture) glDeleteTextures(1,&shadowTexture);
    program=shadowProgram=shadowFbo=shadowTexture=0;
}
