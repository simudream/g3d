/**
  \file GLG3D/Tri.h

  \maintainer Morgan McGuire, http://graphics.cs.williams.edu

  \created 2008-08-10
  \edited  2012-03-16
*/
#ifndef GLG3D_Tri_h
#define GLG3D_Tri_h

#include "G3D/platform.h"
#include "G3D/HashTrait.h"
#include "G3D/BoundsTrait.h"
#include "G3D/Vector2.h"
#include "G3D/Vector3.h"
#include "G3D/AABox.h"
#include "G3D/Array.h"
#include "G3D/CoordinateFrame.h"
#include "G3D/ReferenceCount.h"
#include "G3D/Triangle.h"
#include "GLG3D/Material.h"


namespace G3D {
class Ray;

/**
 \brief Triangle implementation optimized for ray-triangle intersection.  

 Single sided and immutable once created.
 
 The actual vertex positions have some roundoff error compared to a naive
 implementation because they are stored in a format more efficient for 
 intersection computations.

 The size of this class is carefully controlled so that large scenes can
 be stored efficiently and that cache coherence is maintained during processing.
 The implementation is currently 176 bytes in a 64-bit build.

 \sa G3D::Triangle, G3D::MeshShape, G3D::ArticulatedModel, G3D::Surface, G3D::MeshAlg
 */
class Tri {
private:
    // Intersector is declared below
    friend class Intersector;

    // The size of the Tri class does not appear to significantly impact
    // the performance of ray tracing under the current kd tree implementation.

    /** Vertex 0 */
    Vector3                 v0;

    /** Edge vector v1 - v0 */
    Vector3                 e1;

    /** Edge vector v2 - v0 */
    Vector3                 e2;

    /** If the Tri is in a "smooth" surface then the vertex normals,
        otherwise the face normal. */
    Vector3                 m_normal[3];

	/** Twice the area: (e0 x e1).length() */
    float                  doubleArea;

    /** Texture coordinates. */
    Vector2                 m_texCoord[3];

    /** Per-vertex tangents for bump mapping. */
    Vector4                 m_packedTangent[3];

    /** Usually a material, but can be abstracted */
    Proxy<Material>::Ref    m_material;

public:

    /** Assumes that normals are perpendicular to tangents, or that the tangents are zero.

      \param material Create your own Proxy<Material> subclass to store application-specific data; BSDF, image, etc.
       without adding to the size of Tri or having to trampoline all of the Material factory methods.
       To extract the actual material from the proxy use Tri::material and Tri::data<T>.
    */
    Tri(const Vector3& v0, const Vector3& v1, const Vector3& v2, 
        const Vector3& n0, const Vector3& n1, const Vector3& n2, 
        const Proxy<Material>::Ref& material = NULL,
        const Vector2& t0 = Vector2::zero(), const Vector2& t1 = Vector2::zero(), const Vector2& t2 = Vector2::zero(),
        const Vector4& tan0 = Vector4::zero(), const Vector4& tan1 = Vector4::zero(), const Vector4& tan2 = Vector4::zero());

    Tri(const Vector3& v0, const Vector3& v1, const Vector3& v2);

    Tri() {}

    /** Cast to Triangle */
    operator Triangle() const;

    /** Backfacing version of this triangle.  Normals and tangents are
        negated and the winding order is reversed. */
    Tri otherSide() const;
    
    /** Returns a bounding box */
    void getBounds(AABox& box) const {
        const Vector3& v1 = v0 + e1;
        const Vector3& v2 = v0 + e2;

        box = AABox(v0.min(v1).min(v2), v0.max(v1).max(v2));
    }

    /** Surface area, computed each call. */
    float area() const;

    /** Vertex position (must be computed) */
    Vector3 vertex(int i) const {
        switch (i) {
        case 0: return v0;
        case 1: return v0 + e1;
        case 2: return v0 + e2;
        default:
            debugAssertM(false, "Illegal index");
            return v0;
        }
    }

    /** Face normal.  For degenerate triangles, this is zero.  For all other triangles
    it has unit length and is defined by counter-clockwise winding. Calculate every call*/
    Vector3 normal() const {
        return e1.cross(e2).directionOrZero();
    }

    /** Vertex normal */
    const Vector3& normal(int i) const {
        debugAssert(i >= 0 && i <= 2);
        return m_normal[i];
    }

    const Vector2& texCoord(int i) const {
        debugAssert(i >= 0 && i <= 2);
        return m_texCoord[i];
    }

    const Vector4& packedTangent(int i) const {
        debugAssert(i >= 0 && i <= 2);
        return m_packedTangent[i];
    }

    /** Per-vertex unit tangent, for bump mapping. Tangents are perpendicular to 
        the corresponding vertex normals.*/
    Vector3 tangent(int i) const {
        debugAssert(i >= 0 && i <= 2);
        return m_packedTangent[i].xyz();
    }

    /** Per-vertex unit tangent = normal x tangent, for bump mapping.
        (Erroneously called the "binormal" in some literature) */
    Vector3 tangent2(int i) const {
        debugAssert(i >= 0 && i <= 2);
        return m_normal[i].cross(m_packedTangent[i].xyz()) * m_packedTangent[i].w;
    }
    
    /** \brief Resolve and return the material for this Tri.
      */
    Material::Ref material() const {
        return Proxy<Material>::resolve(m_material);
    }

    /** 
     Extract the data field.  Mostly useful when using a Proxy<Material> that is not a Material itself
     to simplfy the downcast.  Exactly the same as:

    \code
        tri->
    \endcode
    */
    template<class T>
    ReferenceCountedPointer<T> data() const {
        return m_material.downcast<T>();
    }

    /** Returns a (relatively) unique integer for this object */
    uint32 hashCode() const {
        return (uint32)((v0.hashCode() << 20) + (e1.hashCode() << 10) + e2.hashCode());
    }

    bool operator==(const Tri& t) const {
        return 
            (v0 == t.v0) &&
            (e1 == t.e1) &&
            (e2 == t.e2) &&
            (m_normal[0] == t.m_normal[0]) &&
            (m_normal[1] == t.m_normal[1]) &&
            (m_normal[2] == t.m_normal[2]) &&
            (m_texCoord[0] == t.m_texCoord[0]) &&
            (m_texCoord[1] == t.m_texCoord[1]) &&
            (m_texCoord[2] == t.m_texCoord[2]) &&
            (m_material == t.m_material);
    }

    /** \brief Performs intersection testing against Tri.  

        For use as a ray intersection functor (callback) for TriTree
        and KDTree.  

        A typical intersection routine will invoke
        Intersector::operator() many times but retain only the closest
        intersection.  Therefore this class computes only \a tri, \a
        u, and \a v when operator() is invoked.

        To obtain the full result of the intersection computation,
        call one of the getResult() methods.

        You can also create your own intersection loops for use with
        an intersector.

        \sa Ray, SurfaceElement, CollisionDetection
    */
    class Intersector {
    public:

        /** The triangle hit, NULL if no triangle hit. 

            This is an "output".*/
        const Tri*      tri;

        /** Barycentric coordinate of the hit corresponding to
            <code>tri->vertex(1)</code>. 

            This is an "output".*/
        float           u;

        /** Barycentric coordinate of the hit corresponding to
            <code>tri->vertex(2)</code>. 

            This is an "output"*/
        float           v;

        /** Enables alpha testing in operator() when true. This is an
            "input"*/
        bool            alphaTest;

        /** Alpha values in the lambertian channel that are less than
         this are treated as holes if alphaTest is true.  This is an
         "input"*/
        float           alphaThreshold;

        /** Eye direction (ray direction from functor) */
        Vector3         eye;

        Intersector() : tri(NULL), u(0), v(0), alphaTest(true), alphaThreshold(0.5f) {}

        ~Intersector() {}

        /** \brief Computes the two-sided intersection of the ray and
            triangle.
     
          Called repeatedly by KDTree::intersect and
          TriTree::intersect.  

          If an intersection is found that is closer than \a distance,
          updates distance and stores the result in \a this.  Sample
          usage follows.

          An explicit intersection loop for an array of triangles:          
          <pre>
            SurfaceElement s;
            Intersector hit;
            float distance = finf();
            bool continueTracing = true;
            for (int t = 0; t < array.size()) && continueTracing; ++t) {
                hit(ray, array[t], distance, continueTracing);
            }

            if (hit.tri != NULL) {
                SurfaceElement s(hit);
                ... shading ...
            }
          </pre>


          Using TriTree to run the intersection loop:

          <pre>
          float distance = finf();
          Intersector hit;
          tree.intersectRay(ray, hit, distance);  

          if (hit.tri != NULL) {
              SurfaceElement s(hit);
              ... shading ...
          }
          </pre>
          
          
          (This corresponds to an "AnyHit program" in the NVIDIA OptiX
          API.)

		  \param twoSided If true, both sides of triangles are tested for intersections. If a back face is hit,
			the normal will not automatically be flipped

          \return true if there was an intersection between the ray and triangle
          */
        bool operator()(const Ray& ray, const Tri& tri, bool twoSided, float& distance);

        /** Computes information about the intersection from an
            established Intersector.  The normal will have unit
            length; it is the interpolated vertex normal, not the
            face normal.  If the tangent is non-zero, it has unit
            length. It may not be precisely perpendicular to the
            normal.

            To obtain the face normal, triangle, and material, use
            the tri member.
          */
        void getResult
        (Vector3&        location,
         Vector3&        normal,
         Vector2&        texCoord,
         Vector3&        tangent1,
         Vector3&        tangent2) const;

        void getResult
        (Vector3&        location,
         Vector3&        normal,
         Vector2&        texCoord) const;
    };

    /** Extract world-space triangles from the model and append them
        onto triArray.

        \param xform After transforming to world space, transform by this xform
         (e.g., to get to a camera's object space, pass the inverse of the camera's
          object-to-world matrix)
     */
    static void getTris(const ReferenceCountedPointer<class Surface>& model, Array<Tri>& triArray, 
                        const CFrame& xform = CFrame());
};// G3D_END_PACKED_CLASS(4)
} // namespace G3D

// Needed for InlineKDTree and KDTree
template<> struct BoundsTrait<G3D::Tri> {
    static void getBounds(const G3D::Tri& tri, G3D::AABox& out) { 
        tri.getBounds(out);
    }
};

// Needed for KDTree
template <> struct HashTrait<G3D::Tri> {
    static size_t hashCode(const G3D::Tri& tri) { 
        return tri.hashCode();
    }
};

#endif
