/**
 \file GLG3D/source/ArticulatedModel2.h

 \author Morgan McGuire, http://graphics.cs.williams.edu
 \created 2011-07-19
 \edited  2011-07-22
 
 Copyright 2000-2011, Morgan McGuire.
 All rights reserved.


 TODO:
 - Fix sponza loading
 - Intersect
 - Load other formats: IFS, PLY2, PLY, 3DS
 - Create heightfield
 - Create cornell box
 - Set bump map parallax steps in specification
 - Implement other preprocess instructions
 - Optimize mergeVertices
 - Optimize parse
 - Pack tangents into short4 format?
 */
#include "GLG3D/ArticulatedModel2.h"

namespace G3D {

const ArticulatedModel2::Pose& ArticulatedModel2::defaultPose() {
    static const Pose p;
    return p;
}


ArticulatedModel2::Ref ArticulatedModel2::create(const ArticulatedModel2::Specification& specification) {
    Ref a = new ArticulatedModel2();
    a->load(specification);
    return a;
}


void ArticulatedModel2::forEachPart(PartCallback& callback, const CFrame& parentFrame, Part* part) {
    // Net transformation from part to world space
    const CFrame& net = parentFrame * part->cframe;

    // Process all children
    for (int c = 0; c < part->m_child.size(); ++c) {
        forEachPart(callback, net, part->m_child[c]);
    }

    // Invoke the callback on this part
    callback(Ref(this), part, parentFrame);
}

void ArticulatedModel2::forEachPart(PartCallback& callback) {
    for (int p = 0; p < m_rootArray.size(); ++p) {
        forEachPart(callback, CFrame(), m_rootArray[p]);
    }
}


ArticulatedModel2::Mesh* ArticulatedModel2::addMesh(const std::string& name, Part* part) {
    part->m_meshArray.append(new Mesh(name, ID(m_nextID)));
    ++m_nextID;
    return part->m_meshArray.last();
}


ArticulatedModel2::Part* ArticulatedModel2::addPart(const std::string& name, Part* parent) {
    m_partArray.append(new Part(name, parent, ID(m_nextID)));
    ++m_nextID;
    if (parent == NULL) {
        m_rootArray.append(m_partArray.last());
    }

    return m_partArray.last();
}


ArticulatedModel2::Mesh* ArticulatedModel2::mesh(const ID& id) {
    Mesh** ptr = m_meshTable.getPointer(id);
    if (ptr == NULL) {
        return NULL;
    } else {
        return *ptr;
    }
}


ArticulatedModel2::Part* ArticulatedModel2::part(const ID& id) {
    Part** ptr = m_partTable.getPointer(id);
    if (ptr == NULL) {
        return NULL;
    } else {
        return *ptr;
    }
}


ArticulatedModel2::Mesh* ArticulatedModel2::mesh(const std::string& partName, const std::string& meshName) {
    Part* p = part(partName);
    if (p != NULL) {
        // Exhaustively cycle through all meshes
        for (int m = 0; m < p->m_meshArray.size(); ++m) {
            if (p->m_meshArray[m]->name == meshName) {
                return p->m_meshArray[m];
            }
        }
    }
    return NULL;
}


ArticulatedModel2::Part* ArticulatedModel2::part(const std::string& partName) {
    // Exhaustively cycle through all parts
    for (int p = 0; p < m_partArray.size(); ++p) {
        if (m_partArray[p]->name == partName) {
            return m_partArray[p];
        }
    }
    return NULL;
}



void ArticulatedModel2::load(const Specification& specification) {
    Stopwatch timer;

    if (endsWith(toLower(specification.filename), ".obj")) {
        loadOBJ(specification);
    } else {
        // Error
        throw std::string("Unrecognized file extension on \"") + specification.filename + "\"";
    }
    timer.after("parse file");

    // Perform operations as demanded by the specification
    preprocess(specification.preprocess);
    timer.after("preprocess");

    // Compute missing elements (normals, tangents) of the part geometry, 
    // perform vertex welding, and recompute bounds.
    cleanGeometry(specification.cleanGeometrySettings);
    timer.after("cleanGeometry");
}


bool ArticulatedModel2::intersect
    (const Ray&     R, 
    const CFrame&   cframe, 
    const Pose&     pose, 
    float&          maxDistance, 
    Part*&          part, 
    Mesh*&          mesh, 
    int&            triStartIndex, 
    float&          u, 
    float&          v) const {

    alwaysAssertM(false, "TODO: ArticulatedModel2::intersect");
    return false;
}

} // namespace G3D
