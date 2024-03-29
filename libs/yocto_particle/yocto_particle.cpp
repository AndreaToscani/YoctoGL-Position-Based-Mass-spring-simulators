//
// Implementation for Yocto/Particle.
//

//
// LICENSE:
//
// Copyright (c) 2020 -- 2020 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#include "yocto_particle.h"

#include <yocto/yocto_geometry.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_shape.h>
#include <iostream>
#include <unordered_set>

// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------


namespace yocto {
std::unordered_map<int, string> obj_map;
float currentAngle = 0;
bool  flag = false;
float explosion_force = 100;
particle_scene make_ptscene(
    const scene_data& ioscene, const particle_params& params) {
  // scene
  auto ptscene = particle_scene{};

  // shapes
  static auto velocity = unordered_map<string, float>{
      {"floor", 0}, {"particles", 1}, {"cloth", 0}, {"collider", 0}};
  auto iid = 0;
  for (auto& ioinstance : ioscene.instances) {
    auto& ioshape         = ioscene.shapes[ioinstance.shape];
   
    auto& iomaterial_name = ioscene.material_names[ioinstance.material];
    if (iomaterial_name == "particles") {
      std::pair<int, string> coppia (iid, iomaterial_name);
      obj_map.insert(coppia);
      add_particles(ptscene, iid++, ioshape.points, ioshape.positions,
          ioshape.radius, 1, 1);
    } else if (iomaterial_name == "cloth") {
      std::pair<int, string> coppia(iid, iomaterial_name);
      obj_map.insert(coppia);
      auto nverts = (int)ioshape.positions.size();
      add_cloth(ptscene, iid++, ioshape.quads, ioshape.positions,
          ioshape.normals, ioshape.radius, 0.5, 1 / 8000.0,
          {nverts - 1, nverts - (int)sqrt((float)nverts)});
    } else if (iomaterial_name == "collider") {
      std::pair<int, string> coppia(iid, iomaterial_name);
      obj_map.insert(coppia);
      add_collider(ptscene, iid++, ioshape.triangles, ioshape.quads,
          ioshape.positions, ioshape.normals, ioshape.radius);
    } else if (iomaterial_name == "floor") {
      std::pair<int, string> coppia(iid, iomaterial_name);
      obj_map.insert(coppia);
      add_collider(ptscene, iid++, ioshape.triangles, ioshape.quads,
          ioshape.positions, ioshape.normals, ioshape.radius);
    } else {
      throw std::invalid_argument{"unknown material " + iomaterial_name};
    }
  }

  // done
  return ptscene;
}

void update_ioscene(scene_data& ioscene, const particle_scene& ptscene) {
  for (auto& ptshape : ptscene.shapes) {
    auto& ioshape = ioscene.shapes[ptshape.shape];
    get_positions(ptshape, ioshape.positions);
    get_normals(ptshape, ioshape.normals);
  }
}

void flatten_scene(scene_data& ioscene) {
  for (auto& ioinstance : ioscene.instances) {
    auto& shape = ioscene.shapes[ioinstance.shape];
    for (auto& position : shape.positions)
      position = transform_point(ioinstance.frame, position);
    for (auto& normal : shape.normals)
      normal = transform_normal(ioinstance.frame, normal);
    ioinstance.frame = identity3x4f;
  }
}

// Scene creation
int add_particles(particle_scene& scene, int shape_id,
    const vector<int>& points, const vector<vec3f>& positions,
    const vector<float>& radius, float mass, float random_velocity) {
  auto& shape             = scene.shapes.emplace_back();
  shape.shape             = shape_id;
  shape.points            = points;
  shape.initial_positions = positions;
  shape.initial_normals.assign(shape.positions.size(), {0, 0, 1});
  shape.initial_radius = radius;
  shape.initial_invmass.assign(positions.size(), 1 / (mass * positions.size()));
  shape.initial_velocities.assign(positions.size(), {0, 0, 0});
  shape.emit_rngscale = random_velocity;
  return (int)scene.shapes.size() - 1;
}
int add_cloth(particle_scene& scene, int shape_id, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius, float mass, float coeff,
    const vector<int>& pinned) {
  auto& shape             = scene.shapes.emplace_back();
  shape.shape             = shape_id;
  shape.quads             = quads;
  shape.initial_positions = positions;
  shape.initial_normals   = normals;
  shape.initial_radius    = radius;
  shape.initial_invmass.assign(positions.size(), 1 / (mass * positions.size()));
  shape.initial_velocities.assign(positions.size(), {0, 0, 0});
  shape.initial_pinned = pinned;
  shape.spring_coeff   = coeff;
  return (int)scene.shapes.size() - 1;
}
int add_collider(particle_scene& scene, int shape_id,
    const vector<vec3i>& triangles, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius) {
  auto& collider     = scene.colliders.emplace_back();
  collider.shape     = shape_id;
  collider.quads     = quads;
  collider.triangles = triangles;
  collider.positions = positions;
  collider.normals   = normals;
  collider.radius    = radius;
  return (int)scene.colliders.size() - 1;
}

// Set shapes
void set_velocities(
    particle_shape& shape, const vec3f& velocity, float random_scale) {
  shape.emit_velocity = velocity;
  shape.emit_rngscale = random_scale;
}

// Get shape properties
void get_positions(const particle_shape& shape, vector<vec3f>& positions) {
  positions = shape.positions;
}
void get_normals(const particle_shape& shape, vector<vec3f>& normals) {
  normals = shape.normals;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------
namespace yocto {

// Init simulation
void init_simulation(particle_scene& scene, const particle_params& params) {
  auto sid = 0;
  for (int i = 0; i < scene.shapes.size(); i++) {
    particle_shape shape = scene.shapes[i];
    shape.emit_rng   = make_rng(params.seed, (sid++) * 2 + 1);
    shape.invmass    = shape.initial_invmass;
    shape.normals    = shape.initial_normals;
    shape.positions  = shape.initial_positions;
    shape.radius     = shape.initial_radius;
    shape.velocities = shape.initial_velocities;

    shape.forces.clear();
    for (int i = 0; i < shape.positions.size(); i++)
      shape.forces.push_back(zero3f);

    // initialize pinned particles
    for (auto pinned : shape.initial_pinned) {
      shape.invmass[pinned] = 0;
    }

    for (auto& velocity : shape.velocities) {
      velocity += sample_sphere(rand2f(shape.emit_rng)) * shape.emit_rngscale *
                  rand1f(shape.emit_rng);
    }
    shape.springs.clear();
    if (shape.spring_coeff > 0) {
      for (auto& edge : get_edges(shape.quads)) {
        shape.springs.push_back({edge.x, edge.y,
            distance(shape.positions[edge.x], shape.positions[edge.y]),
            shape.spring_coeff});
      }
      for (auto& quad : shape.quads) {
        shape.springs.push_back({quad.x, quad.z,
            distance(shape.positions[quad.x], shape.positions[quad.z]),
            shape.spring_coeff});
        shape.springs.push_back({quad.y, quad.w,
            distance(shape.positions[quad.y], shape.positions[quad.w]),
            shape.spring_coeff});
      }
    }
    scene.shapes[i] = shape;
  }
  for (auto& collider : scene.colliders) {
    collider.bvh = {};
    if (collider.quads.size() > 0)
      collider.bvh = make_quads_bvh(
          collider.quads, collider.positions, collider.radius);
    else
      collider.bvh = make_triangles_bvh(
          collider.triangles, collider.positions, collider.radius);
  }
}

bool collide_collider(const particle_collider& collider, const vec3f& position,
    vec3f& hit_position, vec3f& hit_normal) {
  auto ray = ray3f{position, vec3f{0, 1, 0}};
  shape_intersection isec;

  if (collider.quads.size() > 0)
    isec = intersect_quads_bvh( collider.bvh, collider.quads, collider.positions, ray);
  else
    isec = intersect_triangles_bvh(collider.bvh, collider.triangles, collider.positions, ray);

  if (!isec.hit) return false;
  if (collider.quads.size() > 0) {
    auto q        = collider.quads[isec.element];
    hit_position = interpolate_quad(collider.positions[q.x],
        collider.positions[q.y], collider.positions[q.z],
        collider.positions[q.w], isec.uv);
    hit_normal   = normalize(
        interpolate_quad(collider.normals[q.x], collider.normals[q.y],
            collider.normals[q.z], collider.normals[q.w], isec.uv));
  } else {
    auto q = collider.triangles[isec.element];

    hit_position = interpolate_triangle(collider.positions[q.x],
        collider.positions[q.y], collider.positions[q.z], isec.uv);

    hit_normal = normalize(interpolate_triangle(collider.normals[q.x],
        collider.normals[q.y], collider.normals[q.z], isec.uv));
  }

  return dot(hit_normal, ray.d) > 0;
}

// simulate mass-spring
void simulate_massspring(particle_scene& scene, const particle_params& params) {
  // YOUR CODE GOES HERE
  for (auto& shape : scene.shapes) {
    shape.old_positions = shape.positions;
  }

  // COMPUTE DYNAMICS
  for (auto& shape : scene.shapes) {
    for (int i = 0; i < params.mssteps; i++) {
      auto ddt = params.deltat / params.mssteps;
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.forces[k] = vec3f{0, -params.gravity, 0} / shape.invmass[k];
      }

      for (auto& spring : shape.springs) {
        auto& particle0 = shape.positions[spring.vert0];
        auto& particle1 = shape.positions[spring.vert1];
        auto  invmass   = shape.invmass[spring.vert0] +
                       shape.invmass[spring.vert1];

        if (!invmass) continue;

        auto delta_pos = particle1 - particle0;
        auto delta_vel = shape.velocities[spring.vert1] -
                         shape.velocities[spring.vert0];

        auto spring_dir = normalize(delta_pos);
        auto spring_len = length(delta_pos);

        auto force = spring_dir * (spring_len / spring.rest - 1.f) /
                     (spring.coeff * invmass);
        force += dot(delta_vel / spring.rest, spring_dir) * spring_dir /
                 (spring.coeff * 1000 * invmass);
        shape.forces[spring.vert0] += force;
        shape.forces[spring.vert1] -= force;
      }

      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] += ddt * shape.forces[k] * shape.invmass[k];
        shape.positions[k] += ddt * shape.velocities[k];
      }
    }
  }

  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      for (auto& collider : scene.colliders) {
        auto hitpos = zero3f, hit_normal = zero3f;
        if (collide_collider( collider, shape.positions[k], hitpos, hit_normal)) {
          shape.positions[k] = hitpos + hit_normal * 0.005f; 
          auto projection    = dot(shape.velocities[k], hit_normal);
          shape.velocities[k] =
              (shape.velocities[k] - projection * hit_normal) *
                  (1.f - params.bounce.x) -
              projection * hit_normal * (1.f - params.bounce.y);
        }
      }
    }
  }

  // ADJUST VELOCITY
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] *= (1.f - params.dumping * params.deltat);
      if (length(shape.velocities[k]) < params.minvelocity)
        shape.velocities[k] = {0, 0, 0};
    }
  }

  // RECOMPUTE NORMALS
  for (auto& shape : scene.shapes) {
    if (shape.quads.size() > 0)
      shape.normals = quads_normals(shape.quads, shape.positions);
    else
      shape.normals = triangles_normals(shape.triangles, shape.positions);
  }
}

// simulate pbd
void simulate_pbd(particle_scene& scene, const particle_params& params) {
  // YOUR CODE GOES HERE
  // SAVE OLD POSITOINS

  for (auto& shape : scene.shapes) {
    shape.old_positions = shape.positions;
  }
  // PREDICT POSITIONS
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] += vec3f{0, -params.gravity, 0} * params.deltat;
      shape.positions[k] += shape.velocities[k] * params.deltat;
      
    }
  }
  // COMPUTE COLLISIONS
  for (auto& shape : scene.shapes) {
    shape.collisions.clear();
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;

      for (auto& collider : scene.colliders) {
        auto hitpos = zero3f, hit_normal = zero3f;
        if (!collide_collider(collider, shape.positions[k], hitpos, hit_normal))
          continue;
        else {
          
          shape.collisions.push_back({k, hitpos, hit_normal});
        }
      }
    }
  }
  // SOLVE CONSTRAINTS
  for (auto& shape : scene.shapes) {
    for (int i = 0; i < params.pdbsteps; i++) {
      for (auto& spring : shape.springs) {
        auto& particle0 = shape.positions[spring.vert0];
        auto& particle1 = shape.positions[spring.vert1];

        auto invmass = shape.invmass[spring.vert0] +
                       shape.invmass[spring.vert1];

        if (!invmass) continue;

        auto dir = particle1 - particle0;
        auto len = length(dir);
        dir /= len;
        auto lambda = (1.f - spring.coeff) * (len - spring.rest) / invmass;

        shape.positions[spring.vert0] += shape.invmass[spring.vert0] * lambda *
                                         dir;
        shape.positions[spring.vert1] -= shape.invmass[spring.vert1] * lambda *
                                         dir;
      }
      for (auto& collision : shape.collisions) {
        auto& particle = shape.positions[collision.vert];
        if (!shape.invmass[collision.vert]) continue;
        auto projection = dot(particle - collision.position, collision.normal);
        if (projection >= 0) continue;
        particle += -projection * collision.normal;
      }
    }
  }
  // COMPUTE VELOCITIES
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] = (shape.positions[k] - shape.old_positions[k]) /
                            params.deltat;
    }
  }
  // VELOCITY FILTER
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] *= (1.f - params.dumping * params.deltat);
      if (length(shape.velocities[k]) < params.minvelocity)
        shape.velocities[k] = {0, 0, 0};
    }
  }
  // RECOMPUTE NORMALS
  for (auto& shape : scene.shapes) {
    if (shape.quads.size() > 0)
      shape.normals = quads_normals(shape.quads, shape.positions);
    else
      shape.normals = triangles_normals(shape.triangles, shape.positions);
  }
}

//SIMULAZIONE PIANETI 
void simulate_other_planets(particle_scene& scene, const particle_params& params) {
  auto params_g = params;
  switch (params.gravity_type) {
    case gravity_type::mercury : params_g.gravity = 3.7; break;
    case gravity_type::earth: params_g.gravity = 9.8; break;
    case gravity_type::jupiter: params_g.gravity = 24.7; break;
    case gravity_type::mars: params_g.gravity = 3.7; break;
    case gravity_type::saturn: params_g.gravity = 10.4; break;
    case gravity_type::uranus: params_g.gravity = 8.8; break;
    case gravity_type::neptune: params_g.gravity = 11.1; break;
    default: throw std::invalid_argument("unknow planet type ");
  }
  simulate_pbd(scene , params_g);
}



//SIMULAZIONE VENTO 
void simulate_wind(particle_scene& scene, const particle_params& params) {
  // YOUR CODE GOES HERE
  // SAVE OLD POSITOINS
  vec3f wind_dir = {0, -0.2, -1};
  float wind_strength = 100;
  
    

  for (auto& shape : scene.shapes) {
    shape.old_positions = shape.positions;
  }
  // PREDICT POSITIONS
  for (auto& shape : scene.shapes) {
    if (obj_map[shape.shape] == "cloth") {
      shape.invmass[0] = 0;
      shape.invmass[(int)(sqrt((float)shape.positions.size()) -1) ] = 0;
    }
    for (int k = 0; k < shape.positions.size(); k++) {
     // if (k == 0 || k == ((int)sqrt((float)shape.positions.size()) - 1)) shape.invmass[k] = 0;
      
      if (!shape.invmass[k]) continue;
     
      shape.velocities[k] += vec3f{0, -params.gravity, 0} * params.deltat + ((wind_dir * wind_strength*rand1f(make_rng(params.seed))*params.deltat ));
      shape.positions[k] += shape.velocities[k] * params.deltat;
    }
  }
  // COMPUTE COLLISIONS
  for (auto& shape : scene.shapes) {
    shape.collisions.clear();
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;

      for (auto& collider : scene.colliders) {
        auto hitpos = zero3f, hit_normal = zero3f;
        if (!collide_collider(collider, shape.positions[k], hitpos, hit_normal))
          continue;
        else
          shape.collisions.push_back({k, hitpos, hit_normal});
      }
    }
  }
  // SOLVE CONSTRAINTS
  for (auto& shape : scene.shapes) {
    for (int i = 0; i < params.pdbsteps; i++) {
      for (auto& spring : shape.springs) {
        auto& particle0 = shape.positions[spring.vert0];
        auto& particle1 = shape.positions[spring.vert1];

        auto invmass = shape.invmass[spring.vert0] +
                       shape.invmass[spring.vert1];

        if (!invmass) continue;

        auto dir = particle1 - particle0;
        auto len = length(dir);
        dir /= len;
        auto lambda = (1.f - spring.coeff) * (len - spring.rest) / invmass;

        shape.positions[spring.vert0] += shape.invmass[spring.vert0] * lambda *
                                         dir;
        shape.positions[spring.vert1] -= shape.invmass[spring.vert1] * lambda *
                                         dir;
      }
      for (auto& collision : shape.collisions) {
        auto& particle = shape.positions[collision.vert];
        if (!shape.invmass[collision.vert]) continue;
        auto projection = dot(particle - collision.position, collision.normal);
        if (projection >= 0) continue;
        particle += -projection * collision.normal;
      }
    }
  }
  // COMPUTE VELOCITIES
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] = (shape.positions[k] - shape.old_positions[k]) /
                            params.deltat;
    }
  }
  // VELOCITY FILTER
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] *= (1.f - params.dumping * params.deltat);
      if (length(shape.velocities[k]) < params.minvelocity)
        shape.velocities[k] = {0, 0, 0};
    }
  }
  // RECOMPUTE NORMALS
  for (auto& shape : scene.shapes) {
    if (shape.quads.size() > 0)
      shape.normals = quads_normals(shape.quads, shape.positions);
    else
      shape.normals = triangles_normals(shape.triangles, shape.positions);
  }
}



//SIMULATE MAGNET

void simulate_magnet(particle_scene& scene , const particle_params& params) {
  // YOUR CODE GOES HERE
  // SAVE OLD POSITOINS

  for (auto& shape : scene.shapes) {
    shape.old_positions = shape.positions;
  }
  // PREDICT POSITIONS
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size()-1; k++) {
      if (!shape.invmass[k]) continue;
      
      shape.velocities[k] += vec3f{0, -params.gravity, 0} * params.deltat;
                             
      
      if (obj_map[shape.shape] == "particles") {
 
        
          auto m2 = shape.positions[k];
          auto dist_min = 999;
          auto pos_dist_min = scene.colliders[1].positions[0];
          /*
          for (int j = 0; j < scene.colliders[2].positions.size(); j++) {
             auto coll_pos = scene.colliders[2].positions[j];
            auto dist_sq  = distance_squared(m2, coll_pos);
            if (dist_sq < dist_min) {
              dist_min     = dist_sq;
              pos_dist_min = scene.colliders[2].positions[j];
            }
          }*/
          auto rng = make_rng(params.seed);
          auto posrandom = rand1i(rng, scene.colliders[1].positions.size() - 1);

        auto m1 = pos_dist_min;
       auto dist = distance(m1 , m2);
        
        auto f  = -1 / (4 * 3.14 * dist);
        shape.velocities[k] += (f * normalize(m2 - m1) * params.deltat);
        if (dist < 0.05) shape.velocities[k] *= 0;
      }
      shape.positions[k] += shape.velocities[k] * params.deltat;
    }
  }
  // COMPUTE COLLISIONS
  
  for (auto& shape : scene.shapes) {
    shape.collisions.clear();
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      for (auto& collider : scene.colliders) {
    
        auto hitpos = zero3f, hit_normal = zero3f;
        if (!collide_collider(collider, shape.positions[k], hitpos, hit_normal))
          continue;
        else {
          
          shape.collisions.push_back({k, hitpos, hit_normal});
        }
      }
    }
  }
  // SOLVE CONSTRAINTS
  for (auto& shape : scene.shapes) {
    for (int i = 0; i < params.pdbsteps; i++) {
      for (auto& spring : shape.springs) {
        auto& particle0 = shape.positions[spring.vert0];
        auto& particle1 = shape.positions[spring.vert1];

        auto invmass = shape.invmass[spring.vert0] +
                       shape.invmass[spring.vert1];

        if (!invmass) continue;

        auto dir = particle1 - particle0;
        auto len = length(dir);
        dir /= len;
        auto lambda = (1.f - spring.coeff) * (len - spring.rest) / invmass;

        shape.positions[spring.vert0] += shape.invmass[spring.vert0] * lambda *
                                         dir;
        shape.positions[spring.vert1] -= shape.invmass[spring.vert1] * lambda *
                                         dir;
      }
      for (auto& collision : shape.collisions) {
        auto& particle = shape.positions[collision.vert];
        
        if (!shape.invmass[collision.vert]) continue;
        auto projection = dot(particle - collision.position, collision.normal);
        if (projection >= 0) continue;
        particle += -projection * collision.normal;
       

      }
    }
  }
  // COMPUTE VELOCITIES
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] = (shape.positions[k] - shape.old_positions[k]) /
                            params.deltat;
    }
  }
  // VELOCITY FILTER
  for (auto& shape : scene.shapes) {
    for (int k = 0; k < shape.positions.size(); k++) {
      if (!shape.invmass[k]) continue;
      shape.velocities[k] *= (1.f - params.dumping * params.deltat);
      if (length(shape.velocities[k]) < params.minvelocity)
        shape.velocities[k] = {0, 0, 0};
    }
  }
  // RECOMPUTE NORMALS
  for (auto& shape : scene.shapes) {
    if (shape.quads.size() > 0)
      shape.normals = quads_normals(shape.quads, shape.positions);
    else
      shape.normals = triangles_normals(shape.triangles, shape.positions);
  }

}


void simulate_explosion(particle_scene& scene, const particle_params& params) {
 
    // YOUR CODE GOES HERE
    // SAVE OLD POSITOINS
  auto rng = make_rng(params.seed);
    auto rand_vec   = vec3f{0.8, 0.3, 0.8};
  auto rand_force = rand1i(rng,2);
  if(explosion_force > 0)  rand_force = rand1i(rng , explosion_force--);
    for (auto& shape : scene.shapes) {
      shape.old_positions = shape.positions;
    }
    // PREDICT POSITIONS
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] += vec3f{0, -params.gravity, 0} * params.deltat ;
        shape.velocities[k] += (rand_vec*normalize(shape.positions[k]) * rand_force * params.deltat);
        shape.positions[k] += shape.velocities[k] * params.deltat;
        
      }
    }
    // COMPUTE COLLISIONS
    for (auto& shape : scene.shapes) {
      shape.collisions.clear();
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;

        for (auto& collider : scene.colliders) {
          auto hitpos = zero3f, hit_normal = zero3f;
          if (!collide_collider(
                  collider, shape.positions[k], hitpos, hit_normal))
            continue;
          else {
            shape.collisions.push_back({k, hitpos, hit_normal});
          }
        }
      }
    }
    // SOLVE CONSTRAINTS
    for (auto& shape : scene.shapes) {
      for (int i = 0; i < params.pdbsteps; i++) {
        for (auto& spring : shape.springs) {
          auto& particle0 = shape.positions[spring.vert0];
          auto& particle1 = shape.positions[spring.vert1];

          auto invmass = shape.invmass[spring.vert0] +
                         shape.invmass[spring.vert1];

          if (!invmass) continue;

          auto dir = particle1 - particle0;
          auto len = length(dir);
          dir /= len;
          auto lambda = (1.f - spring.coeff) * (len - spring.rest) / invmass;

          shape.positions[spring.vert0] += shape.invmass[spring.vert0] *
                                           lambda * dir;
          shape.positions[spring.vert1] -= shape.invmass[spring.vert1] *
                                           lambda * dir;
        }
        for (auto& collision : shape.collisions) {
          auto& particle = shape.positions[collision.vert];
          if (!shape.invmass[collision.vert]) continue;
          auto projection = dot(
              particle - collision.position, collision.normal);
          if (projection >= 0) continue;
          particle += -projection * collision.normal;
        }
      }
    }
    // COMPUTE VELOCITIES
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] = (shape.positions[k] - shape.old_positions[k]) /
                              params.deltat;
      }
    }
    // VELOCITY FILTER
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] *= (1.f - params.dumping * params.deltat);
        if (length(shape.velocities[k]) < params.minvelocity)
          shape.velocities[k] = {0, 0, 0};
      }
    }
    // RECOMPUTE NORMALS
    for (auto& shape : scene.shapes) {
      if (shape.quads.size() > 0)
        shape.normals = quads_normals(shape.quads, shape.positions);
      else
        shape.normals = triangles_normals(shape.triangles, shape.positions);
    }
  }

void simulate_swarm(
      particle_scene& scene, const particle_params& params) {
    // YOUR CODE GOES HERE
    // SAVE OLD POSITOINS
  float strength = 10;
    for (auto& shape : scene.shapes) {
      shape.old_positions = shape.positions;
    }
    // PREDICT POSITIONS
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        currentAngle += 10;
        auto spinVec = vec3f{cos(currentAngle) , 0.7 , sin(currentAngle)};
        shape.velocities[k] += vec3f{0, -params.gravity/8, 0} * params.deltat;
        shape.positions[k] += shape.velocities[k] * params.deltat + ( spinVec * vec3f{0.05 , 0 , 0.05} * params.deltat );
      }
    }
    // COMPUTE COLLISIONS
    for (auto& shape : scene.shapes) {
      shape.collisions.clear();
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;

        for (auto& collider : scene.colliders) {
          auto hitpos = zero3f, hit_normal = zero3f;
          if (!collide_collider(
                  collider, shape.positions[k], hitpos, hit_normal))
            continue;
          else {
            shape.collisions.push_back({k, hitpos, hit_normal});
          }
        }
      }
    }
    // SOLVE CONSTRAINTS
    for (auto& shape : scene.shapes) {
      for (int i = 0; i < params.pdbsteps; i++) {
        for (auto& spring : shape.springs) {
          auto& particle0 = shape.positions[spring.vert0];
          auto& particle1 = shape.positions[spring.vert1];

          auto invmass = shape.invmass[spring.vert0] +
                         shape.invmass[spring.vert1];

          if (!invmass) continue;

          auto dir = particle1 - particle0;
          auto len = length(dir);
          dir /= len;
          auto lambda = (1.f - spring.coeff) * (len - spring.rest) / invmass;

          shape.positions[spring.vert0] += shape.invmass[spring.vert0] *
                                           lambda * dir;
          shape.positions[spring.vert1] -= shape.invmass[spring.vert1] *
                                           lambda * dir;
        }
        for (auto& collision : shape.collisions) {
          auto& particle = shape.positions[collision.vert];
          if (!shape.invmass[collision.vert]) continue;
          auto projection = dot(
              particle - collision.position, collision.normal);
          if (projection >= 0) continue;
          particle += -projection * collision.normal;
        }
      }
    }
    // COMPUTE VELOCITIES
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] = (shape.positions[k] - shape.old_positions[k]) /
                              params.deltat;
      }
    }
    // VELOCITY FILTER
    for (auto& shape : scene.shapes) {
      for (int k = 0; k < shape.positions.size(); k++) {
        if (!shape.invmass[k]) continue;
        shape.velocities[k] *= (1.f - params.dumping * params.deltat);
        if (length(shape.velocities[k]) < params.minvelocity)
          shape.velocities[k] = {0, 0, 0};
      }
    }
    // RECOMPUTE NORMALS
    for (auto& shape : scene.shapes) {
      if (shape.quads.size() > 0)
        shape.normals = quads_normals(shape.quads, shape.positions);
      else
        shape.normals = triangles_normals(shape.triangles, shape.positions);
    }
  }

    // Simulate one step
void simulate_frame(particle_scene& scene, const particle_params& params) {
  switch (params.solver) {
    case particle_solver_type::mass_spring:
      return simulate_massspring(scene, params);
    case particle_solver_type::position_based:
      return simulate_pbd(scene, params);
    case particle_solver_type::other_planets: 
        return simulate_other_planets(scene, params);
    case particle_solver_type::wind:
      return simulate_wind(scene , params);
    case particle_solver_type::magnet: 
        return simulate_magnet(scene, params);
    case particle_solver_type::explosion:
      return simulate_explosion(scene, params);
    case particle_solver_type::swarm:
      return simulate_swarm(scene,params);
    default: throw std::invalid_argument("unknown solver");
  
  }

}

// Simulate the whole sequence
void simulate_frames(particle_scene& scene, const particle_params& params,
    progress_callback progress_cb) {
  // handle progress
  auto progress = vec2i{0, 1 + (int)params.frames};

  if (progress_cb) progress_cb("init simulation", progress.x++, progress.y);
  init_simulation(scene, params);

  for (auto idx = 0; idx < params.frames; idx++) {
    if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
    simulate_frame(scene, params);
  }

  if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
}

}  // namespace yocto
