#include "ArticulatedModel2.h"
#include "ParseOBJ.h"

static void stripMaterials(ParseOBJ& parseData) {
    ParseMTL::Material::Ref defaultMaterial = ParseMTL::Material::create();

    // Collapse the groups
    for (ParseOBJ::GroupTable git = parseData.groupTable.begin();
        git.isValid();
        ++git) {
        ParseOBJ::Group::Ref& group = git.value();

        // For each mesh
        for (ParseOBJ::MeshTable mit = group->meshTable.begin();
            mit.isValid();
            ++mit) {

            ParseOBJ::Mesh::Ref& mesh = mit.value();
            mesh->material = defaultMaterial;
        }
    }
}


/** Rewrites the group table so that there is a single group per material, 
   and that group contains a single mesh. */
static void mergeGroupsAndMeshesByMaterial(ParseOBJ& parseData) {
    // Construct one mesh per material, and then rebuild the groups at the end
    ParseOBJ::MeshTable newMeshTable;

    // Collapse the groups
    for (ParseOBJ::GroupTable git = parseData.groupTable.begin();
        git.isValid();
        ++git) {
        const ParseOBJ::Group::Ref& group = git.value();

        // For each mesh
        for (ParseOBJ::MeshTable mit = group->meshTable.begin();
            mit.isValid();
            ++mit) {

            const ParseOBJ::Mesh::Ref& srcMesh = mit.value();

            ParseOBJ::Mesh::Ref& dstMesh = newMeshTable.getCreate(srcMesh->material);
            dstMesh->material = srcMesh->material;
            dstMesh.faceArray.append(srcMesh->faceArray;
        }
    }

    // Rebuild the group table from the meshes
    parseData.groupTable.clear();
    for (ParseOBJ::MeshTable mit = newMeshTable.begin();
        mit.isValid();
        ++mit) {

        ParseOBJ::Mesh::Ref& mesh = mit.value();
        ParseOBJ::Group::Ref group = ParseOBJ::Group::create();
        group->name = mesh->material->name;
        parseData.groupTable.set(group->name, group);
    }
}


static Material::Specification toMaterialSpecification(const ParseMTL::Material::Ref& m) {
    Material::Specification s;

    // TODO

    return s;
}


/** Flip texture coordiantes from the OBJ to the G3D convention */
inline static Point2 OBJToG3DTex(const Vector2& t) {
    return Vector2(t.x, 1.0f - t.y);
}

void ArticulatedModel2::loadOBJ(const Specification& specification) {
    // During loading, we make no attempt to optimize the mesh.  We leave that until the
    // Parts have been created.  The vertex arrays are therefore much larger than they
    // need to be.

    ParseOBJ parseData;
    parseData.parse(TextInput(specification.filename));

    Part* part = new Part(specification.filename);

    if (specification.stripMaterials) {
        stripMaterials(parseData);
    }

    if (specification.mergeMeshesByMaterial) {
        mergeGroupsAndMeshesByMaterial(parseData);
    }

    int numSpecifiedNormals = 0;

    // All groups form a single AModel::Part.  Each mesh in each group
    // forms a single AModel::Mesh.
    for (ParseOBJ::GroupTable git = parseData.groupTable.begin();
        git.isValid();
        ++git) {

        const ParseOBJ::Group::Ref& group = git.value();

        // For each mesh
        for (ParseOBJ::MeshTable mit = group->meshTable.begin();
            mit.isValid();
            ++mit) {

            const ParseOBJ::Mesh::Ref& srcMesh = mit.value();

            // Construct the AModel::Mesh for this group+mesh combination
            Mesh* mesh = addMesh(part);
            Array<int>& meshIndexArray = mesh->cpuIndexArray;
            mesh->name = group->name + "/" + srcMesh->material->name;

            if (specification.stripMaterials) {
                // The default material
                mesh->material = Material::create();
            } else { 
                // The specified material.  G3D::Material will cache
                mesh->material = Material::create(toMaterialSpecification(srcMesh->material));
            }

            // For each face
            const Array<ParseOBJ::Face>& faceArray = srcMesh.faceArray;
            int prevNumVertices = vertexArray.size();
            for (int f = 0; f < faceArray.size(); ++f) {
                const Face& face = faceArray[f];

                // For each vertex
                for (int v = 0; v < face.size(); ++v) {
                    const Index& index = face[v];
                    debugAssert(index.vertexArray != ParseOBJ::UNDEFINED);
                    part->cpuVertexArray.append(parseData.vertexArray[index.vertex]);

                    if (index.normalArray != ParseOBJ::UNDEFINED) {
                        part->cpuNormalArray.append(parseData.normalArray[index.normal]);
                        ++numSpecifiedNormals;
                    } else {
                        part->cpuNormalArray.append(Vector3::nan());
                    }

                    if (index.texCoordArray != ParseOBJ::UNDEFINED) {
                        part->cpuTexCoord0Array.append(OBJToG3DTex(parseData.texCoordArray[index.texCoord]));
                    } else {
                        part->cpuTexCoord0Array.append(Point2::zero());
                    }
                }
            }

            // Tessellate the polygon into triangles, writing to both the part index array
            // and the mesh index array.
            for (int t = 2; t < faceArray.size(); ++t) {
                int i = prevNumVertices + t - 2;
                mesh->cpuIndexArray.append(t, t + 1, t + 2);
            }
        }
    }
}
