/** \file App.cpp */
#include "App.h"

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();


/** Dumps the geometry and texture coordinates (no materials) to a
    file.  Does not deal with nested parts */
void convertToOBJFile(const std::string& srcFilename) {
    const std::string dstFilename = FilePath::base(srcFilename) + ".obj";

    FILE* file = FileSystem::fopen(dstFilename.c_str(), "wt");

    ArticulatedModel::Ref m = ArticulatedModel::fromFile(srcFilename);
    fprintf(file, "# %s\n\n", m->name.c_str());
    for (int p = 0; p < m->partArray.size(); ++p) {
        const ArticulatedModel::Part& part = m->partArray[p];


        if (part.parent == -1) {
            const CFrame& cframe = part.cframe;
            
            const bool hasTexCoords = part.texCoordArray.size() > 0;

            // Number of vertices
            const int N = part.geometry.vertexArray.size();

            // Construct a legal part name
            std::string name = "";
            for (int i = 0; i < (int)part.name.size(); ++i) {
                const char c = part.name[i];
                if (isDigit(c) || isLetter(c)) {
                    name += c;
                } else {
                    name += "_";
                }
            }

            if (name == "") {
                name = format("UnnamedPart%d", p);
            }

            // Part name
            fprintf(file, "\ng %s \n", name.c_str());

            // Write geometry
            fprintf(file, "\n");
            for (int v = 0; v < N; ++v) {
                const Point3& vertex = cframe.pointToWorldSpace(part.geometry.vertexArray[v]);
                fprintf(file, "v %g %g %g\n", vertex.x, vertex.y, vertex.z);
            }
            
            if (hasTexCoords) {
                fprintf(file, "\n");
                for (int v = 0; v < N; ++v) {
                    const Point2& texCoord = part.texCoordArray[v];
                    fprintf(file, "vt %g %g\n", texCoord.x, texCoord.y);
                }
            }

            fprintf(file, "\n");
            for (int v = 0; v < N; ++v) {
                const Vector3& normal = cframe.vectorToWorldSpace(part.geometry.normalArray[v]);
                fprintf(file, "vn %g %g %g\n", normal.x, normal.y, normal.z);
            }

            // Triangle list
            fprintf(file, "\n");
            for (int t = 0; t < part.triList.size(); ++t) {
                const ArticulatedModel::Part::TriList::Ref& triList = part.triList[t];
                alwaysAssertM(triList->primitive == PrimitiveType::TRIANGLES, "Only triangle lists supported");
                for (int i = 0; i < triList->indexArray.size(); i += 3) {
                    fprintf(file, "f");
                    for (int j = 0; j < 3; ++j) {
                        // Indices are 1-based; negative values reference relative to the 
                        // last vertex
                        const int index = triList->indexArray[i + j] - N;
                        if (hasTexCoords) {
                            fprintf(file, " %d/%d/%d", index, index, index);
                        } else {
                            fprintf(file, " %d//%d", index, index);
                        }
                    }
                    fprintf(file, "\n");
                }
            }
        }
    }
    
    FileSystem::fclose(file);
}


int main(int argc, const char* argv[]) {
    (void)argc; (void)argv;
    GApp::Settings settings(argc, argv);
    
    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.width       = 1280; 
    settings.window.height      = 720;

    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
    renderDevice->setColorClearValue(Color3::white());

    convertToOBJFile("crate.ifs");
    convertToOBJFile("teapot.ifs");
    ::exit(0);
}


void App::onInit() {

    // Turn on the developer HUD
    debugWindow->setVisible(true);
    developerWindow->setVisible(false);
    developerWindow->cameraControlWindow->setVisible(false);
    showRenderingStats = false;
#if 0
    std::string materialPath = System::findDataFile("material");
    std::string crateFile = System::findDataFile("crate.ifs");
    model = ArticulatedModel::fromFile(crateFile);
    Material::Specification mat;
    std::string base = pathConcat(materialPath, "metalcrate/metalcrate-");
    mat.setLambertian(base + "L.png", 0.2f);
    mat.setSpecular(base + "G.png");
    mat.setGlossyExponentShininess(20);
    BumpMap::Settings b;
    b.iterations = 1;
    mat.setBump(base + "B.png", b);
    Material::Ref material = Material::create(mat);

    /*
    // Save material
    {
        BinaryOutput b("material.mat.sl", G3D_LITTLE_ENDIAN);
        SpeedLoadIdentifier sid;
        material->speedSerialize(sid, b);
        b.commit();
    }

    // Load material
    {
        BinaryInput b("material.mat.sl", G3D_LITTLE_ENDIAN);
        SpeedLoadIdentifier sid;
        material = Material::speedCreate(sid, b);
    }*/

    model->partArray[0].triList[0]->material = material;
#endif

#if 0 // sponza
    Stopwatch timer;
    ArticulatedModel::Ref model = ArticulatedModel::fromFile(System::findDataFile("crytek_sponza/sponza.obj"));
    timer.after("Load OBJ");
    // Save Model
    { 
        BinaryOutput b("model.am.sl", G3D_LITTLE_ENDIAN);
        model->speedSerialize(b);
        b.commit();
    }
    timer.after("speedSerialize");

    // Load Model
    {
        BinaryInput b("model.am.sl", G3D_LITTLE_ENDIAN);
        SpeedLoadIdentifier sid;
        model = ArticulatedModel::speedCreate(b);
    }
    timer.after("speedDeserialize");
#endif

    lighting = defaultLighting();
}


bool App::onEvent(const GEvent& e) {
    if (GApp::onEvent(e)) {
        return true;
    }
    // If you need to track individual UI events, manage them here.
    // Return true if you want to prevent other parts of the system
    // from observing this specific event.
    //
    // For example,
    // if ((e.type == GEventType::GUI_ACTION) && (e.gui.control == m_button)) { ... return true;}
    // if ((e.type == GEventType::KEY_DOWN) && (e.key.keysym.sym == GKey::TAB)) { ... return true; }

    return false;
}

void App::onGraphics3D(RenderDevice* rd, Array<Surface::Ref>& surface3D) {
    Draw::axes(CoordinateFrame(Vector3(0, 0, 0)), rd);

    model->pose(surface3D);
    Surface::sortAndRender(rd, defaultCamera, surface3D, lighting);

    // Call to make the GApp show the output of debugDraw
    drawDebugShapes();
}


void App::onGraphics2D(RenderDevice* rd, Array<Surface2D::Ref>& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction
    Surface2D::sortAndRender(rd, posed2D);
}
