#include "HouseScene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}

int main() {
    try {
        HouseScene house; house.build();
        check(house.triangleCount()>3000,"house and exterior geometry missing");
        check(!house.collides(0,4,23,0.5f),"spawn must be clear");
        // Walk through the entire open doorway and porch, both directions.
        for(float z=23;z<60;z+=0.1f)
            check(!house.collides(0,4,z,0.5f),"entrance path is blocked");
        check(house.collides(7,4,30,0.5f),"entrance jamb must be solid");
        check(house.collides(-16,5,-30,0.5f),"window glass must block movement");
        check(house.collides(-16,1.5f,-30,0.5f),"window sill wall missing");
        check(house.collides(30,12,0,0.5f),"side wall missing");
        check(house.collides(0,14,10,0.5f),"ceiling missing");
        check(house.collides(95,4,40,0.5f),"world boundary missing");
        check(house.collides(20,1.5f,22,0.5f),"bed should block movement");
        check(house.collides(0,21.9f,10,0.5f),"roof should block movement");
        check(house.collides(0,18,30.6f,0.5f),"attic gable should block movement");
        for(float x=3;x<15;x+=0.1f)
            check(!house.collides(x,4,16,0.5f),"bedroom doorway blocked");
        HouseScene::Vec n{};
        float door=house.trace({0,4,36},{0,0,-1},n);
        check(door>30,"bullets should pass through the open entrance");
        float wall=house.trace({7,4,36},{0,0,-1},n);
        check(wall>4 && wall<6,"bullets must stop at exterior wall");
        check(n.z>0.9f,"exterior hit normal must face the shooter");
        float window=house.trace({-18,5,-35},{0,0,1},n);
        check(std::abs(window-5)<0.1f,"window aperture must reach glass without an opaque wall");
        for(float side:{-1.0f,1.0f}) for(float u:{-18.0f,14.0f}) {
            float front=house.trace({u,5,side*35},{0,0,-side},n);
            float lateral=house.trace({side*35,5,u},{-side,0,0},n);
            check(std::abs(front-5)<0.1f && std::abs(lateral-5)<0.1f,
                  "all eight apertures must reach transparent glass");
        }
        float ground=house.trace({45,20,55},{0,-1,0},n);
        check(ground>19 && ground<21 && n.y>0.9f,"outdoor ground ray hit missing");
        unsigned count=house.triangleCount(); house.build();
        check(house.triangleCount()==count,"rebuilding should not duplicate geometry");
        std::cout << "House geometry checks passed ("<<count<<" triangles)\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
