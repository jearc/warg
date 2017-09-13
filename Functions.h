#pragma once
#include <glm/glm.hpp>
using namespace glm;

struct Cylinder
{
  float32 h, r;
};

struct Wall
{
  vec3 p1;
  vec2 p2;
  float32 h;
};

bool intersects(vec3 pa, vec3 da, vec3 pb, vec3 db);
bool intersects(vec3 pa, Cylinder ca, vec3 pb, Cylinder cb);
bool intersects(vec3 pa, Cylinder ca, Wall w);