/** @file    mathop.h
 ** @author  Andrea Vedaldi
 ** @brief   Math operations
 **/

/* AUTORIGHTS
Copyright 2007 (c) Andrea Vedaldi and Brian Fulkerson

This file is part of VLFeat, available in the terms of the GNU
General Public License version 2.
*/

#ifndef VL_MATHOP
#define VL_MATHOP

#include "generic.h"

/** @brief Logarithm of 2 (math constant)*/
#define VL_LOG_OF_2 0.693147180559945

/** @brief Pi (math constant) */
#define VL_PI 3.141592653589793

/** @brief IEEE single precision epsilon (math constant) 
 **
 ** <code>1.0F + VL_EPSILON_F</code> is the smallest representable
 ** single precision number greater than @c 1.0F. Numerically,
 ** ::VL_EPSILON_F is equal to @f$ 2^{-23} @f$.
 **
 **/
#define VL_EPSILON_F 1.19209290E-07F

/** @brief IEEE double precision epsilon (math constant) 
 **
 ** <code>1.0 + VL_EPSILON_D</code> is the smallest representable
 ** double precision number greater than @c 1.0. Numerically,
 ** ::VL_EPSILON_D is equal to @f$ 2^{-52} @f$.
 **/
#define VL_EPSILON_D 2.220446049250313e-16

/* 
   For the code below: An ANSI C compiler takes the two expressions,
   LONG_VAR and CHAR_VAR, and implicitly casts them to the type of the
   first member of the union. Refer to K&R Second Edition Page 148,
   last paragraph.
*/

/** @brief IEEE single precision quiet NaN constant */
static union { vl_uint32 raw ; float value ; } 
  const vl_nan_f = 
    { 0x7FC00000UL } ;

/** @brief IEEE single precision infinity constant */
static union { vl_uint32 raw ; float value ; } 
  const vl_infinity_f = 
    { 0x7F800000UL } ;

/** @brief IEEE double precision quiet NaN constant */
static union { vl_uint64 raw ; double value ; } 
  const vl_nan_d = 
#ifdef VL_COMPILER_MSC
    { 0x7FF8000000000000ui64 } ;
#else
    { 0x7FF8000000000000ULL } ;
#endif

/** @brief IEEE double precision infinity constant */
static union { vl_uint64 raw ; double value ; } 
  const vl_infinity_d = 
#ifdef VL_COMPILER_MSC
    { 0x7FF0000000000000ui64 } ;
#else
    { 0x7FF0000000000000ULL } ;
#endif

/** @brief IEEE single precision NaN (not signaling) */
#define VL_NAN_F (vl_nan_f.value)

/** @brief IEEE single precision positive infinity (not signaling) */
#define VL_INFINITY_F (vl_infinity_f.value)

/** @brief IEEE double precision NaN (not signaling) */
#define VL_NAN_D (vl_nan_d.value)

/** @brief IEEE double precision positive infinity (not signaling) */
#define VL_INFINITY_D (vl_infinity_d.value)

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast <code>mod(x, 2 * VL_PI)</code>
 ** 
 ** @param x input value.
 **
 ** The function is optimized for small absolute values of @a x.
 **
 ** The result is guaranteed not to be smaller than 0. However, due to
 ** finite numerical precision and rounding errors, the result can be
 ** equal to 2 * VL_PI (for instance, if @c x is a very small negative
 ** number).
 ** 
 ** @return <code>mod(x, 2 * VL_PI)</code>
 **/
VL_INLINE
float vl_mod_2pi_f (float x)
{
  while (x > 2 * VL_PI) x -= (float) (2 * VL_PI) ;
  while (x < 0.0F     ) x += (float) (2 * VL_PI);
  return x ;
}

VL_INLINE
double vl_mod_2pi_d (double x)
{
  while (x > 2 * VL_PI) x -= 2 * VL_PI ;
  while (x < 0.0      ) x += 2 * VL_PI ;
  return x ;
}
/** @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast <code>(int) floor(x)</code>
 **
 ** @param x argument.
 ** @return @c (int) floor(x)
 **/
VL_INLINE
int
vl_floor_f (float x)
{
  int xi = (int) x ;
  if (x >= 0 || (float) xi == x) return xi ;
  else return xi - 1 ;
}

VL_INLINE
int
vl_floor_d (double x)
{
  int xi = (int) x ;
  if (x >= 0 || (double) xi == x) return xi ;
  else return xi - 1 ;
}
/* @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast @c abs(x)
 ** @param x argument.
 ** @return @c abs(x)
 **/

VL_INLINE
float
vl_abs_f (float x)
{
  return (x >= 0) ? x : -x ;
}

VL_INLINE
double
vl_abs_d (double x)
{
  return (x >= 0) ? x : -x ;
}
/** @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast @c atan2 approximation
 ** @param y argument.
 ** @param x argument.
 ** 
 ** The function computes a relatively rough but fast approximation of
 ** @c atan2(y,x).
 **
 ** @par Algorithm
 **
 ** The algorithm approximates the function @f$ f(r)=atan((1-r)/(1+r))
 ** @f$, @f$ r \in [-1,1] @f$ with a third order polynomial @f$
 ** f(r)=c_0 + c_1 r + c_2 r^2 + c_3 r^3 @f$.  To fit the polynomial
 ** we impose the constraints
 ** 
 ** @f{eqnarray*}
 ** f(+1) &=& c_0 + c_1 + c_2 + c_3  = atan(0)       = 0,\\
 ** f(-1) &=& c_0 - c_1 + c_2 - c_3  = atan(\infty)  = \pi/2,\\
 ** f(0)  &=& c_0                    = atan(1)       = \pi/4.
 ** @f}
 **
 ** The last degree of freedom is fixed by minimizing the @f$
 ** l^{\infty} @f$ error, which yields
 **
 ** @f[
 ** c_0=\pi/4, \quad
 ** c_1=-0.9675, \quad
 ** c_2=0, \quad
 ** c_3=0.1821,
 ** @f]
 **
 ** with maximum error of 0.0061 radians at 0.35 degrees.
 **
 ** @return Approximation of @c atan2(y,x).
 **/

VL_INLINE
float
vl_fast_atan2_f (float y, float x)
{
  float angle, r ;
  float const c3 = 0.1821F ;
  float const c1 = 0.9675F ;
  float abs_y    = vl_abs_f (y) + VL_EPSILON_F ;
  
  if (x >= 0) {
    r = (x - abs_y) / (x + abs_y) ;
    angle = (float) (VL_PI / 4) ;
  } else {
    r = (x + abs_y) / (abs_y - x) ;
    angle = (float) (3 * VL_PI / 4) ;
  } 
  angle += (c3*r*r - c1) * r ; 
  return (y < 0) ? - angle : angle ;
}

VL_INLINE
double
vl_fast_atan2_d (double y, double x)
{
  double angle, r ;
  double const c3 = 0.1821 ;
  double const c1 = 0.9675 ;
  double abs_y    = vl_abs_d (y) + VL_EPSILON_D ;
  
  if (x >= 0) {
    r = (x - abs_y) / (x + abs_y) ;
    angle = VL_PI / 4 ;
  } else {
    r = (x + abs_y) / (abs_y - x) ;
    angle = 3 * VL_PI / 4 ;
  } 
  angle += (c3*r*r - c1) * r ; 
  return (y < 0) ? - angle : angle ;
}
/** @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast @c resqrt approximation
 ** @param x argument.
 **
 ** The function quickly computes an approximation of @f$ x^{-1/2}
 ** @f$.
 **
 ** @par Algorithm
 **
 ** The goal is to compute @f$ y = x^{-1/2} @f$, which we do by
 ** finding the solution of @f$ 0 = f(y) = y^{-2} - x@f$ by two Newton
 ** steps. Each Newton iteration is given by
 **
 ** @f[
 **   y \leftarrow
 **   y - \frac{f(y)}{\dot f(y)} = 
 **   y + \frac{1}{2} (y-xy^3) =
 **   \frac{y}{2} \left( 3 - xy^2 \right)
 ** @f]
 **
 ** which yields a simple polynomial update rule.
 **
 ** The clever bit (attributed to either J. Carmack or G. Tarolli) is
 ** the way an initial guess @f$ y \approx x^{-1/2} @f$ is chosen.
 **
 ** @see <a href="http://www.lomont.org/Math/Papers/2003/InvSqrt.pdf">Inverse Sqare Root</a>.
 **
 ** @return Approximation of @c resqrt(x).
 **/

VL_INLINE
float
vl_fast_resqrt_f (float x)
{
  /* 32-bit version */
  union {
    float x ;
    vl_int32  i ;
  } u ;
  
  float xhalf = (float) 0.5 * x ;

  /* convert floating point value in RAW integer */
  u.x = x ;                   
  
  /* gives initial guess y0 */
  u.i = 0x5f3759df - (u.i >> 1);  
  /*u.i = 0xdf59375f - (u.i>>1);*/
  
  /* two Newton steps */
  u.x = u.x * ( (float) 1.5  - xhalf*u.x*u.x) ; 
  u.x = u.x * ( (float) 1.5  - xhalf*u.x*u.x) ;
  return u.x ;
}

VL_INLINE
double
vl_fast_resqrt_d (double x)
{
  /* 64-bit version */
  union {
    double x ;
    vl_int64  i ;
  } u ;
  
  double xhalf = (double) 0.5 * x ;
  
  /* convert floating point value in RAW integer */
  u.x = x ;                   
  
  /* gives initial guess y0 */
#ifdef VL_COMPILER_MSC
  u.i = 0x5fe6ec85e7de30dai64 - (u.i >> 1) ; 
#else
  u.i = 0x5fe6ec85e7de30daLL - (u.i >> 1) ;
#endif

  /* two Newton steps */
  u.x = u.x * ( (double) 1.5  - xhalf*u.x*u.x) ; 
  u.x = u.x * ( (double) 1.5  - xhalf*u.x*u.x) ;
  return u.x ;
}

/** @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Fast @c sqrt approximation
 ** @param x argument.
 **
 ** The function computes a fast approximation of @c sqrt(x). 
 **
 ** @par Floating-point algorithm
 **
 ** For the floating point cases, the function uses ::vl_fast_resqrt_f
 ** (or ::vl_fast_resqrt_d) to compute <code>x *
 ** vl_fast_resqrt_f(x)</code>.
 **
 ** @par Integer algorithm
 ** 
 ** We seek for the largest integer @e y such that @f$ y^2 \leq x @f$.
 ** Write @f$ y = w + b_k 2^k + z @f$ where the binary expansion of the
 ** various variable is
 **
 ** @f[
 **  x = \sum_{i=0}^{n-1} 2^i a_i, \qquad
 **  w = \sum_{i=k+1}^{m-1} 2^i b_i, \qquad
 **  z = \sum_{i=0}^{k-1} 2^i b_i.
 ** @f]
 ** 
 ** Assume @e w known. Expanding the square and using the fact that
 ** @f$ b_k^2=b_k @f$, we obtain the following constraint for @f$ b_k
 ** @f$ and @e z:
 **
 ** @f[
 **    x - w^2 \geq 2^k ( 2 w + 2^k ) b_k + z (z + 2wz + 2^{k+1}z b_k)
 ** @f]
 **
 ** A necessary condition for @f$ b_k = 1 @f$ is that this equation is
 ** satisfied for @f$ z = 0 @f$ (as the second term is always
 ** non-negative). In fact, this condition is also sufficient, since
 ** we are looking for the @e largest solution @e y.
 **
 ** This yields the following iterative algorithm. First, note that if
 ** @e x is stored in @e n bits, where @e n is even, then the integer
 ** square root does not require more than @f$ m = n / 2 @f$ bit to be
 ** stored.  Thus initially, @f$ w = 0 @f$ and @f$ k = m - 1 = n/2 - 1
 ** @f$. Then, at each iteration the equation is tested, determining
 ** @f$ b_{m-1}, b_{m-2}, ... @f$ in this order.
 **
 ** @return Approximation of @c sqrt(x).
 **/

VL_INLINE
float
vl_fast_sqrt_f (float x)
{
  return (x < 1e-8) ? 0 : x * vl_fast_resqrt_f (x) ;
}

VL_INLINE
double
vl_fast_sqrt_d (float x)
{
  return (x < 1e-8) ? 0 : x * vl_fast_resqrt_d (x) ;
}

VL_INLINE vl_uint32 vl_fast_sqrt_ui32 (vl_uint32 x) ;
VL_INLINE vl_uint16 vl_fast_sqrt_ui16 (vl_uint16 x) ;
VL_INLINE vl_uint8  vl_fast_sqrt_ui8  (vl_uint8  x) ;

/** @} */

/** @internal
 ** @{ */
#define VL_FAST_SQRT_UI(T, SFX)                 \
  T                                             \
  vl_fast_sqrt_ ## SFX (T x)                    \
  {                                             \
    T y = 0 ; /* w / 2^k */                     \
    T tmp = 0 ;                                 \
    int twok ;                                  \
                                                \
    for (twok = 8 * sizeof(T) - 2 ;             \
         twok >= 0 ; twok -= 2) {               \
      y <<= 1 ; /* y = 2 * y */                 \
      tmp = (2*y + 1) << twok ;                 \
      if (x >= tmp) {                           \
        x -= tmp ;                              \
        y += 1 ;                                \
      }                                         \
    }                                           \
    return y ;                                  \
  }


VL_FAST_SQRT_UI(vl_uint32, ui32)
VL_FAST_SQRT_UI(vl_uint16, ui16)
VL_FAST_SQRT_UI(vl_uint8,  ui8 )
/** @} */

/** ------------------------------------------------------------------
 ** @{
 ** @brief Auto distances.
 ** @param dist pointer to the distance matrix (out).
 ** @param M number of dimensions
 ** @param NX number of data points in X.
 ** @param NY number of data points in Y.
 ** @param X first data set.
 ** @param Y second data set.
 **/

float
vl_dist_l2_f (float * dist, int M, int NX, int NY,
              float const* x, float const* y) ;

float
vl_dist_l2_d (double * dist, int M, int NX, int NY,
              double const* x, double const* y) ;

/** @} */

/* VL_MATHOP */
#endif 
