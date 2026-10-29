#pragma once

#include <array>
#include <vector>

// Geometry is shared by rendering, movement and bullet occlusion.
// build() is CPU-only so these rules can also be checked without a GL context.
class HouseScene {
public:
    static constexpr float halfSize = 30.0f;
    static constexpr float height = 14.0f;
    static constexpr float worldLimit = 94.0f;
    struct Vec { float x, y, z; };
    struct Bounds { Vec min, max; };
    struct Vertex { Vec p, n; float u, v, r, g, b; };
    struct Pane { Vec center; bool alongX; };

    void build();
    bool initializeGL();
    void shutdownGL();
    void renderShadow();
    void draw(float eyeX, float eyeY, float eyeZ, bool lightsOn, bool shadows);
    void drawGlass(float eyeX, float eyeY, float eyeZ);
    bool collides(float x, float y, float z, float radius) const;
    float trace(Vec origin, Vec direction, Vec& normal) const;
    unsigned triangleCount() const;
    unsigned drawCalls() const;

private:
    enum Material { Plaster, Brick, Wood, Roof, Grass, Stone, Metal, Fabric, Leaf, Glow, Count };
    std::array<std::vector<Vertex>, Count> mesh;
    std::vector<Bounds> solids;
    std::vector<Pane> panes;
    std::array<unsigned, Count> buffers{};
    unsigned program = 0, shadowProgram = 0, shadowFbo = 0, shadowTexture = 0;
    std::array<float, 16> lightMatrix{};
    void quad(Vec a, Vec b, Vec c, Vec d, Material m, Vec color);
    void box(float x, float y, float z, float w, float h, float d,
             Material m, Vec color, bool solid = true);
    void wall(bool alongX, float fixed, bool doorway);
    void tree(float x, float z, float scale);
    void submit(bool depthOnly);
};
