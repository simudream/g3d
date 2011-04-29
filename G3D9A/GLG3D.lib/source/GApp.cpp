/**
 \file GApp.cpp
  
 \maintainer Morgan McGuire, http://graphics.cs.williams.edu
 
 \created 2003-11-03
 \edited  2010-09-12
 */

#include "G3D/platform.h"

#include "GLG3D/GApp.h"
#include "G3D/GCamera.h"
#include "G3D/fileutils.h"
#include "G3D/Log.h"
#include "G3D/NetworkDevice.h"
#include "GLG3D/FirstPersonManipulator.h"
#include "GLG3D/UserInput.h"
#include "GLG3D/OSWindow.h"
#include "GLG3D/Shader.h"
#include "GLG3D/Draw.h"
#include "GLG3D/RenderDevice.h"
#ifndef G3D_ANDROID
#   include "GLG3D/VideoRecordDialog.h"
#endif
#include "G3D/ParseError.h"
#include "G3D/FileSystem.h"
#include "GLG3D/GuiTextureBox.h"
#include "GLG3D/GuiPane.h"
#include "G3D/units.h"
#include <time.h>

namespace G3D {

static GApp* lastGApp = NULL;

void screenPrintf(const char* fmt ...) {
    va_list argList;
    va_start(argList, fmt);
    if (lastGApp) {
        lastGApp->vscreenPrintf(fmt, argList);
    }
    va_end(argList);
}

void GApp::vscreenPrintf
(const char*                 fmt,
 va_list                     argPtr) {
    if (showDebugText) {
        std::string s = G3D::vformat(fmt, argPtr);
        m_debugTextMutex.lock();
        debugText.append(s);
        m_debugTextMutex.unlock();
    }
}


DebugID debugDraw
(const Shape::Ref& shape, 
 float             displayTime, 
 const Color4&     solidColor, 
 const Color4&     wireColor, 
 const CFrame&     frame) {

    if (lastGApp) {
        debugAssert(shape.notNull());
        GApp::DebugShape& s = lastGApp->debugShapeArray.next();
        s.shape             = shape;
        s.solidColor        = solidColor;
        s.wireColor         = wireColor;
        s.frame             = frame;
        s.endTime           = System::time() + displayTime;
        s.id                = lastGApp->m_lastDebugID++;
        return s.id;
    } else {
        return 0;
    }
}


/** Attempt to write license file */
static void writeLicense() {
    FILE* f = FileSystem::fopen("g3d-license.txt", "wt");
    if (f != NULL) {
        fprintf(f, "%s", license().c_str());
        FileSystem::fclose(f);
    }
}


GApp::GApp(const Settings& settings, OSWindow* window) :
    m_lastDebugID(0),
#ifndef G3D_ANDROID
    m_activeVideoRecordDialog(NULL),
#endif
    m_settings(settings),
    m_renderPeriod(1),
    m_endProgram(false),
    m_exitCode(0),
    m_debugTextColor(Color3::white()),
    m_debugTextOutlineColor(Color3::black()),
    m_useFilm(settings.film.enabled),
    m_lastFrameOverWait(0),
#ifndef G3D_ANDROID
    debugPane(NULL),
#endif
    renderDevice(NULL),
    userInput(NULL),
    lastWaitTime(System::time()),
    m_desiredFrameRate(5000),
    m_simTimeStep(1.0f / 60.0f),
    m_realTime(0), 
    m_simTime(0) {

    lastGApp = this;

    char b[2048];
    getcwd(b, 2048);
    logLazyPrintf("cwd = %s\n", b);
    
    if (settings.dataDir == "<AUTO>") {
        dataDir = demoFindData(false);
    } else {
        dataDir = settings.dataDir;
    }
    System::setAppDataDir(dataDir);

    if (settings.writeLicenseFile && ! FileSystem::exists("g3d-license.txt")) {
        writeLicense();
    }

    renderDevice = new RenderDevice();

    if (window != NULL) {
        _hasUserCreatedWindow = true;
        renderDevice->init(window);
    } else {
        _hasUserCreatedWindow = false;    
        renderDevice->init(settings.window);
    }
    debugAssertGLOk();

    _window = renderDevice->window();
    _window->makeCurrent();
    debugAssertGLOk();

    m_widgetManager = WidgetManager::create(_window);
    userInput = new UserInput(_window);
    defaultController = FirstPersonManipulator::create(userInput);

    {
        TextOutput t;

        t.writeSymbols("System","{");
        t.pushIndent();
        t.writeNewline();
        System::describeSystem(t);
        if (renderDevice) {
            renderDevice->describeSystem(t);
        }

        NetworkDevice::instance()->describeSystem(t);
        t.writeNewline();
        t.writeSymbol("}");
        t.writeNewline();

        std::string s;
        t.commitString(s);
        logPrintf("%s\n", s.c_str());
    }
    defaultCamera  = GCamera();

    debugAssertGLOk();
    loadFont(settings.debugFontName);
    debugAssertGLOk();

    if (defaultController.notNull()) {
        defaultController->onUserInput(userInput);
        defaultController->setMoveRate(10);
        defaultController->setPosition(Vector3(0, 0, 4));
        defaultController->lookAt(Vector3::zero());
        defaultController->setActive(false);
        defaultCamera.setPosition(defaultController->translation());
        defaultCamera.lookAt(Vector3::zero());
        addWidget(defaultController);
        setCameraManipulator(defaultController);
    }
 
    showDebugText               = true;
    escapeKeyAction             = ACTION_QUIT;
    showRenderingStats          = true;
    fastSwitchCamera            = true;
    catchCommonExceptions       = true;
    manageUserInput             = true;

    {
        GConsole::Settings settings;
        settings.backgroundColor = Color3::green() * 0.1f;
        console = GConsole::create(debugFont, settings, staticConsoleCallback, this);
        console->setActive(false);
        addWidget(console);
    }

    if (m_useFilm) {
        if (! GLCaps::supports_GL_ARB_shading_language_100() || ! GLCaps::supports_GL_ARB_texture_non_power_of_two() ||
            (! GLCaps::supports_GL_ARB_framebuffer_object() && ! GLCaps::supports_GL_EXT_framebuffer_object())) {
            // This GPU can't support the film class
            *const_cast<bool*>(&m_useFilm) = false;
            logPrintf("Warning: Disabled GApp::Settings::film.enabled because it could not be supported on this GPU.");
        } else {
            const ImageFormat* colorFormat = GLCaps::firstSupportedTexture(m_settings.film.preferredColorFormats);

            if (colorFormat == NULL) {
                // This GPU can't support the film class
                *const_cast<bool*>(&m_useFilm) = false;
                logPrintf("Warning: Disabled GApp::Settings::film.enabled because none of the provided color formats could be supported on this GPU.");
            } else {
                m_film = Film::create(colorFormat);
                m_frameBuffer = Framebuffer::create("GApp::m_frameBuffer");

                // The actual buffer allocation code:
                resize(renderDevice->width(), renderDevice->height());
            }
        }
    }

    defaultController->setMouseMode(FirstPersonManipulator::MOUSE_DIRECT_RIGHT_BUTTON);
    defaultController->setActive(true);

#ifndef G3D_ANDROID
    if (settings.useDeveloperTools) {
        UprightSplineManipulator::Ref splineManipulator = UprightSplineManipulator::create(&defaultCamera);
        addWidget(splineManipulator);
        
        GFont::Ref arialFont = GFont::fromFile(System::findDataFile("icon.fnt"));
        GuiTheme::Ref theme = GuiTheme::fromFile(System::findDataFile("osx.gtm"), arialFont);

        debugWindow = GuiWindow::create("Debug Controls", theme, 
            Rect2D::xywh(0, settings.window.height - 150, 150, 150), GuiTheme::TOOL_WINDOW_STYLE, GuiWindow::HIDE_ON_CLOSE);
        debugWindow->setVisible(false);
        debugPane = debugWindow->pane();
        addWidget(debugWindow);

        developerWindow = DeveloperWindow::create
            (this,
             defaultController, 
             splineManipulator,
             Pointer<Manipulator::Ref>(this, &GApp::cameraManipulator, &GApp::setCameraManipulator), 
             m_film,
             theme,
             console,
             Pointer<bool>(debugWindow, &GuiWindow::visible, &GuiWindow::setVisible),
             &showRenderingStats,
             &showDebugText);

        addWidget(developerWindow);
    } else {
        debugPane = NULL;
    }
#endif

    debugAssertGLOk();

    m_simTime     = 0;
    m_realTime    = 0;
    lastWaitTime  = System::time();

    renderDevice->setColorClearValue(Color3(0.1f, 0.5f, 1.0f));
}


#ifndef G3D_ANDROID
GuiWindow::Ref GApp::show(const Texture::Ref& t, const std::string& windowCaption) {
    static const Vector2 offset = Vector2(25, 15);
    static Vector2 lastPos = Vector2(0,0);
    static float y0 = 0;
    
    lastPos += offset;

    std::string name;
    std::string dayTime;

    {
        // Use the current time as the name
        time_t t1;
        ::time(&t1);
        tm* t = localtime(&t1);
        static const char* day[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        int hour = t->tm_hour;
        const char* ap = "am";
        if (hour == 0) {
            hour = 12;
        } else if (hour >= 12) {
            ap = "pm";
            if (hour > 12) {
                hour -= 12;
            }
        }
        dayTime = format("%s %d:%02d:%02d %s", day[t->tm_wday], hour, t->tm_min, t->tm_sec, ap);
    }

    
    if (! windowCaption.empty()) {
        name = windowCaption + " - ";
    }
    name += dayTime;
    
    GuiWindow::Ref display = 
        GuiWindow::create(name, NULL, Rect2D::xywh(lastPos,Vector2(0,0)), GuiTheme::NORMAL_WINDOW_STYLE, GuiWindow::REMOVE_ON_CLOSE);

    GuiTextureBox* box = display->pane()->addTextureBox(t);
    box->setSizeFromInterior(t->vector2Bounds());
    box->zoomTo1();
    display->pack();

    // Cascade, but don't go off the screen
    if ((display->rect().x1() > window()->width()) || (display->rect().y1() > window()->height())) {
        lastPos = offset;
        lastPos.y += y0;
        y0 += offset.y;
        
        display->moveTo(lastPos);
        
        if (display->rect().y1() > window()->height()) {
            y0 = 0;
            lastPos = offset;
            display->moveTo(lastPos);
        }
    }
    
    addWidget(display);
    return display;
}


void GApp::drawMessage(const std::string& message) {
    renderDevice->push2D();
    {
        // White scrim
        renderDevice->setBlendFunc(RenderDevice::BLEND_SRC_ALPHA, RenderDevice::BLEND_ONE_MINUS_SRC_ALPHA);
        Draw::fastRect2D(renderDevice->viewport(), renderDevice, Color4(Color3::white(), 0.8f));

        // Text        
        const GFont::Ref font = debugWindow->theme()->defaultStyle().font;
        const float width = font->bounds(message, 1).x;
        font->draw2D(renderDevice, message, renderDevice->viewport().center(), min(30.0f, renderDevice->viewport().width() / width * 0.80f), 
                     Color3::black(), Color4::clear(), GFont::XALIGN_CENTER, GFont::YALIGN_CENTER);
    }
    renderDevice->pop2D();
    renderDevice->swapBuffers();
}
#endif


void GApp::setExitCode(int code) {
    m_endProgram = true;
    m_exitCode = code;
}


void GApp::loadFont(const std::string& fontName) {
    std::string filename = System::findDataFile(fontName);
    if (FileSystem::exists(filename)) {
        debugFont = GFont::fromFile(filename);
    } else {
        logPrintf(
            "Warning: G3D::GApp could not load font \"%s\".\n"
            "This may be because the G3D::GApp::Settings::dataDir was not\n"
            "properly set in main().\n",
            filename.c_str());

        debugFont = NULL;
    }
}


GApp::~GApp() {
    if (lastGApp == this) {
        lastGApp = NULL;
    }

    NetworkDevice::cleanup();

    debugFont = NULL;
    delete userInput;
    userInput = NULL;

    renderDevice->cleanup();
    delete renderDevice;
    renderDevice = NULL;

    if (_hasUserCreatedWindow) {
        delete _window;
        _window = NULL;
    }

    VertexBuffer::cleanupAllVARAreas();
}


#ifndef G3D_ANDROID
CoordinateFrame GApp::bookmark(const std::string& name, const CoordinateFrame& defaultValue) const {
    return developerWindow->cameraControlWindow->bookmark(name, defaultValue);
}
#endif


int GApp::run() {
    int ret = 0;
    if (catchCommonExceptions) {
        try {
            onRun();
            ret = m_exitCode;
        } catch (const char* e) {
            alwaysAssertM(false, e);
            ret = -1;
        } catch (const GImage::Error& e) {
            alwaysAssertM(false, e.reason + "\n" + e.filename);
            ret = -1;
        } catch (const std::string& s) {
            alwaysAssertM(false, s);
            ret = -1;
        } catch (const TextInput::WrongTokenType& t) {
            alwaysAssertM(false, t.message);
            ret = -1;
        } catch (const TextInput::WrongSymbol& t) {
            alwaysAssertM(false, t.message);
            ret = -1;
        } catch (const VertexAndPixelShader::ArgumentError& e) {
            alwaysAssertM(false, e.message);
            ret = -1;
        } catch (const LightweightConduit::PacketSizeException& e) {
            alwaysAssertM(false, e.message);
            ret = -1;
        } catch (const ParseError& e) {
            alwaysAssertM(false, e.formatFileInfo() + e.message);
            ret = -1;
        }
    } else {
        onRun();
        ret = m_exitCode;
    }

    return ret;
}


void GApp::onRun() {
    if (window()->requiresMainLoop()) {
        
        // The window push/pop will take care of 
        // calling beginRun/oneFrame/endRun for us.
        window()->pushLoopBody(this);

    } else {
        beginRun();

        // Main loop
        do {
            oneFrame();   
        } while (! m_endProgram);

        endRun();
    }
}


void GApp::renderDebugInfo() {
    if (debugFont.notNull() && (showRenderingStats || (showDebugText && (debugText.length() > 0)))) {
        // Capture these values before we render debug output
        int majGL  = renderDevice->stats().majorOpenGLStateChanges;
        int majAll = renderDevice->stats().majorStateChanges;
        int minGL  = renderDevice->stats().minorOpenGLStateChanges;
        int minAll = renderDevice->stats().minorStateChanges;
        int pushCalls = renderDevice->stats().pushStates;

        renderDevice->push2D();
            const static float size = 10;
            if (showRenderingStats) {
                renderDevice->setBlendFunc(RenderDevice::BLEND_SRC_ALPHA, RenderDevice::BLEND_ONE_MINUS_SRC_ALPHA);
                Draw::fastRect2D(Rect2D::xywh(2, 2, renderDevice->width() - 4, size * 5.8 + 2), renderDevice, Color4(0, 0, 0, 0.3f));
            }

            debugFont->begin2DQuads(renderDevice);
            float x = 5;
            Vector2 pos(x, 5);

            if (showRenderingStats) {

                Color3 statColor = Color3::yellow();
                debugFont->begin2DQuads(renderDevice);

                const char* build = 
#               ifdef G3D_DEBUG
                    " (Debug)";
#               else
                    " (Optimized)";
#               endif

                static const std::string description = renderDevice->getCardDescription() + "   " + System::version() + build;
                debugFont->send2DQuads(renderDevice, description, pos, size, Color3::white());
                pos.y += size * 1.5f;
                
                float fps = renderDevice->stats().smoothFrameRate;
                const std::string& s = format(
                    "% 4d fps (% 3d ms)  % 5.1fM tris  GL Calls: %d/%d Maj;  %d/%d Min;  %d push", 
                    iRound(fps),
                    iRound(1000.0f / fps),
                    iRound(renderDevice->stats().smoothTriangles / 1e5) * 0.1f,
                    /*iRound(renderDevice->stats().smoothTriangleRate / 1e4) * 0.01f,*/
                    majGL, majAll, minGL, minAll, pushCalls);
                debugFont->send2DQuads(renderDevice, s, pos, size, statColor);

                pos.x = x;
                pos.y += size * 1.5;

                {
                    int g = iRound(m_graphicsWatch.smoothElapsedTime() / units::milliseconds());
                    int n = iRound(m_networkWatch.smoothElapsedTime() / units::milliseconds());
                    int s = iRound(m_simulationWatch.smoothElapsedTime() / units::milliseconds());
                    int L = iRound(m_logicWatch.smoothElapsedTime() / units::milliseconds());
                    int u = iRound(m_userInputWatch.smoothElapsedTime() / units::milliseconds());
                    int w = iRound(m_waitWatch.smoothElapsedTime() / units::milliseconds());

                    int swapTime = iRound(renderDevice->swapBufferTimer().smoothElapsedTime() / units::milliseconds());

                    const std::string& str = 
                        format("Time:%4d ms Gfx,%4d ms Swap,%4d ms Sim,%4d ms AI,%4d ms Net,%4d ms UI,%4d ms idle", 
                               g, swapTime, s, L, n, u, w);
                    debugFont->send2DQuads(renderDevice, str, pos, size, statColor);
                }

                pos.x = x;
                pos.y += size * 1.5;

                const char* esc = NULL;
                switch (escapeKeyAction) {
                case ACTION_QUIT:
                    esc = "ESC: QUIT      ";
                    break;
                case ACTION_SHOW_CONSOLE:
                    esc = "ESC: CONSOLE   ";
                    break;
                case ACTION_NONE:
                    esc = "               ";
                    break;
                }

                const char* video = 
#ifndef G3D_ANDROID
                    (developerWindow.notNull() && 
                     developerWindow->videoRecordDialog.notNull() &&
                     developerWindow->videoRecordDialog->enabled()) ?
                    "F4: SCREENSHOT  F6: MOVIE     " :
#endif
                    "                              ";

                const char* camera = 
#ifndef G3D_ANDROID
                    (cameraManipulator().notNull() && 
                     defaultController.notNull()) ? 
                    "F2: CAMERA     " :
#endif
                    "               ";
                    
                const char* dev =
#ifndef G3D_ANDROID
                    developerWindow.notNull() ? 
                    "F11: DEV WINDOW":
#endif
                    "               ";

                const std::string& Fstr = format("%s     %s     %s    %s", esc, camera, video, dev);
                debugFont->send2DQuads(renderDevice, Fstr, pos, 8, Color3::white());

                pos.x = x;
                pos.y += size;

            }

            m_debugTextMutex.lock();
            for (int i = 0; i < debugText.length(); ++i) {
                debugFont->send2DQuads(renderDevice, debugText[i], pos, size, m_debugTextColor, m_debugTextOutlineColor);
                pos.y += size * 1.5;
            }
            m_debugTextMutex.unlock();
            debugFont->end2DQuads(renderDevice);
        renderDevice->pop2D();
    }
}


bool GApp::onEvent(const GEvent& event) {
    if (event.type == GEventType::VIDEO_RESIZE) {
        resize(event.resize.w, event.resize.h);
        return true;
    }

    return false;
}


Lighting::Ref GApp::defaultLighting() {
    Lighting::Ref lighting = Lighting::create();

    lighting->lightArray.append(GLight::directional(Vector3(1,2,1), Color3::fromARGB(0xfcf6eb), true));
    lighting->lightArray.append(GLight::directional(Vector3(-1,-0.5f,-1), Color3::fromARGB(0x1e324d), false));

    // Perform our own search first, since we have a better idea of where this directory might be
    // than the general System::findDataFile.  This speeds up loading of the starter app.
    std::string cubePath = "cubemap";
    if (! FileSystem::exists(cubePath)) {
        cubePath = "../data-files/cubemap";
        if (! FileSystem::exists(cubePath)) {
            cubePath = System::findDataFile("cubemap");
        }
    }
    lighting->environmentMapTexture = 
        Texture::fromFile(pathConcat(cubePath, "noonclouds/noonclouds_*.png"), 
                          TextureFormat::RGB8(), Texture::DIM_CUBE_MAP,
                          Texture::Settings::cubeMap(), 
                          Texture::Preprocess::gamma(2.1f));
    lighting->environmentMapConstant = 1.0f;

    return lighting;
}


void GApp::onGraphics3D(RenderDevice* rd, Array<SurfaceRef>& posed3D) {
    alwaysAssertM(false, "Override onGraphics3D");
    //Surface::sortAndRender(rd, defaultCamera, posed3D, m_lighting);
    drawDebugShapes();
}


void GApp::onGraphics2D(RenderDevice* rd, Array<Surface2DRef>& posed2D) {
    Surface2D::sortAndRender(rd, posed2D);
}


void GApp::onGraphics(RenderDevice* rd, Array<SurfaceRef>& posed3D, Array<Surface2DRef>& posed2D) {
    // Clear the entire screen (needed even though we'll render over it because
    // AFR uses clear() to detect that the buffer is not re-used.)
    rd->clear();
    if (m_useFilm) {
        // Clear the frameBuffer
        rd->pushState(m_frameBuffer);
        rd->clear();
        if (m_colorBuffer0->format()->floatingPoint) {
            // Float render targets don't support line smoothing
            rd->setMinLineWidth(1);
        }
        renderDevice->setMinLineWidth(1);
    } else {
        rd->pushState();
    }

    rd->setProjectionAndCameraMatrix(defaultCamera);
    onGraphics3D(rd, posed3D);

    rd->popState();
    if (m_useFilm) {
        // Expose the film
        m_film->exposeAndRender(rd, m_colorBuffer0, 1);
        rd->setMinLineWidth(0);
    }

    rd->push2D();
    {
        onGraphics2D(rd, posed2D);
    }
    rd->pop2D();
}


void GApp::addWidget(const Widget::Ref& module, bool setFocus) {
    m_widgetManager->add(module);
    
    if (setFocus) {
        m_widgetManager->setFocusedWidget(module);
    }
}


void GApp::removeWidget(const Widget::Ref& module) {
    m_widgetManager->remove(module);
}


void GApp::resize(int w, int h) {
    if (m_useFilm &&
        (m_colorBuffer0.isNull() ||
        (m_colorBuffer0->width() != w) ||
        (m_colorBuffer0->height() != h))) {

        m_frameBuffer->clear();

        const ImageFormat* colorFormat = GLCaps::firstSupportedTexture(m_settings.film.preferredColorFormats);
        const ImageFormat* depthFormat = GLCaps::firstSupportedTextureOrRenderBuffer(m_settings.film.preferredDepthFormats);

        m_colorBuffer0 = Texture::createEmpty("GApp::m_colorBuffer0", w, h, 
            colorFormat, Texture::DIM_2D_NPOT, Texture::Settings::video(), 1);

        m_frameBuffer->set(Framebuffer::COLOR0, m_colorBuffer0);

        if (depthFormat) {
            // Prefer creating a texture if we can

            const Framebuffer::AttachmentPoint p = (depthFormat->stencilBits > 0) ? Framebuffer::DEPTH_AND_STENCIL : Framebuffer::DEPTH;
            if (GLCaps::supportsTexture(depthFormat)) {
                m_depthBuffer  = Texture::createEmpty
                    ("GApp::m_depthBuffer", w, h,
                     depthFormat, Texture::DIM_2D_NPOT, Texture::Settings::video(), 1);
                m_frameBuffer->set(p, m_depthBuffer);
            } else {
                m_depthRenderBuffer  = Renderbuffer::createEmpty
                    ("GApp::m_depthRenderBuffer", w, h, depthFormat);
                m_frameBuffer->set(p, m_depthRenderBuffer);
            }
        }
    }
}


void GApp::oneFrame() {
    for (int repeat = 0; repeat < max(1, m_renderPeriod); ++repeat) {
        lastTime = now;
        now = System::time();
        RealTime timeStep = now - lastTime;

        // User input
        m_userInputWatch.tick();
        if (manageUserInput) {
            processGEventQueue();
        }
        debugAssertGLOk();
        onUserInput(userInput);
        m_widgetManager->onUserInput(userInput);
        m_userInputWatch.tock();

        // Network
        m_networkWatch.tick();
        onNetwork();
        m_widgetManager->onNetwork();
        m_networkWatch.tock();

        // Logic
        m_logicWatch.tick();
        {
            onAI();
            m_widgetManager->onAI();
        }
        m_logicWatch.tock();

        // Simulation
        m_simulationWatch.tick();
        {
            RealTime rdt = timeStep;
            SimTime  sdt = m_simTimeStep / m_renderPeriod;
            SimTime  idt = desiredFrameDuration() / m_renderPeriod;

            onBeforeSimulation(rdt, sdt, idt);
            onSimulation(rdt, sdt, idt);
            m_widgetManager->onSimulation(rdt, sdt, idt);
            onAfterSimulation(rdt, sdt, idt);

            if (m_cameraManipulator.notNull()) {
                defaultCamera.setCoordinateFrame(m_cameraManipulator->frame());
            }

            setRealTime(realTime() + rdt);
            setSimTime(simTime() + sdt);
        }
        m_simulationWatch.tock();
    }


    // Pose
    m_posed3D.fastClear();
    m_posed2D.fastClear();
    m_widgetManager->onPose(m_posed3D, m_posed2D);
    onPose(m_posed3D, m_posed2D);

    // Wait 
    // Note: we might end up spending all of our time inside of
    // RenderDevice::beginFrame.  Waiting here isn't double waiting,
    // though, because while we're sleeping the CPU the GPU is working
    // to catch up.

    m_waitWatch.tick();
    {
        RealTime now = System::time();

        // Compute accumulated time
        RealTime cumulativeTime = now - lastWaitTime;

        // Perform wait for actual time needed
        RealTime desiredWaitTime = max(0.0, desiredFrameDuration() - cumulativeTime);
        onWait(max(0.0, desiredWaitTime - m_lastFrameOverWait) * 0.97);

        // Update wait timers
        lastWaitTime = System::time();
        RealTime actualWaitTime = lastWaitTime - now;

        // Learn how much onWait appears to overshoot by and compensate
        double thisOverWait = actualWaitTime - desiredWaitTime;
        if (abs(thisOverWait - m_lastFrameOverWait) / max(abs(m_lastFrameOverWait), abs(thisOverWait)) > 0.4) {
            // Abruptly change our estimate
            m_lastFrameOverWait = thisOverWait;
        } else {
            // Smoothly change our estimate
            m_lastFrameOverWait = lerp(m_lastFrameOverWait, thisOverWait, 0.1);
        }
    }
    m_waitWatch.tock();


    // Graphics
    renderDevice->beginFrame();
    m_graphicsWatch.tick();
    {
        debugAssertGLOk();
        {
            debugAssertGLOk();
            renderDevice->pushState();
            {
                onGraphics(renderDevice, m_posed3D, m_posed2D);
            }
            renderDevice->popState();
            renderDebugInfo();
        }
    }
    m_graphicsWatch.tock();
#ifndef G3D_ANDROID
    if (m_activeVideoRecordDialog) {
        m_activeVideoRecordDialog->maybeRecord(renderDevice);        
    }
#endif
    renderDevice->endFrame();

    // Remove all expired debug shapes
    RealTime now = System::time();
    for (int i = 0; i < debugShapeArray.size(); ++i) {
        if (debugShapeArray[i].endTime <= now) {
            debugShapeArray.fastRemove(i);
            --i;
        }
    }
    debugText.fastClear();

    if (m_endProgram && window()->requiresMainLoop()) {
        window()->popLoopBody();
    }
}


void GApp::drawDebugShapes() {
    renderDevice->setObjectToWorldMatrix(CFrame());
    for (int i = 0; i < debugShapeArray.size(); ++i) {
        const DebugShape& s = debugShapeArray[i];
        s.shape->render(renderDevice, s.frame, s.solidColor, s.wireColor); 
    }
}


void GApp::removeAllDebugShapes() {
    debugShapeArray.fastClear();
}


void GApp::removeDebugShape(DebugID id) {
    for (int i = 0; i < debugShapeArray.size(); ++i) {
        if (debugShapeArray[i].id == id) {
            debugShapeArray.fastRemove(i);
            return;
        }
    }
}


void GApp::onWait(RealTime t) {
    System::sleep(max(0.0, t));
}


void GApp::setSimTimeStep(float s) {
    m_simTimeStep = s;
}


void GApp::setRealTime(RealTime r) {
    m_realTime = r;
}


void GApp::setSimTime(SimTime s) {
    m_simTime = s;
}

void GApp::setDesiredFrameRate(float fps) {
    debugAssert(fps > 0);
    m_desiredFrameRate = fps;
}


void GApp::onInit() {}


void GApp::onCleanup() {}


void GApp::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
    (void)idt;
    (void)rdt;
    (void)sdt;
}


void GApp::onBeforeSimulation(RealTime& rdt, SimTime& sdt, SimTime& idt) {        
    (void)idt;
    (void)rdt;
    (void)sdt;
}

void GApp::onAfterSimulation(RealTime rdt, SimTime sdt, SimTime idt) {        
    (void)idt;
    (void)rdt;
    (void)sdt;
}

void GApp::onPose(Array<Surface::Ref>& posed3D, Array<Surface2D::Ref>& posed2D) {
    (void)posed3D;
    (void)posed2D;
}

void GApp::onNetwork() {}


void GApp::onAI() {}


void GApp::beginRun() {

    m_endProgram = false;
    m_exitCode = 0;

    onInit();

    // Move the controller to the camera's location
    if (defaultController.notNull()) {
        defaultController->setFrame(defaultCamera.coordinateFrame());
    }

    now = System::time() - 0.001;
}


void GApp::endRun() {
    onCleanup();

    Log::common()->section("Files Used");
    Set<std::string>::Iterator end = FileSystem::usedFiles().end();
    Set<std::string>::Iterator f = FileSystem::usedFiles().begin();
    
    while (f != end) {
        Log::common()->println(*f);
        ++f;
    }
    Log::common()->println("");


    if (window()->requiresMainLoop() && m_endProgram) {
        ::exit(m_exitCode);
    }
}


void GApp::staticConsoleCallback(const std::string& command, void* me) {
    ((GApp*)me)->onConsoleCommand(command);
}


void GApp::onConsoleCommand(const std::string& cmd) {
    if (trimWhitespace(cmd) == "exit") {
        setExitCode(0);
        return;
    }
}


void GApp::onUserInput(UserInput* userInput) {
}

void GApp::processGEventQueue() {
    userInput->beginEvents();

    // Event handling
    GEvent event;
    while (window()->pollEvent(event)) {
        
        // For event debugging
        //if (event.type != GEventType::MOUSE_MOTION) {
        //    printf("%s\n", event.toString().c_str());
        //}

        if (WidgetManager::onEvent(event, m_widgetManager)) {
            continue;
        }

        if (onEvent(event)) {
            // Event was consumed
            continue;
        }

        switch(event.type) {
        case GEventType::QUIT:
            setExitCode(0);
            break;

        case GEventType::KEY_DOWN:

            if (console.isNull() || ! console->active()) {
                switch (event.key.keysym.sym) {
                case GKey::ESCAPE:
                    switch (escapeKeyAction) {
                    case ACTION_QUIT:
                        setExitCode(0);
                        break;
                    
                    case ACTION_SHOW_CONSOLE:
                        console->setActive(true);
                        continue;
                        break;

                    case ACTION_NONE:
                        break;
                    }
                    break;

                case GKey::F2:
#ifndef G3D_ANDROID
                    if (fastSwitchCamera && developerWindow.isNull() && defaultController.notNull()) {
                        defaultController->setActive(! defaultController->active());
                        // Consume event
                        continue;
                    }
#endif
                    break;

                // Add other key handlers here
                default:;
                }
            }
        break;

        // Add other event handlers here

        default:;
        }

        userInput->processEvent(event);
    }

    userInput->endEvents();
}

}