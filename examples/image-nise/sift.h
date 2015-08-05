/** @file     sift.h
 ** @brief    Scale Invariant Feature Transform (SIFT)
 ** @author   Andrea Vedaldi
 **/
 
/* AUTORIGHTS
Copyright 2007 (c) Andrea Vedaldi and Brian Fulkerson

This file is part of VLFeat, available in the terms of the GNU
General Public License version 2.
*/

#ifndef VL_SIFT_H
#define VL_SIFT_H

#include <stdio.h>
#include "generic.h"

#ifndef SIFT_NBO
#define SIFT_NBO    8
#endif
#ifndef SIFT_NBP
#define SIFT_NBP    4
#endif
#define SIFT_DIM    (SIFT_NBO * SIFT_NBP * SIFT_NBP)

/** @brief SIFT filter pixel type */
typedef float vl_sift_pix ;

/** ------------------------------------------------------------------
 ** @brief SIFT filter keypoint
 **
 ** This structure represent a keypoint as extracted by the SIFT
 ** filter ::VLSiftFilt.
 **/

typedef struct _VlSiftKeypoint
{
  int o ;           /**< o coordinate (octave). */
  
  int ix ;          /**< Integer unnormalized x coordinate. */
  int iy ;          /**< Integer unnormalized y coordinate. */
  int is ;          /**< Integer s coordinate. */
  
  float x ;     /**< x coordinate. */
  float y ;     /**< y coordinate. */
  float s ;     /**< s coordinate. */
  float sigma ; /**< scale. */
} VlSiftKeypoint ;

/** ------------------------------------------------------------------
 ** @brief SIFT filter
 **
 ** This filter implements the SIFT detector and descriptor.
 **/

typedef struct _VlSiftFilt
{
  double sigman ;       /**< nominal image smoothing. */
  double sigma0 ;       /**< smoothing of pyramid base. */
  double sigmak ;       /**< k-smoothing */ 
  double dsigma0 ;      /**< delta-smoothing. */
  
  int width ;           /**< image width. */
  int height ;          /**< image height. */
  int O ;               /**< number of octaves. */ 
  int S ;               /**< number of levels per octave. */
  int o_min ;           /**< minimum octave index. */
  int s_min ;           /**< minimum level index. */
  int s_max ;           /**< maximum level index. */
  int o_cur ;           /**< current octave. */
  
  vl_sift_pix *temp ;   /**< temporary pixel buffer. */
  vl_sift_pix *octave ; /**< current GSS data. */
  vl_sift_pix *dog ;    /**< current DoG data. */
  int octave_width ;    /**< current octave width. */
  int octave_height ;   /**< current octave height. */
  
  VlSiftKeypoint* keys ;/**< detected keypoints. */
  int nkeys ;           /**< number of detected keypoints. */
  int keys_res ;        /**< size of the keys buffer. */

  double peak_thresh ;  /**< peak threshold. */
  double edge_thresh ;  /**< edge threshold. */
  double norm_thresh ;  /**< norm threshold. */
  double magnif ;       /**< magnification factor. */
  double windowSize ;   /**< size of Gaussian window (in spatial bins) */

  vl_sift_pix *grad ;   /**< GSS gradient data. */
  int grad_o ;          /**< GSS gradient data octave. */

} VlSiftFilt ;

/** @name Create and destroy
 ** @{
 **/
VL_EXPORT
VlSiftFilt*  vl_sift_new    (int width, int height,
                             int noctaves, int nlevels,
                             int o_min) ;
VL_EXPORT
void         vl_sift_delete (VlSiftFilt *f) ;
/** @} */

/** @name Process data
 ** @{
 **/

VL_EXPORT
int   vl_sift_process_first_octave       (VlSiftFilt *f, 
                                          vl_sift_pix const *im) ;

VL_EXPORT
int   vl_sift_process_next_octave        (VlSiftFilt *f) ;

VL_EXPORT
void  vl_sift_detect                     (VlSiftFilt *f) ;

VL_EXPORT
int   vl_sift_calc_keypoint_orientations (VlSiftFilt *f, 
                                          double angles [4],
                                          VlSiftKeypoint const*k);
VL_EXPORT
void  vl_sift_calc_keypoint_descriptor   (VlSiftFilt *f,
                                          vl_sift_pix *descr,
                                          VlSiftKeypoint const* k,
                                          double angle) ;

VL_EXPORT
void  vl_sift_calc_raw_descriptor        (VlSiftFilt const *f,
                                          vl_sift_pix const* image,
                                          vl_sift_pix *descr,
                                          int widht, int height,
                                          double x, double y, 
                                          double s, double angle0) ;

VL_EXPORT
void  vl_sift_keypoint_init              (VlSiftFilt const *f,
                                          VlSiftKeypoint *k, 
                                          double x,
                                          double y,
                                          double sigma) ;
/** @} */

/** @name Retrieve data and parameters
 ** @{
 **/
VL_INLINE int    vl_sift_get_octave_index   (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_noctaves       (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_octave_first   (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_octave_width   (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_octave_height  (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_nlevels        (VlSiftFilt const *f) ;
VL_INLINE int    vl_sift_get_nkeypoints     (VlSiftFilt const *f) ;
VL_INLINE double vl_sift_get_peak_thresh    (VlSiftFilt const *f) ;
VL_INLINE double vl_sift_get_edge_thresh    (VlSiftFilt const *f) ;
VL_INLINE double vl_sift_get_norm_thresh    (VlSiftFilt const *f) ;
VL_INLINE double vl_sift_get_magnif         (VlSiftFilt const *f) ;
VL_INLINE double vl_sift_get_window_size    (VlSiftFilt const *f) ;

VL_INLINE vl_sift_pix *vl_sift_get_octave  (VlSiftFilt const *f, int s) ;
VL_INLINE VlSiftKeypoint const *vl_sift_get_keypoints (VlSiftFilt const *f) ;
/** @} */

/** @name Set parameters
 ** @{
 **/
VL_INLINE void vl_sift_set_peak_thresh (VlSiftFilt *f, double t) ;
VL_INLINE void vl_sift_set_edge_thresh (VlSiftFilt *f, double t) ;
VL_INLINE void vl_sift_set_norm_thresh (VlSiftFilt *f, double t) ;
VL_INLINE void vl_sift_set_magnif      (VlSiftFilt *f, double m) ;
VL_INLINE void vl_sift_set_window_size (VlSiftFilt *f, double m) ;
/** @} */

/* -------------------------------------------------------------------
 *                                     Inline functions implementation
 * ---------------------------------------------------------------- */

/** ------------------------------------------------------------------
 ** @brief Get current octave index.
 ** @param f SIFT filter.
 ** @return index of the current octave.
 **/

VL_INLINE int
vl_sift_get_octave_index (VlSiftFilt const *f) 
{
  return f-> o_cur ;
}

/** ------------------------------------------------------------------
 ** @brief Get number of octaves.
 ** @param f SIFT filter.
 ** @return number of octaves.
 **/

VL_INLINE int
vl_sift_get_noctaves (VlSiftFilt const *f) 
{
  return f-> O ;
}

/**-------------------------------------------------------------------
 ** @brief Get first octave.
 ** @param f SIFT filter.
 ** @return index of the first octave.
 **/

VL_INLINE int
vl_sift_get_octave_first (VlSiftFilt const *f) 
{
  return f-> o_min ;
}

/** ------------------------------------------------------------------
 ** @brief Get current octave width
 ** @param f SIFT filter.
 ** @return current octave width.
 **/

VL_INLINE int 
vl_sift_get_octave_width (VlSiftFilt const *f) 
{
  return f-> octave_width ; 
}

/** ------------------------------------------------------------------
 ** @brief Get current octave height
 ** @param f SIFT filter.
 ** @return current octave height.
 **/

VL_INLINE int 
vl_sift_get_octave_height (VlSiftFilt const *f) 
{
  return f-> octave_height ;
}

/** ------------------------------------------------------------------
 ** @brief Get current octave data
 ** @param f SIFT filter.
 ** @param s level index.
 **
 ** The level index @a s ranges in the interval <tt>s_min = -1</tt>
 ** and <tt> s_max = S + 2</tt>, where @c S is the number of levels
 ** per octave.
 **
 ** @return pointer to the octave data for level @a s.
 **/

VL_INLINE vl_sift_pix *
vl_sift_get_octave (VlSiftFilt const *f, int s) 
{
  int w = vl_sift_get_octave_width  (f) ;
  int h = vl_sift_get_octave_height (f) ;  
  return f->octave + w * h * (s - f->s_min) ;
}

/** ------------------------------------------------------------------
 ** @brief Get number of levels per octave
 ** @param f SIFT filter.
 ** @return number of leves per octave.
 **/

VL_INLINE int
vl_sift_get_nlevels (VlSiftFilt const *f) 
{
  return f-> S ;
}

/** ------------------------------------------------------------------
 ** @brief Get number of keypoints.
 ** @param f SIFT filter.
 ** @return number of keypoints.
 **/

VL_INLINE int 
vl_sift_get_nkeypoints (VlSiftFilt const *f) 
{
  return f-> nkeys ;
}

/** ------------------------------------------------------------------
 ** @brief Get keypoints.
 ** @param f SIFT filter.
 ** @return pointer to the keypoints list.
 **/

VL_INLINE VlSiftKeypoint const *
vl_sift_get_keypoints (VlSiftFilt const *f) 
{
  return f-> keys ;
}

/** ------------------------------------------------------------------
 ** @brief Get peaks treashold
 ** @param f SIFT filter.
 ** @return threshold ;
 **/

VL_INLINE double
vl_sift_get_peak_thresh (VlSiftFilt const *f)
{
  return f -> peak_thresh ;
}

/** ------------------------------------------------------------------
 ** @brief Get edges threshold
 ** @param f SIFT filter.
 ** @return threshold.
 **/

VL_INLINE double
vl_sift_get_edge_thresh (VlSiftFilt const *f) 
{
  return f -> edge_thresh ;
}

/** ------------------------------------------------------------------
 ** @brief Get norm threshold
 ** @param f SIFT filter.
 ** @return threshold.
 **/

VL_INLINE double
vl_sift_get_norm_thresh (VlSiftFilt const *f) 
{
  return f -> norm_thresh ;
}

/** ------------------------------------------------------------------
 ** @brief Get the magnification factor
 ** @param f SIFT filter.
 ** @return magnification factor.
 **/

VL_INLINE double
vl_sift_get_magnif (VlSiftFilt const *f) 
{
  return f -> magnif ;
}

/** ------------------------------------------------------------------
 ** @brief Get the Gaussian window size.
 ** @param f SIFT filter.
 ** @return standard deviation of the Gaussian window (in spatial bin units).
 **/

VL_INLINE double
vl_sift_get_window_size (VlSiftFilt const *f) 
{
  return f -> windowSize ;
}



/** ------------------------------------------------------------------
 ** @brief Set peaks threshold
 ** @param f SIFT filter.
 ** @param t threshold.
 **/

VL_INLINE void
vl_sift_set_peak_thresh (VlSiftFilt *f, double t) 
{
  f -> peak_thresh = t ;
}

/** ------------------------------------------------------------------
 ** @brief Set edges threshold
 ** @param f SIFT filter.
 ** @param t threshold.
 **/

VL_INLINE void
vl_sift_set_edge_thresh (VlSiftFilt *f, double t) 
{
  f -> edge_thresh = t ;
}

/** ------------------------------------------------------------------
 ** @brief Set norm threshold
 ** @param f SIFT filter.
 ** @param t threshold.
 **/

VL_INLINE void
vl_sift_set_norm_thresh (VlSiftFilt *f, double t) 
{
  f -> norm_thresh = t ;
}

/** ------------------------------------------------------------------
 ** @brief Set the magnification factor
 ** @param f SIFT filter.
 ** @param m magnification factor.
 **/

VL_INLINE void
vl_sift_set_magnif (VlSiftFilt *f, double m) 
{
  f -> magnif = m ;
}

/** ------------------------------------------------------------------
 ** @brief Set the Gaussian window size
 ** @param f SIFT filter.
 ** @param x Gaussian window size (in spatial bin units).
 **/

VL_INLINE void
vl_sift_set_window_size (VlSiftFilt *f, double x) 
{
  f -> windowSize = x ;
}

/* VL_SIFT_H */
#endif
