#include <tinysubdiv/tinysubdiv.hpp>

int main()
{
   const tinysubdiv::Mesh triangle{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, {{0, 1, 2}}};
   return tinysubdiv::loop(triangle, 2).triangles.size() == 16 ? 0 : 1;
}
