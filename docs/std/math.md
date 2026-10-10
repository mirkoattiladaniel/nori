# std/math

```nori
import "std/math" as math
```

std/math: scalar math and the 3D linear-algebra primitives, the module's entry file.

## Scalars (this file)

Integer + floating-point math, with no libm on any target (a prerequisite for
the fully libc-free `--nolibc` Linux build). Transcendentals use range reduction + series/rational
cores (~1e-12 over the common range); only `sqrt`/`to_float`/`to_int` are compiler builtins. Float
has no `%`, hence imod.

## Linear algebra (the sibling root files)

A `glam`-equivalent: Vec2/3/4 (`vec.nori`), Mat3/Mat4 (`mat.nori`), Quat (`quat.nori`), and the
`Transform` plus the projection/view matrix constructors (`xform.nori`). Vectors, matrices and
quaternions are mathematical primitives, so they live here; the geometry built on them (rays,
boxes, planes, spheres, frusta and their intersections) lives in `std/geometry`, which imports
this module. The root files of this directory share one scope, so every name below is reachable
unqualified from any of them and as `math::<name>` from a dependent.

Conventions, fixed and asserted in the tests (change one and the tests must change with it):
  · Mat3/Mat4 are column-major, vectors are columns, mul(a, b) applies b first, translation lives
    in w_axis, and mat4_to_cols_array is memcpy-ready for a wgpu `mat4x4<f32>`.
  · Projections are right-handed with a 0..1 depth range: the wgpu/WebGPU convention, not
    OpenGL's -1..1.
  · Quaternions are scalar-last, Hamilton product, right-handed: from_axis_angle(+Y, 90 deg)
    takes +X to -Z.
  · Euler order is intrinsic Y-X-Z, so yaw stays about world up.
### `fn imax(a: Int, b: Int) -> Int`

larger of two ints.

### `fn imin(a: Int, b: Int) -> Int`

smaller of two ints.

### `fn iabs(x: Int) -> Int`

absolute value of an int.

### `fn imod(a: Int, b: Int) -> Int`

integer remainder a mod b (truncated, sign follows a). undefined if b == 0.

### `fn iclamp(x: Int, lo: Int, hi: Int) -> Int`

clamp x into [lo, hi].

### `fn isign(x: Int) -> Int`

sign of an int: 1 if positive, -1 if negative, 0 if zero.

### `fn gcd(a: Int, b: Int) -> Int`

greatest common divisor (Euclid), always non-negative. gcd(0,0) == 0.

### `fn lcm(a: Int, b: Int) -> Int`

least common multiple, always non-negative. 0 if either arg is 0.

### `fn ipow(base: Int, e: Int) -> Int`

integer power base^e by repeated multiplication. e <= 0 yields 1 (no negative-exponent support).

### `fn isqrt(n: Int) -> Int`

integer square root (floor), via Newton's method. -1 for negative input.

### `fn hex_digit(c: Int) -> Int`

value 0..15 of an ASCII hex digit, or -1.

### `fn fbits(x: Float) -> Int`

the IEEE-754 bit pattern of a double, as an Int.

`pub` because `to_str` truncates to six significant digits, so a value cannot be printed, compared
bit-for-bit, or diffed without its bits. The language's `bits_of_f64` is the reinterpretation itself
(one move between a float and a general register, no memory); this name is kept for the code that
already calls it.

Round-trips exactly: `bitsf(fbits(x)) == x` for every finite `x`, and for NaN it preserves the
payload rather than canonicalising it.

### `fn bitsf(b: Int) -> Float`

the double with this IEEE-754 bit pattern. The inverse of `fbits` (`f64_of_bits`).

### `fn trunc(x: Float) -> Float`

truncate toward zero.

### `fn floor(x: Float) -> Float`

largest integer <= x.

### `fn ceil(x: Float) -> Float`

smallest integer >= x.

### `fn round(x: Float) -> Float`

round to nearest, halves away from zero (matches C round()).

### `fn fmod(a: Float, b: Float) -> Float`

floating remainder a - trunc(a/b)*b (sign follows a); NaN if b == 0.

### `fn log(x: Float) -> Float`

natural logarithm; -inf at 0, NaN for x < 0.

### `fn log1p(x: Float) -> Float`

log(1+x), accurate for a tiny x where `log(1.0 + x)` has already lost every digit of the answer

### `fn expm1(x: Float) -> Float`

exp(x)-1, accurate for a tiny x where `exp(x) - 1.0` has cancelled the answer away

### `fn log2(x: Float) -> Float`

base-2 logarithm.

### `fn log10(x: Float) -> Float`

base-10 logarithm.

### `fn exp(x: Float) -> Float`

e^x.

### `fn pow(base: Float, ex: Float) -> Float`

base^exp. Real for base > 0; for base < 0 only integer exponents (else NaN).

### `fn hypot(x: Float, y: Float) -> Float`

sqrt(x*x + y*y) without spurious overflow/underflow.

### `fn cbrt(x: Float) -> Float`

cube root (real, handles negatives).

### `fn sin(x: Float) -> Float`

sine (radians).

### `fn cos(x: Float) -> Float`

cosine (radians).

### `fn tan(x: Float) -> Float`

tangent (radians).

### `fn atan(x: Float) -> Float`

arctangent, result in (-pi/2, pi/2).

### `fn atan2(y: Float, x: Float) -> Float`

two-argument arctangent, result in (-pi, pi].

### `fn asin(x: Float) -> Float`

arcsine, result in [-pi/2, pi/2]; NaN outside the domain [-1, 1].

`x > 1` is not pi/2. Clamping to the endpoint answers an undefined question with a number in the
expected range, which a caller cannot tell from a real result: asin(2) and asin(1) came back
identical. NaN is the IEEE answer and the one that propagates.

### `fn acos(x: Float) -> Float`

arccosine, result in [0, pi]; NaN outside the domain [-1, 1] (see `asin`).

### `fn fabs(x: Float) -> Float`

sqrt is a compiler builtin (lowered to math.sqrt), so it is not declared here.
absolute value of a float.

### `fn fmin(a: Float, b: Float) -> Float`

smaller of two floats.

### `fn fmax(a: Float, b: Float) -> Float`

larger of two floats.

### `fn fclamp(x: Float, lo: Float, hi: Float) -> Float`

clamp x into [lo, hi].

### `fn fsign(x: Float) -> Float`

sign of a float: 1.0 if positive, -1.0 if negative, 0.0 if zero.

### `fn pi() -> Float`

the constant pi.

### `fn tau() -> Float`

the constant tau (2*pi).

### `fn e() -> Float`

Euler's number e.


std/math, mat.nori: Mat3 and Mat4.

Convention: **column-major**, matching glam / wgpu / GLSL. A matrix is stored as its columns
(`x_axis`, `y_axis`, `z_axis`, `w_axis`), which is also the order `mat4_to_cols_array` writes for
a GPU upload: memcpy the array into a uniform buffer and `mat4x4<f32>` reads it unchanged.

Vectors are columns and are multiplied on the right: `v' = M * v`. So `mat4_mul(a, b)` applies
`b` first and then `a`, and a model-view-projection is `mul(proj, mul(view, model))`. The
translation of an affine matrix lives in `w_axis`.

Reading an element: `m.<col>_axis.<row>`; `m.w_axis.x` is row 0, column 3, i.e. the x
translation. There is no `m[i][j]`; the axis names are the indexing.
### `fn mat3_from_cols(x: Vec3, y: Vec3, z: Vec3) -> Mat3`

a 3x3 from its three columns.

### `fn mat3_identity() -> Mat3`

the identity.

### `fn mat3_zero() -> Mat3`

every element zero.

### `fn mat3_from_scale(s: Vec3) -> Mat3`

a diagonal (non-uniform scale) matrix.

### `fn mat3_from_mat4(a: Mat4) -> Mat3`

the upper-left 3x3 of a 4x4: the rotation/scale part, translation dropped.

### `fn mat3_from_rotation_x(r: Float) -> Mat3`

rotation about +X by `r` radians (right-handed: +Y turns toward +Z).

### `fn mat3_from_rotation_y(r: Float) -> Mat3`

rotation about +Y by `r` radians (right-handed: +Z turns toward +X).

### `fn mat3_from_rotation_z(r: Float) -> Mat3`

rotation about +Z by `r` radians (right-handed: +X turns toward +Y).

### `fn mat3_from_axis_angle(axis: Vec3, r: Float) -> Mat3`

rotation of `r` radians about a unit `axis` (Rodrigues), right-handed.

### `fn mat3_mul_vec3(a: Mat3, v: Vec3) -> Vec3`

`a * v`: the vector as a column on the right.

### `fn mat3_mul(a: Mat3, b: Mat3) -> Mat3`

`a * b`: b is applied first.

### `fn mat3_scale(a: Mat3, s: Float) -> Mat3`

every element scaled.

### `fn mat3_add(a: Mat3, b: Mat3) -> Mat3`

componentwise sum.

### `fn mat3_transpose(a: Mat3) -> Mat3`

rows and columns swapped. For a pure rotation this is also the inverse.

### `fn mat3_determinant(a: Mat3) -> Float`

the determinant: the signed volume the unit cube maps to.

### `fn mat3_inverse(a: Mat3) -> Mat3`

the inverse, via the adjugate. A singular matrix (|det| < 1e-30) yields the zero matrix rather than trapping.

### `fn mat3_normal_matrix(a: Mat4) -> Mat3`

the normal matrix: inverse-transpose of the upper-left 3x3.

Normals do not transform by the model matrix: under a non-uniform scale that tilts them off the
surface. This is the matrix that keeps them perpendicular.

### `fn mat3_approx_eq(a: Mat3, b: Mat3, eps: Float) -> Bool`

true when every element is within `eps` of the other's.

### `fn mat3_to_cols_array(a: Mat3) -> Vec<Float>`

the nine elements in column-major order, ready for a GPU upload.
Returns a `Vec<Float>` rather than a fixed-size `[Float]`; it indexes the same way.

### `fn mat4_from_cols(x: Vec4, y: Vec4, z: Vec4, w: Vec4) -> Mat4`

a 4x4 from its four columns.

### `fn mat4_identity() -> Mat4`

the identity.

### `fn mat4_zero() -> Mat4`

every element zero.

### `fn mat4_from_mat3(a: Mat3) -> Mat4`

a 3x3 lifted into the upper-left of an otherwise identity 4x4.

### `fn mat4_from_translation(t: Vec3) -> Mat4`

pure translation.

### `fn mat4_from_scale(s: Vec3) -> Mat4`

pure (possibly non-uniform) scale.

### `fn mat4_from_rotation_x(r: Float) -> Mat4`

rotation about +X by `r` radians.

### `fn mat4_from_rotation_y(r: Float) -> Mat4`

rotation about +Y by `r` radians.

### `fn mat4_from_rotation_z(r: Float) -> Mat4`

rotation about +Z by `r` radians.

### `fn mat4_from_axis_angle(axis: Vec3, r: Float) -> Mat4`

rotation of `r` radians about a unit `axis`.

### `fn mat4_mul_vec4(a: Mat4, v: Vec4) -> Vec4`

`a * v`: the vector as a column on the right.

### `fn mat4_transform_vec4(a: Mat4, v: Vec4) -> Vec4`

alias of `mat4_mul_vec4`, spelled the way the transform functions below are.

### `fn mat4_transform_point3(a: Mat4, p: Vec3) -> Vec3`

a point through an affine matrix: w = 1, translation applied, no perspective divide.

### `fn mat4_transform_vector3(a: Mat4, v: Vec3) -> Vec3`

a direction: w = 0, so translation is ignored.

### `fn mat4_project_point3(a: Mat4, p: Vec3) -> Vec3`

a point through a projective matrix: w = 1 and then the perspective divide. Use this with a
projection or a view-projection, where `mat4_transform_point3` would return an undivided clip point.

### `fn mat4_mul(a: Mat4, b: Mat4) -> Mat4`

`a * b`: b is applied first.

### `fn mat4_scale_elems(a: Mat4, s: Float) -> Mat4`

every element scaled.

### `fn mat4_add(a: Mat4, b: Mat4) -> Mat4`

componentwise sum.

### `fn mat4_transpose(a: Mat4) -> Mat4`

rows and columns swapped.

### `fn mat4_determinant(a: Mat4) -> Float`

the determinant, by Laplace expansion over complementary 2x2 minors.

### `fn mat4_inverse(a: Mat4) -> Mat4`

the inverse: the adjugate over the same twelve 2x2 minors, divided by the determinant.

A singular matrix (|det| < 1e-30) yields the zero matrix rather than trapping: a renderer that
hands in a degenerate transform gets a value it can test, not a frame of infinities.

### `fn mat4_approx_eq(a: Mat4, b: Mat4, eps: Float) -> Bool`

true when every element is within `eps` of the other's.

### `fn mat4_to_cols_array(a: Mat4) -> Vec<Float>`

the sixteen elements in column-major order: the layout a `mat4x4<f32>` uniform expects.

### `fn mat4_from_cols_array(o: Vec<Float>) -> Mat4`

a 4x4 from sixteen elements given in column-major order (the inverse of `mat4_to_cols_array`).


std/math, quat.nori: unit quaternions.

Convention: `Quat { x, y, z, w }` with the scalar last (glam / GLSL order), Hamilton product,
and right-handed rotations: `quat_from_axis_angle(vec3_y(), pi/2)` takes +X to -Z, the same
turn `mat3_from_rotation_y(pi/2)` performs. `quat_mul(a, b)` applies `b` first, exactly as
`mat4_mul` does, so a quaternion and its matrix compose in the same order.

Every rotation function here assumes a unit quaternion. The two-argument constructors produce
one; after a long chain of multiplications, renormalize with `quat_normalize`.

The Mat3/Mat4 constructors that take a Quat live here rather than in mat.nori: they are the
quaternion's business, and the module's root files share one scope, so the name is the same
either way.
### `fn quat(x: Float, y: Float, z: Float, w: Float) -> Quat`

a quaternion from raw components. Does not normalize.

### `fn quat_identity() -> Quat`

the identity rotation.

### `fn quat_from_axis_angle(axis: Vec3, angle: Float) -> Quat`

rotation of `angle` radians about `axis` (normalized here, so any non-zero axis works).

### `fn quat_from_rotation_x(r: Float) -> Quat`

rotation about +X.

### `fn quat_from_rotation_y(r: Float) -> Quat`

rotation about +Y.

### `fn quat_from_rotation_z(r: Float) -> Quat`

rotation about +Z.

### `fn quat_from_euler(x: Float, y: Float, z: Float) -> Quat`

euler angles, intrinsic Y-X-Z: the yaw/pitch/roll order.

The composed rotation is `Ry(y) * Rx(x) * Rz(z)`: roll about Z happens first, then pitch about
X, then yaw about Y. This is the order a camera or a character controller wants, because yaw is
applied last and therefore stays about the world up axis however far the thing is pitched.
`quat_to_euler` is its exact inverse away from the poles.

### `fn quat_from_mat3(a: Mat3) -> Quat`

the rotation part of a 3x3, via Shepperd's method.

`a` must be a pure rotation (orthonormal, det +1); a scaled matrix gives a scaled quaternion.

### `fn quat_from_mat4(a: Mat4) -> Quat`

the rotation part of the upper-left 3x3 of a 4x4. The matrix must carry no scale.

### `fn quat_from_rotation_arc(from: Vec3, to: Vec3) -> Quat`

the shortest rotation taking unit vector `from` to unit vector `to`.

### `fn quat_mul(a: Quat, b: Quat) -> Quat`

the Hamilton product `a * b`: `b`'s rotation is applied first.

### `fn quat_add(a: Quat, b: Quat) -> Quat`

componentwise sum: for blending, not composition.

### `fn quat_sub(a: Quat, b: Quat) -> Quat`

componentwise difference.

### `fn quat_scale(a: Quat, s: Float) -> Quat`

scale every component.

### `fn quat_neg(a: Quat) -> Quat`

negate every component. The same rotation: q and -q are the double cover's two names for it.

### `fn quat_dot(a: Quat, b: Quat) -> Float`

the four-component dot product; cos of half the angle between the two rotations.

### `fn quat_length_sq(a: Quat) -> Float`

squared norm.

### `fn quat_length(a: Quat) -> Float`

norm; 1 for a rotation.

### `fn quat_normalize(a: Quat) -> Quat`

the unit quaternion in the same direction; the identity for a (near) zero input.

### `fn quat_conjugate(a: Quat) -> Quat`

the conjugate: the inverse rotation, for a unit quaternion.

### `fn quat_inverse(a: Quat) -> Quat`

the true inverse, conjugate over the squared norm. Equals the conjugate when `a` is a unit quaternion.

### `fn quat_approx_eq(a: Quat, b: Quat, eps: Float) -> Bool`

true when every component is within `eps` of the other's. Note that `q` and `-q` are the same
rotation and this says false for them: compare `quat_dot` against ±1 if that is what you mean.

### `fn quat_angle_between(a: Quat, b: Quat) -> Float`

the angle, in radians in [0, pi], between two rotations.

### `fn quat_rotate_vec3(q: Quat, v: Vec3) -> Vec3`

rotate a vector by a unit quaternion: v + 2w(u x v) + 2(u x (u x v)), with u the vector part.

This is the expanded `q v q*`, which costs two cross products instead of two quaternion
multiplications and never leaves the imaginary part to round away.

### `fn mat3_from_quat(q: Quat) -> Mat3`

the 3x3 rotation matrix of a unit quaternion.

### `fn mat4_from_quat(q: Quat) -> Mat4`

the 4x4 rotation matrix of a unit quaternion.

### `fn mat4_from_scale_rotation_translation(s: Vec3, r: Quat, t: Vec3) -> Mat4`

scale, then rotate, then translate: the model matrix, built in one step.

Equal to `mul(from_translation(t), mul(from_quat(r), from_scale(s)))` and cheaper: the scale
only ever multiplies the corresponding column.

### `fn quat_to_axis_angle(q: Quat) -> Vec4`

axis and angle packed into a Vec4: `xyz` is the unit axis, `w` the angle in radians in [0, pi].

Nori has no tuple return, so the pair travels in a Vec4: the alternative, two `set` parameters,
reads worse at every call site. The identity yields axis +X and angle 0.

### `fn quat_to_euler(q: Quat) -> Vec3`

euler angles as a Vec3 (x = pitch, y = yaw, z = roll), the inverse of `quat_from_euler`'s Y-X-Z.

At the poles (pitch = +/-90 degrees) yaw and roll describe the same turn; roll is reported as 0
and the whole rotation is folded into yaw.

### `fn quat_nlerp(a: Quat, b: Quat, t: Float) -> Quat`

normalized linear interpolation: cheap, always a unit result, but not constant angular speed.
Takes the short way around.

### `fn quat_slerp(a: Quat, b: Quat, t: Float) -> Quat`

spherical linear interpolation: constant angular speed, the short way around.

`t = 0` returns `a` and `t = 1` returns `b` exactly (the weights are `sin(x)/sin(x)` and `0`),
so an animation never drifts at its keyframes.


std/math, vec.nori: Vec2, Vec3, Vec4.

A `glam`-equivalent for Nori. Every vector is a plain struct of `Float` fields and every
operation is a pure function that returns a new value: structs are handles here, so a bare
second name is a view and mutating one in place would need `inout`. Returning the result keeps
the whole library free of aliasing questions at the cost of nothing measurable: a Vec3 is three
slots and the compiler's escape analysis frames most of them.

Naming is `<type>_<op>`: `vec3_add`, `vec3_dot`, … There is no overloading, so the type is part
of the name rather than inferred from the arguments.

Transcendentals come from math.nori, this module's sibling root file, so they need no prefix; `sqrt`
is a compiler builtin.
### `fn vec2(x: Float, y: Float) -> Vec2`

a 2-vector from its components.

### `fn vec2_splat(s: Float) -> Vec2`

every component the same.

### `fn vec2_zero() -> Vec2`

the zero vector.

### `fn vec2_one() -> Vec2`

(1, 1).

### `fn vec2_x() -> Vec2`

(1, 0).

### `fn vec2_y() -> Vec2`

(0, 1).

### `fn vec2_add(a: Vec2, b: Vec2) -> Vec2`

componentwise sum.

### `fn vec2_sub(a: Vec2, b: Vec2) -> Vec2`

componentwise difference.

### `fn vec2_mul(a: Vec2, b: Vec2) -> Vec2`

componentwise (Hadamard) product.

### `fn vec2_div(a: Vec2, b: Vec2) -> Vec2`

componentwise quotient.

### `fn vec2_scale(a: Vec2, s: Float) -> Vec2`

scale by a scalar.

### `fn vec2_neg(a: Vec2) -> Vec2`

negate.

### `fn vec2_dot(a: Vec2, b: Vec2) -> Float`

dot product.

### `fn vec2_cross(a: Vec2, b: Vec2) -> Float`

the 2-D cross product (a scalar: the z of the 3-D cross of the lifted vectors).

### `fn vec2_length_sq(a: Vec2) -> Float`

squared length: no sqrt, so prefer it whenever only an ordering is needed.

### `fn vec2_length(a: Vec2) -> Float`

euclidean length.

### `fn vec2_distance_sq(a: Vec2, b: Vec2) -> Float`

squared distance between two points.

### `fn vec2_distance(a: Vec2, b: Vec2) -> Float`

distance between two points.

### `fn vec2_normalize(a: Vec2) -> Vec2`

unit vector in the same direction. Division by zero for the zero vector: see `vec2_normalize_or_zero`.

### `fn vec2_normalize_or_zero(a: Vec2) -> Vec2`

unit vector, or the zero vector when the input is (near) zero. The safe form for user data.

### `fn vec2_lerp(a: Vec2, b: Vec2, t: Float) -> Vec2`

linear interpolation; t = 0 gives a, t = 1 gives b.

### `fn vec2_min(a: Vec2, b: Vec2) -> Vec2`

componentwise minimum.

### `fn vec2_max(a: Vec2, b: Vec2) -> Vec2`

componentwise maximum.

### `fn vec2_abs(a: Vec2) -> Vec2`

componentwise absolute value.

### `fn vec2_floor(a: Vec2) -> Vec2`

componentwise floor.

### `fn vec2_ceil(a: Vec2) -> Vec2`

componentwise ceil.

### `fn vec2_approx_eq(a: Vec2, b: Vec2, eps: Float) -> Bool`

true when every component is within `eps` of the other's.

### `fn vec3(x: Float, y: Float, z: Float) -> Vec3`

a 3-vector from its components.

### `fn vec3_splat(s: Float) -> Vec3`

every component the same.

### `fn vec3_zero() -> Vec3`

the zero vector.

### `fn vec3_one() -> Vec3`

(1, 1, 1).

### `fn vec3_x() -> Vec3`

+X.

### `fn vec3_y() -> Vec3`

+Y.

### `fn vec3_z() -> Vec3`

+Z.

### `fn vec3_neg_x() -> Vec3`

-X.

### `fn vec3_neg_y() -> Vec3`

-Y.

### `fn vec3_neg_z() -> Vec3`

-Z: the direction a right-handed camera looks along.

### `fn vec3_add(a: Vec3, b: Vec3) -> Vec3`

componentwise sum.

### `fn vec3_sub(a: Vec3, b: Vec3) -> Vec3`

componentwise difference.

### `fn vec3_mul(a: Vec3, b: Vec3) -> Vec3`

componentwise (Hadamard) product.

### `fn vec3_div(a: Vec3, b: Vec3) -> Vec3`

componentwise quotient.

### `fn vec3_scale(a: Vec3, s: Float) -> Vec3`

scale by a scalar.

### `fn vec3_neg(a: Vec3) -> Vec3`

negate.

### `fn vec3_dot(a: Vec3, b: Vec3) -> Float`

dot product.

### `fn vec3_cross(a: Vec3, b: Vec3) -> Vec3`

right-handed cross product: x cross y == z.

### `fn vec3_length_sq(a: Vec3) -> Float`

squared length: no sqrt, so prefer it whenever only an ordering is needed.

### `fn vec3_length(a: Vec3) -> Float`

euclidean length.

### `fn vec3_distance_sq(a: Vec3, b: Vec3) -> Float`

squared distance between two points.

### `fn vec3_distance(a: Vec3, b: Vec3) -> Float`

distance between two points.

### `fn vec3_normalize(a: Vec3) -> Vec3`

unit vector in the same direction. Division by zero for the zero vector: see `vec3_normalize_or_zero`.

### `fn vec3_normalize_or_zero(a: Vec3) -> Vec3`

unit vector, or the zero vector when the input is (near) zero. The safe form for user data.

### `fn vec3_lerp(a: Vec3, b: Vec3, t: Float) -> Vec3`

linear interpolation; t = 0 gives a, t = 1 gives b.

### `fn vec3_min(a: Vec3, b: Vec3) -> Vec3`

componentwise minimum.

### `fn vec3_max(a: Vec3, b: Vec3) -> Vec3`

componentwise maximum.

### `fn vec3_abs(a: Vec3) -> Vec3`

componentwise absolute value.

### `fn vec3_floor(a: Vec3) -> Vec3`

componentwise floor.

### `fn vec3_ceil(a: Vec3) -> Vec3`

componentwise ceil.

### `fn vec3_max_element(a: Vec3) -> Float`

the largest component.

### `fn vec3_min_element(a: Vec3) -> Float`

the smallest component.

### `fn vec3_reflect(d: Vec3, n: Vec3) -> Vec3`

`d` reflected about the surface whose unit normal is `n`: d - 2(d·n)n.

### `fn vec3_project_onto(a: Vec3, b: Vec3) -> Vec3`

the component of `a` along `b`, as a vector. `b` need not be normalized; zero `b` gives zero.

### `fn vec3_reject_from(a: Vec3, b: Vec3) -> Vec3`

the part of `a` perpendicular to `b`.

### `fn vec3_angle_between(a: Vec3, b: Vec3) -> Float`

unsigned angle between two vectors, in radians, in [0, pi]. Zero for a zero-length input.

### `fn vec3_approx_eq(a: Vec3, b: Vec3, eps: Float) -> Bool`

true when every component is within `eps` of the other's.

### `fn vec3_any_orthonormal(a: Vec3) -> Vec3`

some unit vector perpendicular to `a`: for building an arbitrary basis around a direction.

### `fn vec4(x: Float, y: Float, z: Float, w: Float) -> Vec4`

a 4-vector from its components.

### `fn vec4_splat(s: Float) -> Vec4`

every component the same.

### `fn vec4_zero() -> Vec4`

the zero vector.

### `fn vec4_one() -> Vec4`

(1, 1, 1, 1).

### `fn vec4_add(a: Vec4, b: Vec4) -> Vec4`

componentwise sum.

### `fn vec4_sub(a: Vec4, b: Vec4) -> Vec4`

componentwise difference.

### `fn vec4_mul(a: Vec4, b: Vec4) -> Vec4`

componentwise (Hadamard) product.

### `fn vec4_div(a: Vec4, b: Vec4) -> Vec4`

componentwise quotient.

### `fn vec4_scale(a: Vec4, s: Float) -> Vec4`

scale by a scalar.

### `fn vec4_neg(a: Vec4) -> Vec4`

negate.

### `fn vec4_dot(a: Vec4, b: Vec4) -> Float`

dot product.

### `fn vec4_length_sq(a: Vec4) -> Float`

squared length.

### `fn vec4_length(a: Vec4) -> Float`

euclidean length.

### `fn vec4_distance_sq(a: Vec4, b: Vec4) -> Float`

squared distance between two points.

### `fn vec4_distance(a: Vec4, b: Vec4) -> Float`

distance between two points.

### `fn vec4_normalize(a: Vec4) -> Vec4`

unit vector in the same direction.

### `fn vec4_normalize_or_zero(a: Vec4) -> Vec4`

unit vector, or the zero vector when the input is (near) zero.

### `fn vec4_lerp(a: Vec4, b: Vec4, t: Float) -> Vec4`

linear interpolation; t = 0 gives a, t = 1 gives b.

### `fn vec4_min(a: Vec4, b: Vec4) -> Vec4`

componentwise minimum.

### `fn vec4_max(a: Vec4, b: Vec4) -> Vec4`

componentwise maximum.

### `fn vec4_abs(a: Vec4) -> Vec4`

componentwise absolute value.

### `fn vec4_floor(a: Vec4) -> Vec4`

componentwise floor.

### `fn vec4_ceil(a: Vec4) -> Vec4`

componentwise ceil.

### `fn vec4_approx_eq(a: Vec4, b: Vec4, eps: Float) -> Bool`

true when every component is within `eps` of the other's.

### `fn vec3_extend(a: Vec3, w: Float) -> Vec4`

Vec3 -> Vec4 with the given w. `w = 1` for a point, `w = 0` for a direction.

### `fn vec4_truncate(a: Vec4) -> Vec3`

Vec4 -> Vec3, dropping w. No divide: see `vec4_project3` for the perspective divide.

### `fn vec4_project3(a: Vec4) -> Vec3`

Vec4 -> Vec3 through the perspective divide (xyz / w).

### `fn vec2_extend(a: Vec2, z: Float) -> Vec3`

Vec2 -> Vec3 with the given z.

### `fn vec3_truncate(a: Vec3) -> Vec2`

Vec3 -> Vec2, dropping z.


std/math, xform.nori: projections, view matrices, and the TRS Transform.

Convention: everything here is right-handed with a **0..1 clip-space depth**; the
wgpu/WebGPU/Direct3D convention, not OpenGL's -1..1. A point on the near plane comes out at
`ndc.z == 0` and one on the far plane at `ndc.z == 1`. Feeding an OpenGL-convention projection
to wgpu silently loses the near half of the depth buffer, which is why the depth range is in
every one of these names' documentation rather than left to the reader.

View space is right-handed too: the camera sits at the origin looking down **-Z**, with +X right
and +Y up. X and Y in NDC still run -1..1.
### `fn perspective_rh(fov_y: Float, aspect: Float, z_near: Float, z_far: Float) -> Mat4`

right-handed perspective, depth 0..1 (wgpu convention).

`fov_y` is the full vertical field of view in radians; `aspect` is width/height. `z_near` must
be > 0: a right-handed perspective has no finite matrix through the eye point.

### `fn perspective_infinite_rh(fov_y: Float, aspect: Float, z_near: Float) -> Mat4`

right-handed perspective with the far plane at infinity, depth 0..1.

Nothing is ever clipped for being too far away, and the depth precision the far plane was
costing goes back into the near range. Depth tends to 1 as z tends to -infinity.

### `fn perspective_infinite_reverse_rh(fov_y: Float, aspect: Float, z_near: Float) -> Mat4`

right-handed reversed-Z infinite perspective: the near plane maps to 1 and infinity to 0.

The float depth buffer's exponent bunches its precision near zero, and a reversed range puts
that precision at the far distances where a conventional 0..1 depth buffer has none. Pair it
with a `GREATER` depth test and a clear to 0.

### `fn orthographic_rh(left: Float, right: Float, bottom: Float, top: Float, z_near: Float, z_far: Float) -> Mat4`

right-handed orthographic, depth 0..1 (wgpu convention).

The box `[left, right] x [bottom, top] x [-z_near, -z_far]` in view space maps onto the clip
cube. Note the z arguments are positive distances in front of the camera, as with `perspective_rh`.

### `fn look_to_rh(eye: Vec3, dir: Vec3, up: Vec3) -> Mat4`

right-handed view matrix for a camera at `eye` looking along `dir`, with `up` as the world up hint.

The result maps world space into view space, where the camera is at the origin looking down -Z.
`dir` and `up` must not be parallel; `up` need not be perpendicular to `dir` and need not be a
unit vector; only its component perpendicular to `dir` is used.

### `fn look_at_rh(eye: Vec3, target: Vec3, up: Vec3) -> Mat4`

right-handed view matrix for a camera at `eye` looking at `target`. `target` lands on -Z in view space.

### `struct Transform`

a scale/rotate/translate transform: the decomposed form of an affine `Mat4`.

Keeping the three parts apart is what lets an animation interpolate a rotation as a rotation
(`quat_slerp`) instead of blending matrix elements, which is not a rotation in between.

### `fn transform_identity() -> Transform`

the identity transform: no translation, no rotation, unit scale.

### `fn transform_new(translation: Vec3, rotation: Quat, scale: Vec3) -> Transform`

a transform from its three parts.

### `fn transform_from_translation(t: Vec3) -> Transform`

a transform that only moves.

### `fn transform_from_rotation(r: Quat) -> Transform`

a transform that only rotates.

### `fn transform_from_scale(s: Vec3) -> Transform`

a transform that only scales.

### `fn transform_to_mat4(t: Transform) -> Mat4`

the affine matrix: scale, then rotate, then translate.

### `fn transform_from_mat4(a: Mat4) -> Transform`

decompose an affine matrix back into scale, rotation and translation.

Exact for a matrix that `transform_to_mat4` produced. A matrix carrying shear has no TRS form at
all, and what comes back is the nearest one: the column lengths as the scale and the
orthonormalized basis as the rotation.

### `fn transform_mul(a: Transform, b: Transform) -> Transform`

`a * b` as transforms: `b` is applied first, matching `mat4_mul`.

Only exact when `a` has a uniform scale: a rotated non-uniform scale composed with another
scale is a shear, which no TRS can hold. Compose the matrices when that matters.

### `fn transform_inverse(t: Transform) -> Transform`

the inverse transform. Exact for a non-zero scale.

### `fn transform_point3(t: Transform, p: Vec3) -> Vec3`

a point through the transform: scaled, rotated, then translated.

### `fn transform_vector3(t: Transform, v: Vec3) -> Vec3`

a direction through the transform: scaled and rotated, never translated.

### `fn transform_right(t: Transform) -> Vec3`

the transform's local +X in world space.

### `fn transform_up(t: Transform) -> Vec3`

the transform's local +Y in world space.

### `fn transform_forward(t: Transform) -> Vec3`

the transform's local -Z in world space: the direction a camera with this transform looks.

### `fn transform_approx_eq(a: Transform, b: Transform, eps: Float) -> Bool`

true when all three parts are within `eps` componentwise.


