#include <tinysubdiv/tinysubdiv.hpp>
#include <iomanip>
#include <iostream>
int main() {
    const tinysubdiv::Mesh mesh{{{1,1,1},{1,-1,-1},{-1,1,-1},{-1,-1,1}},{{0,1,2},{0,3,1},{0,2,3},{1,3,2}}};
    const auto result=tinysubdiv::loop(mesh,2);
    std::cout << std::setprecision(9) << result.positions.size() << ' ' << result.triangles.size() << '\n';
    for(const auto& p:result.positions) std::cout << p.x << ' ' << p.y << ' ' << p.z << '\n';
    for(const auto& t:result.triangles) std::cout << t[0] << ' ' << t[1] << ' ' << t[2] << '\n';
}
