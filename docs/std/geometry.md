# std/geometry

```nori
import "std/geometry" as geometry
```

std/geometry, frustum.nori: the six clip planes of a view-projection and the culling tests
built on them.

Written for a **0..1 clip-space depth**, the wgpu/WebGPU convention that
`std/math`'s projections produce (not OpenGL's -1..1).
### `struct Frustum`

the six clip planes of a view-projection, each oriented so that inside is a positive distance.

### `fn frustum_from_mat4(vp: m::Mat4) -> Frustum`

the six clip planes of a view-projection matrix (Gribb-Hartmann), each normalized and oriented
so that a point inside has a positive distance from all six.

Written for a **0..1 depth** clip volume: the near plane is row 2 of the matrix on its own,
where an OpenGL -1..1 volume would use `row3 + row2`. Pass the same matrix you upload as the
camera's view-projection and the frustum matches what the GPU will clip against.

### `fn plane_from_vec4(c: m::Vec4) -> Plane`

a Vec4 of coefficients (a, b, c, d) read as the plane `ax + by + cz + d == 0`, normalized.

### `fn frustum_intersects_aabb(f: Frustum, a: Aabb) -> Bool`

false when the box is entirely outside the frustum (the cull test).

Conservative: a box straddling two planes' outsides without crossing the frustum can still
answer true. That costs a few extra draws and never drops a visible one, which is the usual trade for a
per-object cull; an exact test costs more than the draw it saves.

### `fn frustum_intersects_sphere(f: Frustum, s: Sphere) -> Bool`

false when the sphere is entirely outside the frustum.

### `fn frustum_contains_point(f: Frustum, p: m::Vec3) -> Bool`

true when the point is inside all six planes.


std/geometry: rays, boxes, planes, spheres, frusta, and the intersections a renderer actually
calls: picking (ray/triangle), physics broad-phase (AABB/AABB), and frustum culling. This file is
the module's entry and overview; the shapes live in `shapes.nori`, the ray queries in
`intersect.nori`, and the view frustum in `frustum.nori`.

Geometry is built on the primitives, so this module imports `std/math` (Vec3, Mat4, …) rather
than containing them. Everything below therefore spells a vector `m::Vec3`.

Nori has no tuple return, so a query that produces more than a Bool returns a small result
struct (`RayHit`, `AabbHit`, `TriHit`), declared here because all three sub-files produce them.
Every one carries its `hit` flag, and the other fields are only meaningful when `hit` is true;
they are left at zero otherwise so a caller that forgets to check gets an obvious value rather
than a stale one.

A `t` is always a distance along the ray in units of `dir`'s length, so a unit `dir` makes `t` a
world distance. All of these treat a ray as a half-line: an intersection behind the origin is a miss.

There is no `Point3`/`Direction3` distinction: a position and a
displacement are both `m::Vec3`, as in `glam`. That is what lets `ray.origin` and
`ray.dir` share a type and `vec3_sub` of two points give a direction, and it is also what lets a
caller pass a point where a direction was meant with no complaint from the checker. Introducing
the distinction would be a redesign of the whole math layer, not a change to this module.
### `struct RayHit`

a hit with a single parameter along the ray.

### `struct AabbHit`

a slab hit: `t_min` is where the ray enters the box and `t_max` where it leaves. For a ray that
starts inside, `t_min` is 0 and `t_max` is the exit.

### `struct TriHit`

a triangle hit with its barycentric coordinates: the point is `(1-u-v)*v0 + u*v1 + v*v2`.


std/geometry, intersect.nori: the ray queries. Every one returns a result struct from
`geometry.nori` (`RayHit`, `AabbHit`, `TriHit`) rather than a bare Bool, because Nori has no
tuple return and a hit is never just yes/no.

A ray is a half-line: an intersection behind the origin is a miss. `t` is measured in units of
`dir`'s length, so normalize the direction (`ray_normalized`) to get world distances.
### `fn ray_aabb(r: Ray, a: Aabb) -> AabbHit`

ray against an axis-aligned box, by the slab method.

A ray that starts inside hits with `t_min == 0`. Division by a zero direction component is
left unguarded: IEEE gives +/-infinity, the slab for that axis becomes
(-inf, +inf) or empty, and both are the right answer. The one case the infinities get wrong is
an origin exactly on a slab plane with a zero direction, which yields 0*inf = NaN; the
comparisons below are written so a NaN falls out as a miss.

### `fn ray_plane(r: Ray, pl: Plane) -> RayHit`

ray against an infinite plane. A ray parallel to the plane misses, even if it lies in the plane,
since there is no single `t` to report for a whole line of contact.

### `fn ray_sphere(r: Ray, s: Sphere) -> RayHit`

ray against a sphere; `t` is the near intersection, or the far one when the origin is inside.

### `fn ray_triangle(r: Ray, v0: m::Vec3, v1: m::Vec3, v2: m::Vec3) -> TriHit`

ray against a triangle, Möller-Trumbore, double-sided.

Returns the barycentric `u`, `v` alongside `t`, which is what picking wants: the hit point's
uv/normal/colour is the same interpolation of the vertices' own. Backface culling is left to the
caller (the sign of `det` before the reciprocal) because a picker usually wants both faces and a
renderer usually does not.


std/geometry, shapes.nori: the primitive shapes (Ray, Aabb, Plane, Sphere) and the queries
that involve only one or two of them. The ray-vs-shape intersections live in `intersect.nori`
and the view frustum in `frustum.nori`; the shared `RayHit`/`AabbHit`/`TriHit` result structs are
declared in `geometry.nori`, this module's entry file.
### `struct Plane`

a plane as `dot(normal, p) + d == 0`. `normal` is unit for every constructor here, which makes
`plane_signed_distance` a true distance rather than a scaled one.

### `fn ray(origin: m::Vec3, dir: m::Vec3) -> Ray`

a ray from an origin and a direction. `dir` is not normalized here; do it yourself if you want
`t` to be a world distance.

### `fn ray_normalized(origin: m::Vec3, dir: m::Vec3) -> Ray`

a ray with a unit direction, so every `t` it produces is a world distance.

### `fn ray_at(r: Ray, t: Float) -> m::Vec3`

the point at parameter `t`.

### `fn aabb(min: m::Vec3, max: m::Vec3) -> Aabb`

a box from its two corners. They are not sorted; see `aabb_from_corners` if they might be swapped.

### `fn aabb_from_corners(a: m::Vec3, b: m::Vec3) -> Aabb`

a box from two arbitrary corners, sorted componentwise.

### `fn aabb_from_center_extents(c: m::Vec3, e: m::Vec3) -> Aabb`

a box from its centre and its half-extents.

### `fn aabb_empty() -> Aabb`

the empty box: min at +infinity, max at -infinity, so merging any point into it gives that point.

This inversion is intentional: a box built by folding `aabb_merge_point` over a mesh needs a
starting value that loses to everything, and a zero-sized box at the origin would instead pull
every bound toward the origin.

### `fn aabb_is_empty(a: Aabb) -> Bool`

true when the box holds no volume (any axis inverted), which is what `aabb_empty` returns.

### `fn aabb_center(a: Aabb) -> m::Vec3`

the box's centre.

### `fn aabb_extents(a: Aabb) -> m::Vec3`

the box's half-extents: centre plus these is `max`.

### `fn aabb_size(a: Aabb) -> m::Vec3`

the box's full size along each axis.

### `fn aabb_merge(a: Aabb, b: Aabb) -> Aabb`

the smallest box containing both.

### `fn aabb_merge_point(a: Aabb, p: m::Vec3) -> Aabb`

the smallest box containing `a` and the point `p`.

### `fn aabb_expand(a: Aabb, k: Float) -> Aabb`

the box grown by `k` on every side.

### `fn aabb_contains_point(a: Aabb, p: m::Vec3) -> Bool`

true when the point is inside or on the boundary.

### `fn aabb_contains_aabb(a: Aabb, b: Aabb) -> Bool`

true when `b` lies entirely within `a`.

### `fn aabb_intersects_aabb(a: Aabb, b: Aabb) -> Bool`

true when the two boxes overlap (touching faces count).

### `fn aabb_transform(a: Aabb, tm: m::Mat4) -> Aabb`

the smallest axis-aligned box containing the transformed box.

The eight corners are never built: a box's half-extent along an axis after an affine map is the
absolute-valued matrix applied to the old half-extents, which is three dot products instead of
eight transforms and a fold.

### `fn aabb_closest_point(a: Aabb, p: m::Vec3) -> m::Vec3`

the point of the box nearest to `p` (`p` itself when it is inside).

### `fn plane_from_point_normal(p: m::Vec3, n: m::Vec3) -> Plane`

a plane through `p` with unit normal `n`.

### `fn plane_from_points(a: m::Vec3, b: m::Vec3, c: m::Vec3) -> Plane`

the plane through three points, its normal following the right-hand rule around a, b, c.

### `fn plane_signed_distance(pl: Plane, p: m::Vec3) -> Float`

signed distance from the plane to `p`: positive on the normal's side.

### `fn plane_project_point(pl: Plane, p: m::Vec3) -> m::Vec3`

`p` projected perpendicularly onto the plane.

### `fn plane_normalize(pl: Plane) -> Plane`

the same plane with a unit normal (and `d` scaled to match).

### `fn sphere(center: m::Vec3, radius: Float) -> Sphere`

a sphere from a centre and a radius.

### `fn sphere_contains_point(s: Sphere, p: m::Vec3) -> Bool`

true when `p` is inside or on the surface.

### `fn sphere_intersects_sphere(a: Sphere, b: Sphere) -> Bool`

true when the two spheres overlap.

### `fn sphere_intersects_aabb(s: Sphere, a: Aabb) -> Bool`

true when the sphere and the box overlap: the box's nearest point is within the radius.


