/*************************************************************************/
/*  collision_solver_2d_sat.cpp                                          */
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
#include "collision_solver_2d_sat.h"
#include "geometry.h"

struct _CollectorCallback2D {

	CollisionSolver2DSW::CallbackResult callback;
	void *userdata;
	bool swap;
	bool collided;
	Vector2 normal;
	Vector2 *sep_axis;

	_FORCE_INLINE_ void call(const Vector2& p_point_A, const Vector2& p_point_B) {

		//if (normal.dot(p_point_A) >= normal.dot(p_point_B))
		// return;
		if (swap)
			callback(p_point_B,p_point_A,userdata);
		else
			callback(p_point_A,p_point_B,userdata);
	}

};

typedef void (*GenerateContactsFunc)(const Vector2 *,int, const Vector2 *,int ,_CollectorCallback2D *);


_FORCE_INLINE_ static void _generate_contacts_point_point(const Vector2 * p_points_A,int p_point_count_A, const Vector2 * p_points_B,int p_point_count_B,_CollectorCallback2D *p_collector) {

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND( p_point_count_A != 1 );
	ERR_FAIL_COND( p_point_count_B != 1 );
#endif

	p_collector->call(*p_points_A,*p_points_B);
}

_FORCE_INLINE_ static void _generate_contacts_point_edge(const Vector2 * p_points_A,int p_point_count_A, const Vector2 * p_points_B,int p_point_count_B,_CollectorCallback2D *p_collector) {

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND( p_point_count_A != 1 );
	ERR_FAIL_COND( p_point_count_B != 2 );
#endif

	Vector2 closest_B = Geometry::get_closest_point_to_segment_uncapped_2d(*p_points_A, p_points_B );
	p_collector->call(*p_points_A,closest_B);

}


struct _generate_contacts_Pair {
	int idx;
	float d;
	_FORCE_INLINE_ bool operator <(const _generate_contacts_Pair& l) const { return d< l.d; }
};

_FORCE_INLINE_ static void _generate_contacts_edge_edge(const Vector2 * p_points_A,int p_point_count_A, const Vector2 * p_points_B,int p_point_count_B,_CollectorCallback2D *p_collector) {

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND( p_point_count_A != 2 );
	ERR_FAIL_COND( p_point_count_B != 2 ); // circle is actually a 4x3 matrix
#endif


	Vector2 rel_A=p_points_A[1]-p_points_A[0];
	Vector2 rel_B=p_points_B[1]-p_points_B[0];

	Vector2 t = p_collector->normal.tangent();

	real_t dA[2]={t.dot(p_points_A[0]),t.dot(p_points_A[1])};
	Vector2 pA[2]={p_points_A[0],p_points_A[1]};

	if (dA[0]>dA[1]) {
		SWAP(dA[0],dA[1]);
		SWAP(pA[0],pA[1]);
	}

	float dB[2]={t.dot(p_points_B[0]),t.dot(p_points_B[1])};
	Vector2 pB[2]={p_points_B[0],p_points_B[1]};
	if (dB[0]>dB[1]) {
		SWAP(dB[0],dB[1]);
		SWAP(pB[0],pB[1]);
	}


	if (dA[0]<dB[0]) {

		Vector2 n = (p_points_A[1]-p_points_A[0]).normalized().tangent();
		real_t d = n.dot(p_points_A[1]);

		if (dA[1]>dB[1]) {
			//A contains B
			for(int i=0;i<2;i++) {

				Vector2 b = p_points_B[i];
				Vector2 a = n.plane_project(d,b);
				if (p_collector->normal.dot(a) > p_collector->normal.dot(b)-CMP_EPSILON)
					continue;
				p_collector->call(a,b);

			}
		} else {

			// B0,A1 containment

			Vector2 n_B = (p_points_B[1]-p_points_B[0]).normalized().tangent();
			real_t d_B = n_B.dot(p_points_B[1]);

			// first, B on A

			{
				Vector2 b = p_points_B[0];
				Vector2 a = n.plane_project(d,b);
				if (p_collector->normal.dot(a) < p_collector->normal.dot(b)-CMP_EPSILON)
					p_collector->call(a,b);
			}

			// second, A on B

			{
				Vector2 a = p_points_A[1];
				Vector2 b = n_B.plane_project(d_B,a);
				if (p_collector->normal.dot(a) < p_collector->normal.dot(b)-CMP_EPSILON)
					p_collector->call(a,b);
			}



		}


	} else {

		Vector2 n = (p_points_B[1]-p_points_B[0]).normalized().tangent();
		real_t d = n.dot(p_points_B[1]);

		if (dB[1]>dA[1]) {
			//B contains A
			for(int i=0;i<2;i++) {

				Vector2 a = p_points_A[i];
				Vector2 b = n.plane_project(d,a);
				if (p_collector->normal.dot(a) > p_collector->normal.dot(b)-CMP_EPSILON)
					continue;
				p_collector->call(a,b);
			}
		} else {

			// A0,B1 containment
			Vector2 n_A = (p_points_A[1]-p_points_A[0]).normalized().tangent();
			real_t d_A = n_A.dot(p_points_A[1]);

			// first A on B

			{
				Vector2 a = p_points_A[0];
				Vector2 b = n.plane_project(d,a);
				if (p_collector->normal.dot(a) < p_collector->normal.dot(b)-CMP_EPSILON)
					p_collector->call(a,b);

			}

			//second, B on A

			{

				Vector2 b = p_points_B[1];
				Vector2 a = n_A.plane_project(d_A,b);
				if (p_collector->normal.dot(a) < p_collector->normal.dot(b)-CMP_EPSILON)
					p_collector->call(a,b);
			}

		}
	}


#if 0

	Vector2 axis = rel_A.normalized();
	Vector2 axis_B = rel_B.normalized();
	if (axis.dot(axis_B)<0)
		axis_B=-axis_B;
	axis=(axis+axis_B)*0.5;

	Vector2 normal_A = axis.tangent();
	real_t dA = normal_A.dot(p_points_A[0]);
	Vector2 normal_B = rel_B.tangent().normalized();
	real_t dB = normal_A.dot(p_points_B[0]);

	Vector2 A[4]={ normal_A.plane_project(dA,p_points_B[0]), normal_A.plane_project(dA,p_points_B[1]), p_points_A[0], p_points_A[1] };
	Vector2 B[4]={ p_points_B[0], p_points_B[1], normal_B.plane_project(dB,p_points_A[0]), normal_B.plane_project(dB,p_points_A[1]) };

	_generate_contacts_Pair dvec[4];
	for(int i=0;i<4;i++) {
		dvec[i].d=axis.dot(p_points_A[0]-A[i]);
		dvec[i].idx=i;
	}

	SortArray<_generate_contacts_Pair> sa;
	sa.sort(dvec,4);

	for(int i=1;i<=2;i++) {

		Vector2 a = A[i];
		Vector2 b = B[i];
		if (p_collector->normal.dot(a) > p_collector->normal.dot(b)-CMP_EPSILON)
			continue;
		p_collector->call(a,b);
	}

#elif 0
	Vector2 axis = rel_A.normalized(); //make an axis
	Vector2 axis_B = rel_B.normalized();
	if (axis.dot(axis_B)<0)
		axis_B=-axis_B;
	axis=(axis+axis_B)*0.5;
	Vector2 base_A = p_points_A[0] - axis * axis.dot(p_points_A[0]);
	Vector2 base_B = p_points_B[0] - axis * axis.dot(p_points_B[0]);

	//sort all 4 points in axis
	float dvec[4]={ axis.dot(p_points_A[0]), axis.dot(p_points_A[1]), axis.dot(p_points_B[0]), axis.dot(p_points_B[1]) };

	//todo , find max/min and then use 2 central points
	SortArray<float> sa;
	sa.sort(dvec,4);

	//use the middle ones as contacts
	for (int i=1;i<=2;i++) {

		Vector2 a = base_A+axis*dvec[i];
		Vector2 b = base_B+axis*dvec[i];
		if (p_collector->normal.dot(a) > p_collector->normal.dot(b)-0.01) {
			print_line("fail a: "+a);
			print_line("fail b: "+b);
			continue;
		}
		print_line("res a: "+a);
		print_line("res b: "+b);
		p_collector->call(a,b);
	}
#endif
}

static void _generate_contacts_from_supports(const Vector2 * p_points_A,int p_point_count_A, const Vector2 * p_points_B,int p_point_count_B,_CollectorCallback2D *p_collector) {


#ifdef DEBUG_ENABLED
	ERR_FAIL_COND( p_point_count_A <1 );
	ERR_FAIL_COND( p_point_count_B <1 );
#endif


	static const GenerateContactsFunc generate_contacts_func_table[2][2]={
		{
			_generate_contacts_point_point,
			_generate_contacts_point_edge,
		},{
			0,
			_generate_contacts_edge_edge,
		}
	};

	int pointcount_B;
	int pointcount_A;
	const Vector2 *points_A;
	const Vector2 *points_B;

	if (p_point_count_A > p_point_count_B) {
		//swap
		p_collector->swap = !p_collector->swap;
		p_collector->normal = -p_collector->normal;

		pointcount_B = p_point_count_A;
		pointcount_A = p_point_count_B;
		points_A=p_points_B;
		points_B=p_points_A;
	} else {

		pointcount_B = p_point_count_B;
		pointcount_A = p_point_count_A;
		points_A=p_points_A;
		points_B=p_points_B;
	}

	int version_A = (pointcount_A > 3 ?  3 : pointcount_A) -1;
	int version_B = (pointcount_B > 3 ?  3 : pointcount_B) -1;

	GenerateContactsFunc contacts_func = generate_contacts_func_table[version_A][version_B];
	ERR_FAIL_COND(!contacts_func);
	contacts_func(points_A,pointcount_A,points_B,pointcount_B,p_collector);

}



template<class ShapeA, class ShapeB>
class SeparatorAxisTest2D {

	const ShapeA *shape_A;
	const ShapeB *shape_B;
	const Matrix32 *transform_A;
	const Matrix32 *transform_B;
	const Matrix32 *transform_inv_A;
	const Matrix32 *transform_inv_B;
	real_t best_depth;
	Vector2 best_axis;
	int best_axis_count;
	int best_axis_index;
	_CollectorCallback2D *callback;

public:

	_FORCE_INLINE_ bool test_previous_axis() {

		if (callback && callback->sep_axis && *callback->sep_axis!=Vector2()) {
			return test_axis(*callback->sep_axis);
		} else {
#ifdef DEBUG_ENABLED
			best_axis_count++;
#endif

		}
		return true;
	}

	_FORCE_INLINE_ bool test_axis(const Vector2& p_axis) {

		Vector2 axis=p_axis;


		if (	Math::abs(axis.x)<CMP_EPSILON &&
			Math::abs(axis.y)<CMP_EPSILON) {
			// strange case, try an upwards separator
			axis=Vector2(0.0,1.0);
		}

		real_t min_A,max_A,min_B,max_B;

		shape_A->project_range(axis,*transform_A,min_A,max_A);
		shape_B->project_range(axis,*transform_B,min_B,max_B);

		min_B -= ( max_A - min_A ) * 0.5;
		max_B += ( max_A - min_A ) * 0.5;

		real_t dmin = min_B - ( min_A + max_A ) * 0.5;
		real_t dmax = max_B - ( min_A + max_A ) * 0.5;

		if (dmin > 0.0 || dmax < 0.0) {
			if (callback && callback->sep_axis)
				*callback->sep_axis=axis;
#ifdef DEBUG_ENABLED
			best_axis_count++;
#endif

			return false; // doesn't contain 0
		}

		//use the smallest depth

		dmin = Math::abs(dmin);

		if ( dmax < dmin ) {
			if ( dmax < best_depth ) {
				best_depth=dmax;
				best_axis=axis;
#ifdef DEBUG_ENABLED
				best_axis_index=best_axis_count;
#endif

			}
		} else {
			if ( dmin < best_depth ) {
				best_depth=dmin;
				best_axis=-axis; // keep it as A axis
#ifdef DEBUG_ENABLED
				best_axis_index=best_axis_count;
#endif
			}
		}

	//	print_line("test axis: "+p_axis+" depth: "+rtos(best_depth));
#ifdef DEBUG_ENABLED
		best_axis_count++;
#endif

		return true;
	}


	_FORCE_INLINE_ void generate_contacts() {

		// nothing to do, don't generate
		if (best_axis==Vector2(0.0,0.0))
			return;

		callback->collided=true;

		if (!callback->callback)
			return; //only collide, no callback
		static const int max_supports=2;


		Vector2 supports_A[max_supports];
		int support_count_A;
		shape_A->get_supports(transform_A->basis_xform_inv(-best_axis).normalized(),supports_A,support_count_A);
		for(int i=0;i<support_count_A;i++) {
			supports_A[i] = transform_A->xform(supports_A[i]);
		}



		Vector2 supports_B[max_supports];
		int support_count_B;
		shape_B->get_supports(transform_B->basis_xform_inv(best_axis).normalized(),supports_B,support_count_B);
		for(int i=0;i<support_count_B;i++) {
			supports_B[i] = transform_B->xform(supports_B[i]);
		}
/*


		print_line("**************************");
		printf("CBK: %p\n",callback->userdata);
		print_line("type A: "+itos(shape_A->get_type()));
		print_line("type B: "+itos(shape_B->get_type()));
		print_line("xform A: "+*transform_A);
		print_line("xform B: "+*transform_B);
		print_line("normal: "+best_axis);
		print_line("depth: "+rtos(best_depth));
		print_line("index: "+itos(best_axis_index));

		for(int i=0;i<support_count_A;i++) {

			print_line("A-"+itos(i)+": "+supports_A[i]);
		}

		for(int i=0;i<support_count_B;i++) {

			print_line("B-"+itos(i)+": "+supports_B[i]);
		}
//*/




		callback->normal=best_axis;
		_generate_contacts_from_supports(supports_A,support_count_A,supports_B,support_count_B,callback);

		if (callback && callback->sep_axis && *callback->sep_axis!=Vector2())
			*callback->sep_axis=Vector2(); //invalidate previous axis (no test)
		//CollisionSolver2DSW::CallbackResult cbk=NULL;
		//cbk(Vector2(),Vector2(),NULL);

	}

	_FORCE_INLINE_ SeparatorAxisTest2D(const ShapeA *p_shape_A,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a, const ShapeB *p_shape_B,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {
		best_depth=1e15;
		shape_A=p_shape_A;
		shape_B=p_shape_B;
		transform_A=&p_transform_a;
		transform_B=&p_transform_b;
		transform_inv_A=&p_transform_inv_a;
		transform_inv_B=&p_transform_inv_b;
		callback=p_collector;
#ifdef DEBUG_ENABLED
		best_axis_count=0;
		best_axis_index=-1;
#endif
	}

};

/****** SAT TESTS *******/
/****** SAT TESTS *******/
/****** SAT TESTS *******/
/****** SAT TESTS *******/


typedef void (*CollisionFunc)(const Shape2DSW*,const Matrix32&,const Matrix32&,const Shape2DSW*,const Matrix32&,const Matrix32&,_CollectorCallback2D *p_collector);

static void _collision_segment_segment(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const SegmentShape2DSW *segment_A = static_cast<const SegmentShape2DSW*>(p_a);
	const SegmentShape2DSW *segment_B = static_cast<const SegmentShape2DSW*>(p_b);

	SeparatorAxisTest2D<SegmentShape2DSW,SegmentShape2DSW> separator(segment_A,p_transform_a,p_transform_inv_a,segment_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_A->get_normal()).normalized()))
		return;
	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_B->get_normal()).normalized()))
		return;

	separator.generate_contacts();

}

static void _collision_segment_circle(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {


	const SegmentShape2DSW *segment_A = static_cast<const SegmentShape2DSW*>(p_a);
	const CircleShape2DSW *circle_B = static_cast<const CircleShape2DSW*>(p_b);


	SeparatorAxisTest2D<SegmentShape2DSW,CircleShape2DSW> separator(segment_A,p_transform_a,p_transform_inv_a,circle_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis(
				(p_transform_a.xform(segment_A->get_b())-p_transform_a.xform(segment_A->get_a())).normalized().tangent()
				))
		return;

//	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_A->get_normal()).normalized()))
//		return;
	if (!separator.test_axis((p_transform_a.xform(segment_A->get_a())-p_transform_b.get_origin()).normalized()))
		return;
	if (!separator.test_axis((p_transform_a.xform(segment_A->get_b())-p_transform_b.get_origin()).normalized()))
		return;


	separator.generate_contacts();


}

static void _collision_segment_rectangle(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const SegmentShape2DSW *segment_A = static_cast<const SegmentShape2DSW*>(p_a);
	const RectangleShape2DSW *rectangle_B = static_cast<const RectangleShape2DSW*>(p_b);

	SeparatorAxisTest2D<SegmentShape2DSW,RectangleShape2DSW> separator(segment_A,p_transform_a,p_transform_inv_a,rectangle_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_A->get_normal()).normalized()))
		return;

	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_b.elements[1].normalized()))
		return;

	separator.generate_contacts();

}

static void _collision_segment_capsule(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const SegmentShape2DSW *segment_A = static_cast<const SegmentShape2DSW*>(p_a);
	const CapsuleShape2DSW *capsule_B = static_cast<const CapsuleShape2DSW*>(p_b);

	SeparatorAxisTest2D<SegmentShape2DSW,CapsuleShape2DSW> separator(segment_A,p_transform_a,p_transform_inv_a,capsule_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_A->get_normal()).normalized()))
		return;

	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;

	if (!separator.test_axis((p_transform_a.xform(segment_A->get_a())-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*0.5)).normalized()))
		return;
	if (!separator.test_axis((p_transform_a.xform(segment_A->get_a())-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*-0.5)).normalized()))
		return;
	if (!separator.test_axis((p_transform_a.xform(segment_A->get_b())-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*0.5)).normalized()))
		return;
	if (!separator.test_axis((p_transform_a.xform(segment_A->get_b())-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*-0.5)).normalized()))
		return;

	separator.generate_contacts();
}

static void _collision_segment_convex_polygon(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const SegmentShape2DSW *segment_A = static_cast<const SegmentShape2DSW*>(p_a);
	const ConvexPolygonShape2DSW *convex_B = static_cast<const ConvexPolygonShape2DSW*>(p_b);

	SeparatorAxisTest2D<SegmentShape2DSW,ConvexPolygonShape2DSW> separator(segment_A,p_transform_a,p_transform_inv_a,convex_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis(p_transform_inv_a.basis_xform_inv(segment_A->get_normal()).normalized()))
		return;

	for(int i=0;i<convex_B->get_point_count();i++) {

		if (!separator.test_axis( p_transform_inv_b.basis_xform_inv(convex_B->get_segment_normal(i)).normalized() ))
			return;
	}

	separator.generate_contacts();

}


/////////

static void _collision_circle_circle(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CircleShape2DSW *circle_A = static_cast<const CircleShape2DSW*>(p_a);
	const CircleShape2DSW *circle_B = static_cast<const CircleShape2DSW*>(p_b);


	SeparatorAxisTest2D<CircleShape2DSW,CircleShape2DSW> separator(circle_A,p_transform_a,p_transform_inv_a,circle_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	if (!separator.test_axis((p_transform_a.get_origin()-p_transform_b.get_origin()).normalized()))
		return;

	separator.generate_contacts();

}

static void _collision_circle_rectangle(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CircleShape2DSW *circle_A = static_cast<const CircleShape2DSW*>(p_a);
	const RectangleShape2DSW *rectangle_B = static_cast<const RectangleShape2DSW*>(p_b);


	SeparatorAxisTest2D<CircleShape2DSW,RectangleShape2DSW> separator(circle_A,p_transform_a,p_transform_inv_a,rectangle_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	const Vector2 &sphere=p_transform_a.elements[2];
	const Vector2 *axis=&p_transform_b.elements[0];
	const Vector2& half_extents = rectangle_B->get_half_extents();

	if (!separator.test_axis(axis[0].normalized()))
		return;

	if (!separator.test_axis(axis[1].normalized()))
		return;

	Vector2 local_v = p_transform_inv_b.xform(p_transform_a.get_origin());

	Vector2 he(
		(local_v.x<0) ? -half_extents.x : half_extents.x,
		(local_v.y<0) ? -half_extents.y : half_extents.y
	);


	if (!separator.test_axis((p_transform_b.xform(he)-sphere).normalized()))
		return;

	separator.generate_contacts();
}

static void _collision_circle_capsule(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CircleShape2DSW *circle_A = static_cast<const CircleShape2DSW*>(p_a);
	const CapsuleShape2DSW *capsule_B = static_cast<const CapsuleShape2DSW*>(p_b);


	SeparatorAxisTest2D<CircleShape2DSW,CapsuleShape2DSW> separator(circle_A,p_transform_a,p_transform_inv_a,capsule_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//capsule axis
	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;

	//capsule endpoints
	if (!separator.test_axis((p_transform_a.get_origin()-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*0.5)).normalized()))
		return;
	if (!separator.test_axis((p_transform_a.get_origin()-(p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*-0.5)).normalized()))
		return;


	separator.generate_contacts();


}

static void _collision_circle_convex_polygon(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CircleShape2DSW *circle_A = static_cast<const CircleShape2DSW*>(p_a);
	const ConvexPolygonShape2DSW *convex_B = static_cast<const ConvexPolygonShape2DSW*>(p_b);


	SeparatorAxisTest2D<CircleShape2DSW,ConvexPolygonShape2DSW> separator(circle_A,p_transform_a,p_transform_inv_a,convex_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//poly faces and poly points vs circle
	for(int i=0;i<convex_B->get_point_count();i++) {

		if (!separator.test_axis( (p_transform_b.xform(convex_B->get_point(i))-p_transform_a.get_origin()).normalized() ))
			return;

		if (!separator.test_axis( p_transform_inv_b.basis_xform_inv(convex_B->get_segment_normal(i)).normalized() ))
			return;
	}

	separator.generate_contacts();
}


/////////

static void _collision_rectangle_rectangle(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const RectangleShape2DSW *rectangle_A = static_cast<const RectangleShape2DSW*>(p_a);
	const RectangleShape2DSW *rectangle_B = static_cast<const RectangleShape2DSW*>(p_b);


	SeparatorAxisTest2D<RectangleShape2DSW,RectangleShape2DSW> separator(rectangle_A,p_transform_a,p_transform_inv_a,rectangle_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//box faces A
	if (!separator.test_axis(p_transform_a.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_a.elements[1].normalized()))
		return;

	//box faces B
	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_b.elements[1].normalized()))
		return;

	separator.generate_contacts();
}

static void _collision_rectangle_capsule(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const RectangleShape2DSW *rectangle_A = static_cast<const RectangleShape2DSW*>(p_a);
	const CapsuleShape2DSW *capsule_B = static_cast<const CapsuleShape2DSW*>(p_b);


	SeparatorAxisTest2D<RectangleShape2DSW,CapsuleShape2DSW> separator(rectangle_A,p_transform_a,p_transform_inv_a,capsule_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//box faces
	if (!separator.test_axis(p_transform_a.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_a.elements[1].normalized()))
		return;

	//capsule axis
	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;


	//box endpoints to capsule circles

	for(int i=0;i<2;i++) {

		Vector2 capsule_endpoint = p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*(i==0?0.5:-0.5);

		const Vector2& half_extents = rectangle_A->get_half_extents();
		Vector2 local_v = p_transform_inv_a.xform(capsule_endpoint);

		Vector2 he(
			(local_v.x<0) ? -half_extents.x : half_extents.x,
			(local_v.y<0) ? -half_extents.y : half_extents.y
		);


		if (!separator.test_axis(p_transform_a.xform(he).normalized()))
			return;

	}


	separator.generate_contacts();
}

static void _collision_rectangle_convex_polygon(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const RectangleShape2DSW *rectangle_A = static_cast<const RectangleShape2DSW*>(p_a);
	const ConvexPolygonShape2DSW *convex_B = static_cast<const ConvexPolygonShape2DSW*>(p_b);

	SeparatorAxisTest2D<RectangleShape2DSW,ConvexPolygonShape2DSW> separator(rectangle_A,p_transform_a,p_transform_inv_a,convex_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//box faces
	if (!separator.test_axis(p_transform_a.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_a.elements[1].normalized()))
		return;

	//convex faces
	for(int i=0;i<convex_B->get_point_count();i++) {

		if (!separator.test_axis( p_transform_inv_b.basis_xform_inv(convex_B->get_segment_normal(i)).normalized() ))
			return;
	}

	separator.generate_contacts();

}


/////////

static void _collision_capsule_capsule(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CapsuleShape2DSW *capsule_A = static_cast<const CapsuleShape2DSW*>(p_a);
	const CapsuleShape2DSW *capsule_B = static_cast<const CapsuleShape2DSW*>(p_b);


	SeparatorAxisTest2D<CapsuleShape2DSW,CapsuleShape2DSW> separator(capsule_A,p_transform_a,p_transform_inv_a,capsule_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//capsule axis

	if (!separator.test_axis(p_transform_b.elements[0].normalized()))
		return;

	if (!separator.test_axis(p_transform_a.elements[0].normalized()))
		return;

	//capsule endpoints

	for(int i=0;i<2;i++) {

		Vector2 capsule_endpoint_A = p_transform_a.get_origin()+p_transform_a.elements[1]*capsule_A->get_height()*(i==0?0.5:-0.5);

		for(int j=0;j<2;j++) {

			Vector2 capsule_endpoint_B = p_transform_b.get_origin()+p_transform_b.elements[1]*capsule_B->get_height()*(j==0?0.5:-0.5);

			if (!separator.test_axis( (capsule_endpoint_A-capsule_endpoint_B).normalized() ))
				return;

		}
	}

	separator.generate_contacts();

}

static void _collision_capsule_convex_polygon(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {

	const CapsuleShape2DSW *capsule_A = static_cast<const CapsuleShape2DSW*>(p_a);
	const ConvexPolygonShape2DSW *convex_B = static_cast<const ConvexPolygonShape2DSW*>(p_b);


	SeparatorAxisTest2D<CapsuleShape2DSW,ConvexPolygonShape2DSW> separator(capsule_A,p_transform_a,p_transform_inv_a,convex_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	//capsule axis

	if (!separator.test_axis(p_transform_a.elements[0].normalized()))
		return;


	//poly vs capsule
	for(int i=0;i<convex_B->get_point_count();i++) {

		Vector2 cpoint = p_transform_b.xform(convex_B->get_point(i));

		for(int j=0;j<2;j++) {

			Vector2 capsule_endpoint_A = p_transform_a.get_origin()+p_transform_a.elements[1]*capsule_A->get_height()*(j==0?0.5:-0.5);

			if (!separator.test_axis( (cpoint - capsule_endpoint_A).normalized() ))
				return;

		}

		if (!separator.test_axis( p_transform_inv_b.basis_xform_inv(convex_B->get_segment_normal(i)).normalized() ))
			return;
	}

	separator.generate_contacts();
}


/////////


static void _collision_convex_polygon_convex_polygon(const Shape2DSW* p_a,const Matrix32& p_transform_a,const Matrix32& p_transform_inv_a,const Shape2DSW* p_b,const Matrix32& p_transform_b,const Matrix32& p_transform_inv_b,_CollectorCallback2D *p_collector) {


	const ConvexPolygonShape2DSW *convex_A = static_cast<const ConvexPolygonShape2DSW*>(p_a);
	const ConvexPolygonShape2DSW *convex_B = static_cast<const ConvexPolygonShape2DSW*>(p_b);

	SeparatorAxisTest2D<ConvexPolygonShape2DSW,ConvexPolygonShape2DSW> separator(convex_A,p_transform_a,p_transform_inv_a,convex_B,p_transform_b,p_transform_inv_b,p_collector);

	if (!separator.test_previous_axis())
		return;

	for(int i=0;i<convex_A->get_point_count();i++) {

		if (!separator.test_axis( p_transform_inv_a.basis_xform_inv(convex_A->get_segment_normal(i)).normalized() ))
			return;
	}

	for(int i=0;i<convex_B->get_point_count();i++) {

		if (!separator.test_axis( p_transform_inv_b.basis_xform_inv(convex_B->get_segment_normal(i)).normalized() ))
			return;
	}

	separator.generate_contacts();

}


////////

bool sat_2d_calculate_penetration(const Shape2DSW *p_shape_A, const Matrix32& p_transform_A, const Matrix32& p_transform_inv_A, const Shape2DSW *p_shape_B, const Matrix32& p_transform_B, const Matrix32& p_transform_inv_B, CollisionSolver2DSW::CallbackResult p_result_callback,void *p_userdata, bool p_swap,Vector2 *sep_axis) {

	Physics2DServer::ShapeType type_A=p_shape_A->get_type();

	ERR_FAIL_COND_V(type_A==Physics2DServer::SHAPE_LINE,false);
	//ERR_FAIL_COND_V(type_A==Physics2DServer::SHAPE_RAY,false);
	ERR_FAIL_COND_V(p_shape_A->is_concave(),false);

	Physics2DServer::ShapeType type_B=p_shape_B->get_type();

	ERR_FAIL_COND_V(type_B==Physics2DServer::SHAPE_LINE,false);
	//ERR_FAIL_COND_V(type_B==Physics2DServer::SHAPE_RAY,false);
	ERR_FAIL_COND_V(p_shape_B->is_concave(),false);


	static const CollisionFunc collision_table[5][5]={
		{_collision_segment_segment,
		 _collision_segment_circle,
		 _collision_segment_rectangle,
		 _collision_segment_capsule,
		 _collision_segment_convex_polygon},
		{0,
		 _collision_circle_circle,
		 _collision_circle_rectangle,
		 _collision_circle_capsule,
		 _collision_circle_convex_polygon},
		{0,
		 0,
		 _collision_rectangle_rectangle,
		 _collision_rectangle_capsule,
		 _collision_rectangle_convex_polygon},
		{0,
		 0,
		 0,
		 _collision_capsule_capsule,
		 _collision_capsule_convex_polygon},
		{0,
		 0,
		 0,
		 0,
		 _collision_convex_polygon_convex_polygon}

	};

	_CollectorCallback2D callback;
	callback.callback=p_result_callback;
	callback.swap=p_swap;
	callback.userdata=p_userdata;
	callback.collided=false;
	callback.sep_axis=sep_axis;

	const Shape2DSW *A=p_shape_A;
	const Shape2DSW *B=p_shape_B;
	const Matrix32 *transform_A=&p_transform_A;
	const Matrix32 *transform_B=&p_transform_B;
	const Matrix32 *transform_inv_A=&p_transform_inv_A;
	const Matrix32 *transform_inv_B=&p_transform_inv_B;

	if (type_A > type_B) {
		SWAP(A,B);
		SWAP(transform_A,transform_B);
		SWAP(transform_inv_A,transform_inv_B);
		SWAP(type_A,type_B);
		callback.swap = !callback.swap;
	}


	CollisionFunc collision_func = collision_table[type_A-2][type_B-2];
	ERR_FAIL_COND_V(!collision_func,false);


	collision_func(A,*transform_A,*transform_inv_A,B,*transform_B,*transform_inv_B,&callback);

	return callback.collided;


}
