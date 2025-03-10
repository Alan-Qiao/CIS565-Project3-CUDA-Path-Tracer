#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "sceneStructs.h"
#include "utilities.h"

/**
 * Handy-dandy hash function that provides seeds for random number generation.
 */
__host__ __device__ inline unsigned int utilhash(unsigned int a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// CHECKITOUT
/**
 * Compute a point at parameter value `t` on ray `r`.
 * Warning: Point may interact with intersectiong object.
 */
__host__ __device__ glm::vec3 getPointOnRay(Ray& r, float t) {
    return r.origin + t * glm::normalize(r.direction);
}

/**
 * Multiplies a mat4 and a vec4 and returns a vec3 clipped from the vec4.
 */
__host__ __device__ glm::vec3 multiplyMV(glm::mat4 m, glm::vec4 v) {
    return glm::vec3(m * v);
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed cube. Untransformed,
 * the cube ranges from -0.5 to 0.5 in each axis and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float boxIntersectionTest(Geom& box, Ray& r, 
    glm::vec3 &normal, bool &outside) {
    Ray q;
    q.origin    =                multiplyMV(box.inverseTransform, glm::vec4(r.origin   , 1.0f));
    q.direction = glm::normalize(multiplyMV(box.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float tmin = -1e38f;
    float tmax = 1e38f;
    glm::vec3 tmin_n;
    glm::vec3 tmax_n;
    for (int xyz = 0; xyz < 3; ++xyz) {
        float qdxyz = q.direction[xyz];
        /*if (glm::abs(qdxyz) > 0.00001f)*/ {
            float t1 = (-0.5f - q.origin[xyz]) / qdxyz;
            float t2 = (+0.5f - q.origin[xyz]) / qdxyz;
            float ta = glm::min(t1, t2);
            float tb = glm::max(t1, t2);
            glm::vec3 n;
            n[xyz] = t2 < t1 ? +1 : -1;
            if (ta > 0 && ta > tmin) {
                tmin = ta;
                tmin_n = n;
            }
            if (tb < tmax) {
                tmax = tb;
                tmax_n = n;
            }
        }
    }

    if (tmax >= tmin && tmax > 0) {
        outside = true;
        if (tmin <= 0) {
            tmin = tmax;
            tmin_n = tmax_n;
            outside = false;
        }
        glm::vec3 intersectionPoint = multiplyMV(box.transform, glm::vec4(getPointOnRay(q, tmin), 1.0f));
        normal = glm::normalize(multiplyMV(box.invTranspose, glm::vec4(tmin_n, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }
    return -1;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed sphere. Untransformed,
 * the sphere always has radius 0.5 and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float sphereIntersectionTest(Geom& sphere, Ray& r,
        glm::vec3 &normal, bool &outside) {
    float radius = .5;

    glm::vec3 ro = multiplyMV(sphere.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(sphere.inverseTransform, glm::vec4(r.direction, 0.0f)));

    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    float vDotDirection = glm::dot(rt.origin, rt.direction);
    float radicand = vDotDirection * vDotDirection - (glm::dot(rt.origin, rt.origin) - powf(radius, 2));
    if (radicand < 0) {
        return -1;
    }

    float squareRoot = sqrt(radicand);
    float firstTerm = -vDotDirection;
    float t1 = firstTerm + squareRoot;
    float t2 = firstTerm - squareRoot;

    float t = 0;
    if (t1 < 0 && t2 < 0) {
        return -1;
    } else if (t1 > 0 && t2 > 0) {
        t = min(t1, t2);
        outside = true;
    } else {
        t = max(t1, t2);
        outside = false;
    }

    glm::vec3 objspaceIntersection = getPointOnRay(rt, t);

    glm::vec3 intersectionPoint = multiplyMV(sphere.transform, glm::vec4(objspaceIntersection, 1.f));
    normal = glm::normalize(multiplyMV(sphere.invTranspose, glm::vec4(objspaceIntersection, 0.f)));
    /*if (!outside) {
        normal = -normal;
    }*/

    return glm::length(r.origin - intersectionPoint);
}

__host__ __device__ float boundingBoxIntersectionTest(glm::vec3 ro, glm::vec3 rdR, AABoundBox& bounds) {
    float tx1 = (bounds.minCoord.x - ro.x) * rdR.x;
    float tx2 = (bounds.maxCoord.x - ro.x) * rdR.x;
    float tmin = min(tx1, tx2);
    float tmax = max(tx1, tx2);
    float ty1 = (bounds.minCoord.y - ro.y) * rdR.y;
    float ty2 = (bounds.maxCoord.y - ro.y) * rdR.y;
    tmin = max(tmin, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));
    float tz1 = (bounds.minCoord.z - ro.z) * rdR.z;
    float tz2 = (bounds.maxCoord.z - ro.z) * rdR.z;
    tmin = max(tmin, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));
    if (tmax >= tmin && tmax > 0) {
        return tmin;
    }
    return -1;
}

__host__ __device__ glm::vec3 barycentric(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    const glm::vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    const float d00 = glm::dot(v0, v0);
    const float d01 = glm::dot(v0, v1);
    const float d11 = glm::dot(v1, v1);
    const float d20 = glm::dot(v2, v0);
    const float d21 = glm::dot(v2, v1);
    const float invDenom = 1.f / (d00 * d11 - d01 * d01);
    const float v = (d11 * d20 - d01 * d21) * invDenom;
    const float w = (d00 * d21 - d01 * d20) * invDenom;
    const float u = 1.0f - v - w;
    return glm::vec3(u, v, w);
}

//__host__ __device__ glm::vec3 barycentric(glm::vec3 p, glm::vec3 t1, glm::vec3 t2, glm::vec3 t3) {
//    glm::vec3 edge1 = t2 - t1;
//    glm::vec3 edge2 = t3 - t2;
//    float S = 1 / glm::length(glm::cross(edge1, edge2));
//
//    edge1 = p - t2;
//    edge2 = p - t3;
//    float S1 = glm::length(glm::cross(edge1, edge2));
//
//    edge1 = p - t1;
//    edge2 = p - t3;
//    float S2 = glm::length(glm::cross(edge1, edge2));
//
//    edge1 = p - t1;
//    edge2 = p - t2;
//    float S3 = glm::length(glm::cross(edge1, edge2));
//
//    return glm::vec3(S1 * S, S2 * S, S3 * S);
//}


__host__ __device__ float meshIntersectionTest(Geom& mesh, BVHNode* BVHNodes, Triangle* Prims,
    Ray& r, glm::vec3& normal, glm::vec2& uv, int& primIdx, float t_min) {
    glm::vec3 ro = multiplyMV(mesh.inverseTransform, glm::vec4(r.origin, 1.f));
    glm::vec3 rd = glm::normalize(multiplyMV(mesh.inverseTransform, glm::vec4(r.direction, 0.f)));
    glm::vec3 rdR = 1.f / rd;

    primIdx = -1;

#if USE_BVH
    int stack[32];
    int stackPtr = 0;

    stack[stackPtr++] = mesh.bvhRootIdx;

    while (stackPtr > 0) {
        BVHNode& node = BVHNodes[stack[--stackPtr]];
        float t = boundingBoxIntersectionTest(ro, rdR, node.bounds);
        if (t > 0 && t < t_min) {
            if (node.isLeaf()) {
                for (int testPrimIdx = node.leftInd; testPrimIdx < node.leftInd + node.primCnt; testPrimIdx++) {
                    Triangle& prim = Prims[testPrimIdx];
                    glm::vec3 baryPoint;
                    if (glm::intersectRayTriangle(ro, rd, prim.v1.pos, prim.v2.pos, prim.v3.pos, baryPoint)) {
                        if (baryPoint.z < t_min) {
                            t_min = baryPoint.z;
                            primIdx = testPrimIdx;
                        }
                    }

                }
            }
            else {
                stack[stackPtr++] = node.leftInd;
                stack[stackPtr++] = node.leftInd + 1;
            }
        }
    }
#else
    for (int testPrimIdx = mesh.primStartIdx; testPrimIdx < mesh.primStartIdx + mesh.primCnt; testPrimIdx++) {
        Triangle& prim = Prims[testPrimIdx];
        glm::vec3 baryPoint;
        if (glm::intersectRayTriangle(ro, rd, prim.v1.pos, prim.v2.pos, prim.v3.pos, baryPoint)) {
            if (baryPoint.z < t_min) {
                t_min = baryPoint.z;
                primIdx = testPrimIdx;
            }
        }
    }
#endif
    if (primIdx == -1) {
        return -1;
    }

    Triangle& prim = Prims[primIdx];
    Vertex& v1 = prim.v1;
    Vertex& v2 = prim.v2;
    Vertex& v3 = prim.v3;

    glm::vec3 objSpaceIntersection = ro + rd * t_min;
    glm::vec3 baryWeights = barycentric(objSpaceIntersection, v1.pos, v2.pos, v3.pos);
    glm::vec3 objSpaceNormal = glm::normalize(baryWeights.x * v1.nor + baryWeights.y * v2.nor + baryWeights.z * v3.nor);
    
    glm::vec3 intersectionPoint = multiplyMV(mesh.transform, glm::vec4(objSpaceIntersection, 1.f));
    normal = glm::normalize(multiplyMV(mesh.invTranspose, glm::vec4(objSpaceNormal, 0.f)));
    uv = baryWeights.x * v1.uv + baryWeights.y * v2.uv + baryWeights.z * v3.uv;

    return glm::length(r.origin - intersectionPoint);
}
