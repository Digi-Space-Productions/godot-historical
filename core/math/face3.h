/*************************************************************************/
/*  face3.h                                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifndef FACE3_H
#define FACE3_H

#include "vector3.h"
#include "plane.h"
#include "aabb.h"
#include "transform.h"

class Face3 {
public:

        enum Side {
                SIDE_OVER,
                SIDE_UNDER,
                SIDE_SPANNING,
                SIDE_COPLANAR
        };


        Vector3 vertex[3];

        /**
         *
         * @param p_plane plane used to split the face
         * @param p_res array of at least 3 faces, amount used in functio return
         * @param p_is_point_over array of at least 3 booleans, determining which face is over the plane, amount used in functio return
         * @param _epsilon constant used for numerical error rounding, to add "thickness" to the plane (so coplanar points can happen)
         * @return amount of faces generated by the split, either 0 (means no split possible), 2 or 3
         */
                  
        int split_by_plane(const Plane& p_plane,Face3 *p_res,bool *p_is_point_over) const;

	Plane get_plane(ClockDirection p_dir=CLOCKWISE) const;
	Vector3 get_random_point_inside() const;


        Side get_side_of(const Face3& p_face,ClockDirection p_clock_dir=CLOCKWISE) const;

        bool is_degenerate() const;
	real_t get_area() const;

        Vector3 get_median_point() const;

	bool intersects_ray(const Vector3& p_from,const Vector3& p_dir,Vector3 * p_intersection=0) const;
	bool intersects_segment(const Vector3& p_from,const Vector3& p_dir,Vector3 * p_intersection=0) const;

        ClockDirection get_clock_dir() const; ///< todo, test if this is returning the proper clockwisity

        void get_support(const Vector3& p_normal,const Transform& p_transform,Vector3 *p_vertices,int* p_count,int p_max) const;
        void project_range(const Vector3& p_normal,const Transform& p_transform,float& r_min, float& r_max) const;
        
        AABB get_aabb() const {

                AABB aabb( vertex[0], Vector3() );
                aabb.expand_to( vertex[1] );
                aabb.expand_to( vertex[2] );
                return aabb;
        }

        bool intersects_aabb(const AABB& p_aabb) const;
	operator String() const;

        inline Face3() {}
        inline Face3(const Vector3 &p_v1,const Vector3 &p_v2,const Vector3 &p_v3) { vertex[0]=p_v1; vertex[1]=p_v2; vertex[2]=p_v3; }

};


#endif // FACE3_H
