"""Verify the real Forest Factory lasso module without an Editor or GPU."""
import unittest

from Tests.scene.test_forest_area import run_forest_cpu_contract

CODE = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

import forest.factory;
namespace forest = epochengine::forest;

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "Forest lasso fixture line %d: %s\n", __LINE__, #expr); \
    std::exit(91); \
} } while (false)

void lasso_contract() {
 using Direction = std::array<double, 3>;
 using Footprint = forest::ForestLassoFootprint;
 const auto nearSeam = [](double x, double y) -> Direction {return {-1.0, x, y};};
 const auto nearPole = [](double x, double y) -> Direction {return {x, 1.0, y};};
 for (const auto project : {+nearSeam, +nearPole}) {
  const std::array ring{project(-.15, -.15), project(.15, -.15), project(.15, -.05),
      project(-.05, -.05), project(-.05, .15), project(-.15, .15)};
  const auto footprint = Footprint::from_directions(ring);
  CHECK(footprint && footprint->triangles().size() == ring.size() - 2u);
  for (const auto corner : ring) CHECK(footprint->contains(corner));
  CHECK(footprint->contains(project(.10, -.10)) && footprint->contains(project(-.10, .10)));
  CHECK(footprint->contains(project(.05, -.05)) && !footprint->contains(project(0., 0.)));
  CHECK(!footprint->contains({0., 0., 0.}) && !footprint->contains({1., -1., 0.}));
  auto closed = std::vector<Direction>(ring.begin(), ring.end());
  closed.push_back(ring.front());
  const auto closedFootprint = Footprint::from_directions(closed);
  CHECK(closedFootprint && closedFootprint->vertex_directions().size() == ring.size());
  const auto candidates = footprint->candidate_directions(.05, 1337u, 6371000.);
  CHECK(candidates.size() == 64u);
  CHECK(candidates == closedFootprint->candidate_directions(.05, 1337u, 6371000.));
  CHECK(candidates != footprint->candidate_directions(.05, 1338u, 6371000.));
  for (const auto direction : candidates) {
   CHECK(footprint->contains(direction));
   CHECK(std::abs(std::hypot(direction[0], direction[1], direction[2]) - 1.) < 1e-12);
  }
  for (const auto triangle : footprint->triangles()) {
   const auto vertices = footprint->vertex_directions();
   const auto a = vertices[triangle[0]], b = vertices[triangle[1]], c = vertices[triangle[2]];
   CHECK(footprint->contains({a[0] + b[0] + c[0], a[1] + b[1] + c[1], a[2] + b[2] + c[2]}));
   CHECK(a[0] * (b[1] * c[2] - b[2] * c[1]) + a[1] * (b[2] * c[0] - b[0] * c[2])
       + a[2] * (b[0] * c[1] - b[1] * c[0]) > 0.0);
  }
  std::reverse(closed.begin(), closed.end());
  const auto reversed = Footprint::from_directions(closed);
  CHECK(reversed && reversed->contains(project(.10, -.10)) && !reversed->contains(project(0., 0.)));
 }

 const auto square = [&](double half) {
  return std::array{nearSeam(-half, -half), nearSeam(half, -half),
      nearSeam(half, half), nearSeam(-half, half)};
 };
 const auto sized = Footprint::from_directions(square(.01));
 CHECK(sized && sized->angular_radius_radians() > .01);
 CHECK(sized->candidate_directions(.6, 1u, 100.).size() == 3u);
 CHECK(sized->candidate_directions(0., 1u, 100.).empty());
 CHECK(sized->candidate_directions(.6, 1u, std::numeric_limits<double>::infinity()).empty());
 const auto metreScale = Footprint::from_directions(square(5e-8));
 CHECK(metreScale && metreScale->angular_radius_radians() < 1e-7);
 const auto tinyCandidates = metreScale->candidate_directions(.001, 10u, 6371000.);
 CHECK(tinyCandidates.size() == 1u && metreScale->contains(tinyCandidates.front()));

 const std::array acrossCubeFace{Direction{1., .8, -.2}, Direction{.8, 1., -.2},
     Direction{.8, 1., .2}, Direction{1., .8, .2}};
 const auto seam = Footprint::from_directions(acrossCubeFace);
 CHECK(seam && seam->contains({1., 1., 0.}) && !seam->contains({.8, -1., .0}));

 const std::array extent{Direction{1., 0., 0.}, Direction{-1., 0., 0.}, Direction{0., 1., 0.}};
 CHECK(!Footprint::from_directions(extent));
 auto invalid = square(.1);
 std::swap(invalid[1], invalid[2]);
 CHECK(!Footprint::from_directions(invalid));
 invalid = square(.1);
 invalid[0][0] = std::numeric_limits<double>::quiet_NaN();
 CHECK(!Footprint::from_directions(invalid));
 CHECK(!Footprint::from_directions(std::array{nearSeam(0., 0.), nearSeam(.3, 0.), nearSeam(.1, 0.)}));
 const std::array touching{nearSeam(0., 0.), nearSeam(.3, 0.), nearSeam(.3, .3),
     nearSeam(.15, 0.), nearSeam(0., .3)};
 CHECK(!Footprint::from_directions(touching));
 auto collinear = std::vector<Direction>(touching.begin(), touching.end());
 collinear.erase(collinear.begin() + 3);
 collinear.insert(collinear.begin() + 1, nearSeam(.15, 0.));
 CHECK(Footprint::from_directions(collinear));

 std::vector<Direction> bounded;
 for (std::size_t index = 0; index < forest::kMaximumForestLassoVertices; ++index) {
  const auto angle = 6.283185307179586 * double(index) / double(forest::kMaximumForestLassoVertices);
  bounded.push_back(nearSeam(.1 * std::cos(angle), .1 * std::sin(angle)));
 }
 CHECK(Footprint::from_directions(bounded));
 bounded.push_back(nearSeam(.12, -.003));
 CHECK(!Footprint::from_directions(bounded));
}
int main() { lasso_contract(); }
'''


class ForestLassoGeometry(unittest.TestCase):
    def test_concave_boundaries_seams_extent_and_spherical_sampling(self):
        run_forest_cpu_contract(self, CODE, 'forest-lasso-geometry',
                                'ForestLassoGeometry', 'forest.lasso_geometry_contract.cpp')
